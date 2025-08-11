#include <stdint.h>
/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "support.h"
#include "M68k.h"
#include "RegisterAllocator.h"
#include "cache.h"

static inline __attribute__((always_inline)) uint32_t * load_s16_ext32(uint32_t *ptr, uint8_t reg, int16_t s16)
{
    if (s16 & 0x8000)
        *ptr++ = movn_immed_u16(reg, ~s16, 0);
    else
        *ptr++ = movw_immed_u16(reg, s16);

    return ptr;
}

static inline __attribute__((always_inline)) uint32_t * load_reg_from_addr_offset(uint32_t *ptr, uint8_t size, uint8_t base, uint8_t reg, int32_t offset, uint8_t offset_32bit, int sign_ext)
{
    uint8_t reg_d16 = RA_AllocARMRegister(&ptr);

    int free_base = 0;

    if (base == 0xff)
    {
        free_base = 1;
        base = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_reg(base, 31);
    }

    switch (size)
    {
        case 4:
            if (offset == 0)
                *ptr++ = ldr_offset(base, reg, 0);
            else if (offset > -256 && offset < 256)
                *ptr++ = ldur_offset(base, reg, offset & 0x1ff);
            else if (offset > 0 && offset < 16384 && (offset & 3) == 0)
                *ptr++ = ldr_offset(base, reg, offset);
            else {
                if (offset_32bit) {
                    if ((offset & 0xffff) != 0) {
                        *ptr++ = movw_immed_u16(reg_d16, offset);
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
                        }
                    } else {
                        if ((offset >> 16) & 0xffff)
                        {
                            *ptr++ = mov_immed_u16(reg_d16, (offset >> 16) & 0xffff, 1);
                        }
                        else
                        {
                            *ptr++ = mov_reg(reg_d16, 31);
                        }
                    }
                }
                else {
                    if (offset > 0)
                        *ptr++ = mov_immed_u16(reg_d16, offset, 0);
                    else
                        *ptr++ = movn_immed_u16(reg_d16, -offset - 1, 0);
                }
                *ptr++ = add_reg(reg_d16, base, reg_d16, LSL, 0);
                *ptr++ = ldr_offset(reg_d16, reg, 0);
            }
            break;
        case 2:
            if (offset > -256 && offset < 256)
                if (sign_ext)
                    *ptr++ = ldursh_offset(base, reg, offset & 0x1ff);
                else
                    *ptr++ = ldurh_offset(base, reg, offset & 0x1ff);
            else if (offset >= 0 && offset < 8192 && (offset & 1) == 0)
                if (sign_ext)
                    *ptr++ = ldrsh_offset(base, reg, offset);
                else
                    *ptr++ = ldrh_offset(base, reg, offset);
            else {
                if (offset_32bit) {
                    if (offset & 0xffff) {
                        *ptr++ = movw_immed_u16(reg_d16, offset);
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
                        }
                    } else {
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = mov_immed_u16(reg_d16, (offset >> 16) & 0xffff, 1);
                        } else {
                            *ptr++ = mov_reg(reg_d16, 31);
                        }
                    }
                } else {
                    if (offset > 0)
                        *ptr++ = mov_immed_u16(reg_d16, offset, 0);
                    else
                        *ptr++ = movn_immed_u16(reg_d16, -offset - 1, 0);
                }
                *ptr++ = add_reg(reg_d16, base, reg_d16, LSL, 0);
                if (sign_ext)
                    *ptr++ = ldrsh_offset(reg_d16, reg, 0);
                else
                    *ptr++ = ldrh_offset(reg_d16, reg, 0);
            }
            break;
        case 1:
            if (offset > -256 && offset < 256)
                if (sign_ext)
                    *ptr++ = ldursb_offset(base, reg, offset);
                else
                    *ptr++ = ldurb_offset(base, reg, offset);
            else if (offset >= 0 && offset < 4096)
                if (sign_ext)
                    *ptr++ = ldrsb_offset(base, reg, offset);
                else
                    *ptr++ = ldrb_offset(base, reg, offset);
            else {
                if (offset_32bit) {
                    if (offset & 0xffff) {
                        *ptr++ = movw_immed_u16(reg_d16, offset);
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
                        }
                    } else {
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = mov_immed_u16(reg_d16, (offset >> 16) & 0xffff, 1);
                        } else {
                            *ptr++ = mov_reg(reg_d16, 31);
                        }
                    }
                } else {
                    if (offset > 0)
                        *ptr++ = mov_immed_u16(reg_d16, offset, 0);
                    else
                        *ptr++ = movn_immed_u16(reg_d16, -offset - 1, 0);
                }
                *ptr++ = add_reg(reg_d16, base, reg_d16, LSL, 0);
                if (sign_ext)
                    *ptr++ = ldrsb_offset(reg_d16, reg, 0);
                else
                    *ptr++ = ldrb_offset(reg_d16, reg, 0);
            }
            break;
        case 0:
            if (offset > -4096 && offset < 4096)
            {
                if (offset < 0)
                    *ptr++ = sub_immed(reg, base, -offset);
                else
                    *ptr++ = add_immed(reg, base, offset);
            }
            else
            {
                if (offset_32bit) {
                    if (offset & 0xffff) {
                        *ptr++ = movw_immed_u16(reg_d16, offset);
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
                        }
                    } else {
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = mov_immed_u16(reg_d16, (offset >> 16) & 0xffff, 1);
                        } else {
                            *ptr++ = mov_reg(reg_d16, 31);
                        }
                    }
                } else {
                    if (offset > 0)
                        *ptr++ = mov_immed_u16(reg_d16, offset, 0);
                    else
                        *ptr++ = movn_immed_u16(reg_d16, -offset - 1, 0);
                }
                *ptr++ = add_reg(reg, base, reg_d16, LSL, 0);
            }
            break;
        default:
            kprintf("Unknown size opcode\n");
            break;
    }

    RA_FreeARMRegister(&ptr, reg_d16);

    if (free_base)
        RA_FreeARMRegister(&ptr, base);

    return ptr;
}

static inline __attribute__((always_inline)) uint32_t * load_reg_from_addr(uint32_t *ptr, uint8_t size, uint8_t base, uint8_t reg, uint8_t index, uint8_t shift, int sign_ext)
{
    int free_base = 0;

    if (base == 0xff)
    {
        free_base = 1;
        base = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_reg(base, 31);
    }

    if (index == 0xff)
    {
        switch (size)
        {
            case 4:
                *ptr++ = ldr_offset(base, reg, 0);
                break;
            case 2:
                if (sign_ext)
                    *ptr++ = ldrsh_offset(base, reg, 0);
                else
                    *ptr++ = ldrh_offset(base, reg, 0);
                break;
            case 1:
                if (sign_ext)
                    *ptr++ = ldrsb_offset(base, reg, 0);
                else
                    *ptr++ = ldrb_offset(base, reg, 0);
                break;
            case 0:
                *ptr++ = mov_reg(reg, base);
                break;
            default:
                kprintf("Unknown size opcode\n");
                break;
        }
    }
    else
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        switch (size)
        {
            case 4:
                *ptr++ = add_reg(tmp, base, index, LSL, shift);
                *ptr++ = ldr_offset(tmp, reg, 0);
                break;
            case 2:
                *ptr++ = add_reg(tmp, base, index, LSL, shift);
                if (sign_ext)
                    *ptr++ = ldrsh_offset(tmp, reg, 0);
                else
                    *ptr++ = ldrh_offset(tmp, reg, 0);
                break;
            case 1:
                *ptr++ = add_reg(tmp, base, index, LSL, shift);
                if (sign_ext)
                    *ptr++ = ldrsb_offset(tmp, reg, 0);
                else
                    *ptr++ = ldrb_offset(tmp, reg, 0);
                break;
            case 0:
                *ptr++ = add_reg(reg, base, index, LSL, shift);
                break;
            default:
                kprintf("Unknown size opcode\n");
                break;
        }
        RA_FreeARMRegister(&ptr, tmp);
    }

    if (free_base)
        RA_FreeARMRegister(&ptr, base);

    return ptr;
}

static inline __attribute__((always_inline)) uint32_t * store_reg_to_addr_offset(uint32_t *ptr, uint8_t size, uint8_t base, uint8_t reg, int32_t offset, uint8_t offset_32bit)
{
    uint8_t reg_d16 = RA_AllocARMRegister(&ptr);
    int free_base = 0;

    if (base == 0xff)
    {
        free_base = 1;
        base = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_reg(base, 31);
    }

    switch (size)
    {
        case 4:
            if (offset == 0)
                *ptr++ = str_offset(base, reg, 0);
            else if (offset > -256 && offset < 256)
                *ptr++ = stur_offset(base, reg, offset & 0x1ff);
            else if (offset > 0 && offset < 16384 && (offset & 3) == 0)
                *ptr++ = str_offset(base, reg, offset);
            else {
                if (offset_32bit) {
                    if (offset & 0xffff) {
                        *ptr++ = movw_immed_u16(reg_d16, offset);
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
                        }
                    } else {
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = mov_immed_u16(reg_d16, (offset >> 16) & 0xffff, 1);
                        } else {
                            *ptr++ = mov_reg(reg_d16, 31);
                        }
                    }
                }
                else {
                    if (offset > 0)
                        *ptr++ = mov_immed_u16(reg_d16, offset, 0);
                    else
                        *ptr++ = movn_immed_u16(reg_d16, -offset - 1, 0);
                }
                *ptr++ = add_reg(reg_d16, base, reg_d16, LSL, 0);
                *ptr++ = str_offset(reg_d16, reg, 0);
            }
            break;
        case 2:
            if (offset > -256 && offset < 256)
                *ptr++ = sturh_offset(base, reg, offset & 0x1ff);
            else if (offset >= 0 && offset < 8192 && (offset & 1) == 0)
                *ptr++ = strh_offset(base, reg, offset);
            else {
                if (offset_32bit) {
                    if (offset & 0xffff) {
                        *ptr++ = movw_immed_u16(reg_d16, offset);
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
                        }
                    } else {
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = mov_immed_u16(reg_d16, (offset >> 16) & 0xffff, 1);
                        } else {
                            *ptr++ = mov_reg(reg_d16, 31);
                        }
                    }
                } else {
                    if (offset > 0)
                        *ptr++ = mov_immed_u16(reg_d16, offset, 0);
                    else
                        *ptr++ = movn_immed_u16(reg_d16, -offset - 1, 0);
                }
                *ptr++ = add_reg(reg_d16, base, reg_d16, LSL, 0);
                *ptr++ = strh_offset(reg_d16, reg, 0);
            }
            break;
        case 1:
            if (offset > -256 && offset < 256)
                *ptr++ = sturb_offset(base, reg, offset);
            else if (offset >= 0 && offset < 4096)
                *ptr++ = strb_offset(base, reg, offset);
            else {
                if (offset_32bit) {
                    if (offset & 0xffff) {
                        *ptr++ = movw_immed_u16(reg_d16, offset);
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
                        }
                    } else {
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = mov_immed_u16(reg_d16, (offset >> 16) & 0xffff, 1);
                        } else {
                            *ptr++ = mov_reg(reg_d16, 31);
                    }
                }
                } else {
                    if (offset > 0)
                        *ptr++ = mov_immed_u16(reg_d16, offset, 0);
                    else
                        *ptr++ = movn_immed_u16(reg_d16, -offset - 1, 0);
                }
                *ptr++ = add_reg(reg_d16, base, reg_d16, LSL, 0);
                *ptr++ = strb_offset(reg_d16, reg, 0);
            }
            break;
        case 0:
            if (offset > -4096 && offset < 4096)
            {
                if (offset < 0)
                    *ptr++ = sub_immed(reg, base, -offset);
                else
                    *ptr++ = add_immed(reg, base, offset);
            }
            else
            {
                if (offset_32bit) {
                    if (offset & 0xffff) {
                        *ptr++ = movw_immed_u16(reg_d16, offset);
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
                        }
                    } else {
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = mov_immed_u16(reg_d16, (offset >> 16) & 0xffff, 1);
                        } else {
                            *ptr++ = mov_reg(reg_d16, 31);
                        }
                    }
                } else {
                    if (offset > 0)
                        *ptr++ = mov_immed_u16(reg_d16, offset, 0);
                    else
                        *ptr++ = movn_immed_u16(reg_d16, -offset - 1, 0);
                }
                *ptr++ = add_reg(reg, base, reg_d16, LSL, 0);
            }
            break;
        default:
            kprintf("Unknown size opcode\n");
            break;
    }
    
    RA_FreeARMRegister(&ptr, reg_d16);

    if (free_base)
        RA_FreeARMRegister(&ptr, base);

    return ptr;
}

static inline __attribute__((always_inline)) uint32_t * store_reg_to_addr(uint32_t *ptr, uint8_t size, uint8_t base, uint8_t reg, uint8_t index, uint8_t shift)
{
    int free_base = 0;

    if (base == 0xff)
    {
        free_base = 1;
        base = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_reg(base, 31);
    }

    if (index == 0xff)
    {
        switch (size)
        {
            case 4:
                *ptr++ = str_offset(base, reg, 0);
                break;
            case 2:
                *ptr++ = strh_offset(base, reg, 0);
                break;
            case 1:
                *ptr++ = strb_offset(base, reg, 0);
                break;
            case 0:
                *ptr++ = mov_reg(reg, base);
                break;
            default:
                kprintf("Unknown size opcode\n");
                break;
        }
    }
    else
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        switch (size)
        {
            case 4:
                *ptr++ = add_reg(tmp, base, index, LSL, shift);
                *ptr++ = str_offset(tmp, reg, 0);
                break;
            case 2:
                *ptr++ = add_reg(tmp, base, index, LSL, shift);
                *ptr++ = strh_offset(tmp, reg, 0);
                break;
            case 1:
                *ptr++ = add_reg(tmp, base, index, LSL, shift);
                *ptr++ = strb_offset(tmp, reg, 0);
                break;
            case 0:
                *ptr++ = add_reg(reg, base, index, LSL, shift);
                break;
            default:
                kprintf("Unknown size opcode\n");
                break;
        }
        RA_FreeARMRegister(&ptr, tmp);
    }

    if (free_base)
        RA_FreeARMRegister(&ptr, base);

    return ptr;
}

#define M68K_EA_DA 0x8000
#define M68K_EA_REG 0x7000
#define M68K_EA_WL 0x0800
#define M68K_EA_SCALE 0x0600
#define M68K_EA_FULL 0x0100
#define M68K_EA_OFF8 0x00FF

#define M68K_EA_BS 0x0080
#define M68K_EA_IS 0x0040
#define M68K_EA_BD_SIZE 0x0030
#define M68K_EA_IIS 0x0007

/*
    Emits ARM insns to load effective address and read value from ther to specified register.

    Inputs:
        ptr     pointer to ARM instruction stream
        size    size of data for load operation, can be 4 (long), 2 (short) or 1 (byte).
                If size of 0 is specified the function does not load a value from EA into
                register but rather loads the EA into that register.
                If postincrement or predecrement modes are used and size 0 is specified, then
                the instruction translator is reponsible for increasing/decreasing the address
                register, otherwise it is done in this function!
                If highest bit of size is set, the instruction sign-extends the load to full
                register size
        arm_reg ARM register to store the EA or value from EA into
        ea      EA encoded field.
        m68k_ptr pointer to m68k instruction stream past the instruction opcode itself. It may
                be increased if further bytes from m68k side are read

    Output:
        ptr     pointer to ARM instruction stream after the newly generated code
*/
uint32_t *EMIT_LoadFromEffectiveAddress(uint32_t *ptr, uint8_t size, uint8_t *arm_reg, uint8_t ea, uint16_t *m68k_ptr, uint8_t *ext_words, uint8_t read_only, int32_t *imm_offset)
{
    uint8_t sign_ext = 0;
    uint8_t mode = ea >> 3;
    uint8_t src_reg = ea & 7;

    if (size & 0x80)
    {
        sign_ext = 1;
        size &= 0x7f;
    }

    if (imm_offset)
        *imm_offset = 0;

    if (mode == 0) /* Mode 000: Dn */
    {
        switch (size)
        {
            case 4:
                if (read_only)
                    *arm_reg = RA_MapM68kRegister(&ptr, src_reg);
                else
                    *arm_reg = RA_CopyFromM68kRegister(&ptr, src_reg);
                break;
            case 2:
                if (sign_ext) {
                    if (*arm_reg == 0xff)
                        *arm_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = sxth(*arm_reg, RA_MapM68kRegister(&ptr, src_reg));
                }
                else {
                    if (read_only)
                        *arm_reg = RA_MapM68kRegister(&ptr, src_reg);
                    else
                        *arm_reg = RA_CopyFromM68kRegister(&ptr, src_reg);
                }
                break;
            case 1:
                if (sign_ext) {
                    if (*arm_reg == 0xff)
                        *arm_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = sxtb(*arm_reg, RA_MapM68kRegister(&ptr, src_reg));
                }
                else {
                    if (read_only)
                        *arm_reg = RA_MapM68kRegister(&ptr, src_reg);
                    else
                        *arm_reg = RA_CopyFromM68kRegister(&ptr, src_reg);
                }
                break;
            case 0:
                kprintf("Load form EA: Dn with wrong operand size! Opcode %04x at %08x\n", cache_read_16(ICACHE, (uint32_t)(uintptr_t)&m68k_ptr[-*ext_words]), m68k_ptr - *ext_words);
                break;
            default:
                kprintf("Wrong size\n");
                break;
        }
    }
    else if (mode == 1) /* Mode 001: An */
    {
        switch (size)
        {
            case 4:
                if (read_only)
                    *arm_reg = RA_MapM68kRegister(&ptr, src_reg + 8);
                else
                    *arm_reg = RA_CopyFromM68kRegister(&ptr, src_reg + 8);
                break;
            case 2:
                if (sign_ext) {
                    if (*arm_reg == 0xff)
                        *arm_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = sxth(*arm_reg, RA_MapM68kRegister(&ptr, src_reg + 8));
                }
                else {
                    if (read_only)
                        *arm_reg = RA_MapM68kRegister(&ptr, src_reg + 8);
                    else
                        *arm_reg = RA_CopyFromM68kRegister(&ptr, src_reg + 8);
                }
                break;
            case 0:
                kprintf("Load form EA: An with wrong operand size! Opcode %04x at %08x\n", cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[-*ext_words]), m68k_ptr - *ext_words);
                {
                    uint16_t *ptr = &m68k_ptr[-*ext_words] - 8;
                    for (int i=0; i < 16; i++)
                        kprintf("%04x ", ptr[i]);
                    kprintf("\n");
                }
                break;
            default:
                kprintf("Wrong size\n");
                break;
        }
    }
    else
    {
        if (*arm_reg == 0xff)
            *arm_reg = RA_AllocARMRegister(&ptr);

        if (mode == 2) /* Mode 002: (An) */
        {
            if (size == 0) {
                if (read_only) {
                    RA_FreeARMRegister(&ptr, *arm_reg);
                    *arm_reg = RA_MapM68kRegister(&ptr, src_reg + 8);
                } else {
                    uint8_t tmp = RA_MapM68kRegister(&ptr, src_reg + 8);
                    *ptr++ = mov_reg(*arm_reg, tmp);
                }
            }
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                switch (size)
                {
                    case 4:
                        *ptr++ = ldr_offset(reg_An, *arm_reg, 0);
                        break;
                    case 2:
                        if (sign_ext)
                            *ptr++ = ldrsh_offset(reg_An, *arm_reg, 0);
                        else
                            *ptr++ = ldrh_offset(reg_An, *arm_reg, 0);
                        break;
                    case 1:
                        if (sign_ext)
                            *ptr++ = ldrsb_offset(reg_An, *arm_reg, 0);
                        else
                            *ptr++ = ldrb_offset(reg_An, *arm_reg, 0);
                        break;
                    default:
                        kprintf("Unknown size opcode\n");
                        break;
                }
            }
        }
        else if (mode == 3) /* Mode 003: (An)+ */
        {
            if (size == 0) {
                RA_FreeARMRegister(&ptr, *arm_reg);
                *arm_reg = RA_MapM68kRegister(&ptr, src_reg + 8);
            }
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);

                RA_SetDirtyM68kRegister(&ptr, 8 + src_reg);

                /* Rare case where source and dest are the same register and size == 4 */
                if (size == 4 && reg_An == *arm_reg) {
                    *ptr++ = ldr_offset(reg_An, *arm_reg, 0);
                }
                else
                {
                    switch (size)
                    {
                        case 4:
                            *ptr++ = ldr_offset_postindex(reg_An, *arm_reg, 4);
                            break;
                        case 2:
                            if (sign_ext)
                                *ptr++ = ldrsh_offset_postindex(reg_An, *arm_reg, 2);
                            else
                                *ptr++ = ldrh_offset_postindex(reg_An, *arm_reg, 2);
                            break;
                        case 1:
                            if (src_reg == 7)
                                if (sign_ext)
                                    *ptr++ = ldrsb_offset_postindex(reg_An, *arm_reg, 2);
                                else
                                    *ptr++ = ldrb_offset_postindex(reg_An, *arm_reg, 2);
                            else
                                if (sign_ext)
                                    *ptr++ = ldrsb_offset_postindex(reg_An, *arm_reg, 1);
                                else
                                    *ptr++ = ldrb_offset_postindex(reg_An, *arm_reg, 1);
                            break;
                        default:
                            kprintf("Unknown size opcode\n");
                            break;
                    }
                }
            }
        }
        else if (mode == 4) /* Mode 004: -(An) */
        {
            if (size == 0) {
                RA_FreeARMRegister(&ptr, *arm_reg);
                *arm_reg = RA_MapM68kRegister(&ptr, src_reg + 8);
            }
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                RA_SetDirtyM68kRegister(&ptr, 8 + src_reg);

                /* Rare case where source and dest are the same register and size == 4 */
                if (size == 4 && reg_An == *arm_reg) {
                    *ptr++ = ldur_offset(reg_An, *arm_reg, -4);
                }
                else
                {
                    switch (size)
                    {
                        case 4:
                            *ptr++ = ldr_offset_preindex(reg_An, *arm_reg, -4);
                            break;
                        case 2:
                            if (sign_ext)
                                *ptr++ = ldrsh_offset_preindex(reg_An, *arm_reg, -2);
                            else
                                *ptr++ = ldrh_offset_preindex(reg_An, *arm_reg, -2);
                            break;
                        case 1:
                            if (src_reg == 7)
                                if (sign_ext)
                                    *ptr++ = ldrsb_offset_preindex(reg_An, *arm_reg, -2);
                                else
                                    *ptr++ = ldrb_offset_preindex(reg_An, *arm_reg, -2);
                            else
                                if (sign_ext)
                                    *ptr++ = ldrsb_offset_preindex(reg_An, *arm_reg, -1);
                                else
                                    *ptr++ = ldrb_offset_preindex(reg_An, *arm_reg, -1);
                            break;
                        default:
                            kprintf("Unknown size opcode\n");
                            break;
                    }
                }
            }
        }
        else if (mode == 5) /* Mode 005: (d16, An) */
        {
            if (imm_offset && size == 0 && read_only)
            {
                RA_FreeARMRegister(&ptr, *arm_reg);
                *arm_reg = RA_MapM68kRegister(&ptr, src_reg + 8);
                *imm_offset = (int16_t)cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
            }
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                int16_t off16 = (int16_t)cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);

                ptr = load_reg_from_addr_offset(ptr, size, reg_An, *arm_reg, off16, 0, sign_ext);
            }
        }
        else if (mode == 6) /* Mode 006: (d8, An, Xn.SIZE*SCALE) */
        {
            uint16_t brief = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
            uint8_t extra_reg = (brief >> 12) & 7;

            if ((brief & 0x0100) == 0)
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                uint8_t tmp1 = 0xff;
                uint8_t tmp2 = 0xff;
                int8_t displ = brief & 0xff;

                if (displ > 0)
                {
                    tmp1 = RA_AllocARMRegister(&ptr);
                    *ptr++ = add_immed(tmp1, reg_An, displ);
                }
                else if (displ < 0)
                {
                    tmp1 = RA_AllocARMRegister(&ptr);
                    *ptr++ = sub_immed(tmp1, reg_An, -displ);
                }
                else
                {
                    //*ptr++ = mov_reg(tmp1, reg_An);
                }

                if (brief & (1 << 11))
                {
                    if (brief & 0x8000)
                        tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg); // RA_CopyFromM68kRegister(&ptr, 8 + extra_reg);
                    else
                        tmp2 = RA_MapM68kRegister(&ptr, extra_reg); // RA_CopyFromM68kRegister(&ptr, extra_reg);
                }
                else
                {
                    if (brief & 0x8000)
                        tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                    else
                        tmp2 = RA_MapM68kRegister(&ptr, extra_reg);
                    
                    uint8_t tmp3 = RA_AllocARMRegister(&ptr);
                    *ptr++ = sxth(tmp3, tmp2);
                    RA_FreeARMRegister(&ptr, tmp2);
                    tmp2 = tmp3;
                }

                ptr = load_reg_from_addr(ptr, size, displ ? tmp1 : reg_An, *arm_reg, tmp2, (brief >> 9) & 3, sign_ext);

                if (displ)
                    RA_FreeARMRegister(&ptr, tmp1);

                RA_FreeARMRegister(&ptr, tmp2);
            }
            else
            {
                uint8_t bd_reg = 0xff;
                uint8_t outer_reg = 0xff;
                uint8_t base_reg = 0xff;
                uint8_t index_reg = 0xff;

                /* Check if base register is suppressed */
                if (!(brief & M68K_EA_BS))
                {
                    /* Base register in use. Alloc it and load its contents */
                    base_reg = RA_MapM68kRegister(&ptr, 8 + src_reg);
                }

                /* Check if index register is in use */
                if (!(brief & M68K_EA_IS))
                {
                    /* Index register in use. Alloc it and load its contents */
                    if (brief & (1 << 11))
                    {
                        if (brief & 0x8000)
                            index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg); // RA_CopyFromM68kRegister(&ptr, 8 + extra_reg);
                        else
                            index_reg = RA_MapM68kRegister(&ptr, extra_reg); // RA_CopyFromM68kRegister(&ptr, extra_reg);
                    }
                    else
                    {
                        if (brief & 0x8000)
                            index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                        else
                            index_reg = RA_MapM68kRegister(&ptr, extra_reg);

                        uint8_t tmp3 = RA_AllocARMRegister(&ptr);
                        *ptr++ = sxth(tmp3, index_reg);
                        RA_FreeARMRegister(&ptr, index_reg);
                        index_reg = tmp3;
                    }
                }

                uint16_t lo16, hi16;

                /* Check if base displacement needs to be fetched */
                switch ((brief & M68K_EA_BD_SIZE) >> 4)
                {
                    case 2: /* Word displacement */
                        bd_reg = RA_AllocARMRegister(&ptr);
                        lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                        ptr = load_s16_ext32(ptr, bd_reg, lo16);
                        break;
                    case 3: /* Long displacement */
                        bd_reg = RA_AllocARMRegister(&ptr);
                        hi16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                        lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                        *ptr++ = movw_immed_u16(bd_reg, lo16);
                        if (hi16 != 0)
                            *ptr++ = movt_immed_u16(bd_reg, hi16);
                        break;
                }

                /* Check if outer displacement needs to be fetched */
                switch ((brief & M68K_EA_IIS) & 3)
                {
                    case 2: /* Word outer displacement */
                        outer_reg = RA_AllocARMRegister(&ptr);
                        lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                        ptr = load_s16_ext32(ptr, outer_reg, lo16);
                        break;
                    case 3: /* Long outer displacement */
                        outer_reg = RA_AllocARMRegister(&ptr);
                        hi16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                        lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                        *ptr++ = movw_immed_u16(outer_reg, lo16);
                        if (hi16 != 0)
                            *ptr++ = movt_immed_u16(outer_reg, hi16);
                        break;
                }

                if ((brief & 0x0f) == 0)
                {
                    /* Address register indirect with index mode */
                    if (base_reg != 0xff && bd_reg != 0xff)
                    {
                        *ptr++ = add_reg(bd_reg, base_reg, bd_reg, LSL, 0);
                    }
                    else if (bd_reg == 0xff && base_reg != 0xff)
                    {
                        bd_reg = base_reg;
                    }
                    /*
                        Now, either base register or base displacement were given, if
                        index register was specified, use it.
                    */
                    ptr = load_reg_from_addr(ptr, size, bd_reg, *arm_reg, index_reg, (brief >> 9) & 3, sign_ext);
                }
                else
                {
                    if (bd_reg == 0xff)
                    {
                        bd_reg = RA_AllocARMRegister(&ptr);
                        if (base_reg == 0xff)
                            *ptr++ = mov_reg(bd_reg, 31);
                        else
                            *ptr++ = mov_reg(bd_reg, base_reg);
                        base_reg = 0xff;
                    }

                    /* Postindexed mode */
                    if (brief & 0x04)
                    {
                        /* Fetch data from base reg */
                        if (base_reg == 0xff)
                            *ptr++ = ldr_offset(bd_reg, bd_reg, 0);
                        else
                            *ptr++ = ldr_regoffset(bd_reg, bd_reg, base_reg, UXTW, 0);
                        
                        if (outer_reg != 0xff)
                            *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, LSL, 0);

                        ptr = load_reg_from_addr(ptr, size, bd_reg, *arm_reg, index_reg, (brief >> 9) & 3, sign_ext);
                    }
                    else /* Preindexed mode */
                    {
                        /* Fetch data from base reg with eventually applied index */
                        if (brief & M68K_EA_IS)
                        {
                            if (bd_reg == 0xff) {
                                bd_reg = RA_AllocARMRegister(&ptr);
                                *ptr++ = ldr_offset(base_reg, bd_reg, 0);
                            }
                            else
                                ptr = load_reg_from_addr(ptr, 4, base_reg, bd_reg, bd_reg, 0, 0);
                        }
                        else
                        {
                            if (bd_reg != 0xff && base_reg != 0xff)
                                *ptr++ = add_reg(bd_reg, base_reg, bd_reg, LSL, 0);
                            ptr = load_reg_from_addr(ptr, 4, bd_reg, bd_reg, index_reg, (brief >> 9) & 3, 0);
                        }

                        if (outer_reg != 0xff)
                            *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, LSL, 0);

                        ptr = load_reg_from_addr(ptr, size, bd_reg, *arm_reg, 0xff, 0, sign_ext);
                    }
                }

                if (bd_reg != 0xff)
                    RA_FreeARMRegister(&ptr, bd_reg);
                if (outer_reg != 0xff)
                    RA_FreeARMRegister(&ptr, outer_reg);
                if (base_reg != 0xff)
                    RA_FreeARMRegister(&ptr, base_reg);
                if (index_reg != 0xff)
                    RA_FreeARMRegister(&ptr, index_reg);
            }
        }
        else if (mode == 7)
        {
            if (src_reg == 2) /* (d16, PC) mode */
            {
                if (imm_offset && size == 0 && read_only)
                {
                    int8_t off8 = 2 + 2*(*ext_words);
                    ptr = EMIT_GetOffsetPC(ptr, &off8);
                    RA_FreeARMRegister(&ptr, *arm_reg);
                    *arm_reg = REG_PC;
                    *imm_offset = off8 + (int16_t)cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                }
                else
                {
                    int8_t off8 = 2 + 2*(*ext_words);
                    ptr = EMIT_GetOffsetPC(ptr, &off8);
                    int32_t off = off8 + (int16_t)(cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]));

                    ptr = load_reg_from_addr_offset(ptr, size, REG_PC, *arm_reg, off, 1, sign_ext);
                }
            }
            else if (src_reg == 3)
            {
                uint16_t brief = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                uint8_t extra_reg = (brief >> 12) & 7;

                if ((brief & 0x0100) == 0)
                {
                    uint8_t tmp1 = RA_AllocARMRegister(&ptr);
                    uint8_t tmp2 = 0xff;
                    int8_t displ = brief & 0xff;
                    int8_t off = 2 + 2*(*ext_words - 1);
                    int16_t full_off = 0;
                    ptr = EMIT_GetOffsetPC(ptr, &off);

                    full_off = off + displ;

                    if (full_off >= 0)
                    {
                        *ptr++ = add_immed(tmp1, REG_PC, full_off);
                    }
                    else
                    {
                        *ptr++ = sub_immed(tmp1, REG_PC, -full_off);
                    }

                    if (brief & (1 << 11))
                    {
                        if (brief & 0x8000)
                            tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg); // RA_CopyFromM68kRegister(&ptr, 8 + extra_reg);
                        else
                            tmp2 = RA_MapM68kRegister(&ptr, extra_reg); // RA_CopyFromM68kRegister(&ptr, extra_reg);
                    }
                    else
                    {
                        if (brief & 0x8000)
                            tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                        else
                            tmp2 = RA_MapM68kRegister(&ptr, extra_reg);

                        uint8_t tmp3 = RA_AllocARMRegister(&ptr);
                        *ptr++ = sxth(tmp3, tmp2);

                        RA_FreeARMRegister(&ptr, tmp2);
                        tmp2 = tmp3;
                    }

                    ptr = load_reg_from_addr(ptr, size, tmp1, *arm_reg, tmp2, (brief >> 9) & 3, sign_ext);

                    RA_FreeARMRegister(&ptr, tmp1);
                    RA_FreeARMRegister(&ptr, tmp2);
                }
                else
                {
                    uint8_t bd_reg = 0xff;
                    uint8_t outer_reg = 0xff;
                    uint8_t base_reg = 0xff;
                    uint8_t index_reg = 0xff;

                    /* Check if base register is suppressed */
                    if (!(brief & M68K_EA_BS))
                    {
                        /* Base register in use. Alloc it and load its contents */
                        base_reg = RA_AllocARMRegister(&ptr);
                        int8_t off = 2 + 2*(*ext_words - 1);
                        //ptr = EMIT_FlushPC(ptr);
                        ptr = EMIT_GetOffsetPC(ptr, &off);
                        if (off > 0)
                            *ptr++ = add_immed(base_reg, REG_PC, off);
                        else
                            *ptr++ = sub_immed(base_reg, REG_PC, -off);
                    }

                    /* Check if index register is in use */
                    if (!(brief & M68K_EA_IS))
                    {
                        /* Index register in use. Alloc it and load its contents */
                        if (brief & (1 << 11))
                        {
                            if (brief & 0x8000)
                                index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg); // RA_CopyFromM68kRegister(&ptr, 8 + extra_reg);
                            else
                                index_reg = RA_MapM68kRegister(&ptr, extra_reg); // RA_CopyFromM68kRegister(&ptr, extra_reg);
                        }
                        else
                        {
                            if (brief & 0x8000)
                                index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                            else
                                index_reg = RA_MapM68kRegister(&ptr, extra_reg);

                            uint8_t tmp3 = RA_AllocARMRegister(&ptr);
                            *ptr++ = sxth(tmp3, index_reg);

                            RA_FreeARMRegister(&ptr, index_reg);
                            index_reg = tmp3;
                        }
                    }

                    int8_t pc_off = 2 + (*ext_words) * 2;
                    ptr = EMIT_GetOffsetPC(ptr, &pc_off);
                    /* Check if base displacement needs to be fetched */
                    switch ((brief & M68K_EA_BD_SIZE) >> 4)
                    {
                        case 2: /* Word displacement */
                            bd_reg = RA_AllocARMRegister(&ptr);
                            *ptr++ = ldrsh_offset(REG_PC, bd_reg, pc_off);
                            (*ext_words)++;
                            break;
                        case 3: /* Long displacement */
                            bd_reg = RA_AllocARMRegister(&ptr);
                            if (pc_off & 2) {
                                *ptr++ = ldur_offset(REG_PC, bd_reg, pc_off);
                            } else {
                                *ptr++ = ldr_offset(REG_PC, bd_reg, pc_off);
                            }
                            (*ext_words) += 2;
                            break;
                    }

                    pc_off = 2 + (*ext_words) * 2;
                    ptr = EMIT_GetOffsetPC(ptr, &pc_off);

                    /* Check if outer displacement needs to be fetched */
                    switch ((brief & M68K_EA_IIS) & 3)
                    {
                        case 2: /* Word outer displacement */
                            outer_reg = RA_AllocARMRegister(&ptr);
                            *ptr++ = ldrsh_offset(REG_PC, outer_reg, pc_off);
                            (*ext_words)++;
                            break;
                        case 3: /* Long outer displacement */
                            outer_reg = RA_AllocARMRegister(&ptr);
                            if (pc_off & 2) {
                                *ptr++ = ldur_offset(REG_PC, outer_reg, pc_off);
                            }
                            else
                                *ptr++ = ldr_offset(REG_PC, outer_reg, pc_off);
                            (*ext_words) += 2;
                            break;
                    }

                    if ((brief & 0x0f) == 0)
                    {
                        /* Address register indirect with index mode */
                        if (base_reg != 0xff && bd_reg != 0xff)
                        {
                            *ptr++ = add_reg(bd_reg, base_reg, bd_reg, LSL, 0);
                        }
                        else if (bd_reg == 0xff && base_reg != 0xff)
                        {
                            bd_reg = base_reg;
                        }
                        /*
                            Now, either base register or base displacement were given, if
                            index register was specified, use it.
                        */
                        ptr = load_reg_from_addr(ptr, size, bd_reg, *arm_reg, index_reg, (brief >> 9) & 3, sign_ext);
                    }
                    else
                    {
                        if (bd_reg == 0xff)
                        {
                            bd_reg = RA_AllocARMRegister(&ptr);
                            if (base_reg == 0xff)
                                *ptr++ = mov_reg(bd_reg, 31);
                            else
                                *ptr++ = mov_reg(bd_reg, base_reg);
                            RA_FreeARMRegister(&ptr, base_reg);
                            base_reg = 0xff;
                        }

                        /* Postindexed mode */
                        if (brief & 0x04)
                        {
                            /* Fetch data from base reg */
                            if (base_reg == 0xff)
                                *ptr++ = ldr_offset(bd_reg, bd_reg, 0);
                            else
                                *ptr++ = ldr_regoffset(bd_reg, bd_reg, base_reg, UXTW, 0);

                            if (outer_reg != 0xff)
                                *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, LSL, 0);

                            ptr = load_reg_from_addr(ptr, size, bd_reg, *arm_reg, index_reg, (brief >> 9) & 3, sign_ext);
                        }
                        else /* Preindexed mode */
                        {
                            /* Fetch data from base reg with eventually applied index */
                            if (brief & M68K_EA_IS)
                            {
                                if (bd_reg == 0xff) {
                                    bd_reg = RA_AllocARMRegister(&ptr);
                                    *ptr++ = ldr_offset(base_reg, bd_reg, 0);
                                }
                                else
                                {
                                    if (base_reg == 0xff) {
                                        uint8_t t = RA_AllocARMRegister(&ptr);
                                        *ptr++ = mov_reg(t, 31);
                                        *ptr++ = ldr_regoffset(t, bd_reg, bd_reg, UXTW, 0);
                                        RA_FreeARMRegister(&ptr, t);
                                    }
                                    else {
                                        *ptr++ = ldr_regoffset(base_reg, bd_reg, bd_reg, UXTW, 0);
                                    }
                                }
                            }
                            else
                            {
                                if (bd_reg != 0xff) {
                                    if (base_reg != 0xff)
                                        *ptr++ = add_reg(bd_reg, base_reg, bd_reg, LSL, 0);
                                }

                                ptr = load_reg_from_addr(ptr, 4, bd_reg, bd_reg, index_reg, (brief >> 9) & 3, 0);
                            }

                            if (outer_reg != 0xff)
                                *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, LSL, 0);

                            ptr = load_reg_from_addr_offset(ptr, size, bd_reg, *arm_reg, 0, 0, sign_ext);
                        }
                    }

                    if (bd_reg != 0xff)
                        RA_FreeARMRegister(&ptr, bd_reg);
                    if (outer_reg != 0xff)
                        RA_FreeARMRegister(&ptr, outer_reg);
                    if (base_reg != 0xff)
                        RA_FreeARMRegister(&ptr, base_reg);
                    if (index_reg != 0xff)
                        RA_FreeARMRegister(&ptr, index_reg);
                }
            }
            else if (src_reg == 0)
            {
                uint16_t lo16;
                lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);

                if (size == 0) {
                    ptr = load_s16_ext32(ptr, *arm_reg, lo16);
                }
                else
                {
                    uint8_t tmp_reg = RA_AllocARMRegister(&ptr);
                    ptr = load_s16_ext32(ptr, tmp_reg, lo16);
                    ptr = load_reg_from_addr_offset(ptr, size, tmp_reg, *arm_reg, 0, 0, sign_ext);
                    RA_FreeARMRegister(&ptr, tmp_reg);
                }
            }
            else if (src_reg == 1)
            {
                uint16_t hi16, lo16;
                hi16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);

                if (size == 0) {
                    if (lo16 == 0 && hi16 == 0)
                    {
                        *ptr++ = mov_reg(*arm_reg, 31);
                    }
                    else if (lo16 != 0)
                    {
                        *ptr++ = movw_immed_u16(*arm_reg, lo16);
                        if (hi16 != 0 || lo16 & 0x8000)
                            *ptr++ = movt_immed_u16(*arm_reg, hi16);
                    }
                    else
                    {
                        *ptr++ = mov_immed_u16(*arm_reg, hi16, 1);
                    }
                }
                else
                {
                    uint8_t tmp_reg = RA_AllocARMRegister(&ptr);

                    if (lo16 == 0 && hi16 == 0)
                    {
                        *ptr++ = mov_reg(tmp_reg, 31);
                    }
                    else if (lo16 != 0)
                    {
                        *ptr++ = movw_immed_u16(tmp_reg, lo16);
                        if (hi16 != 0 || lo16 & 0x8000)
                            *ptr++ = movt_immed_u16(tmp_reg, hi16);
                    }
                    else
                    {
                        *ptr++ = mov_immed_u16(tmp_reg, hi16, 1);
                    }

                    switch (size)
                    {
                        case 4:
                            *ptr++ = ldr_offset(tmp_reg, *arm_reg, 0);
                            break;
                        case 2:
                            if (sign_ext)
                                *ptr++ = ldrsh_offset(tmp_reg, *arm_reg, 0);
                            else
                                *ptr++ = ldrh_offset(tmp_reg, *arm_reg, 0);
                            break;
                        case 1:
                            if (sign_ext)
                                *ptr++ = ldrsb_offset(tmp_reg, *arm_reg, 0);
                            else
                                *ptr++ = ldrb_offset(tmp_reg, *arm_reg, 0);
                            break;
                    }
                    RA_FreeARMRegister(&ptr, tmp_reg);
                }
            }
            else if (src_reg == 4)
            {
                int8_t pc_off;
                uint16_t lo16, hi16, off;
                switch (size)
                {
                    case 4:
                        hi16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                        lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);

                        if (lo16 == 0 && hi16 == 0)
                        {
                            *ptr++ = mov_reg(*arm_reg, 31);
                        }
                        else if (lo16 != 0)
                        {
                            *ptr++ = movw_immed_u16(*arm_reg, lo16);
                            if (hi16 != 0 || lo16 & 0x8000)
                                *ptr++ = movt_immed_u16(*arm_reg, hi16);
                        }
                        else
                        {
                            *ptr++ = mov_immed_u16(*arm_reg, hi16, 1);
                        }
                        break;
                    case 2:
                        off = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);

                        if (sign_ext && (off & 0x8000))
                            *ptr++ = movn_immed_u16(*arm_reg, ~off, 0);
                        else
                            *ptr++ = mov_immed_u16(*arm_reg, off, 0);
                        break;
                    case 1:
                        off = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]) & 0xff;
                        if (sign_ext && (off & 0x80))
                            *ptr++ = movn_immed_u16(*arm_reg, ~(off | 0xff00), 0);
                        else
                            *ptr++ = mov_immed_u16(*arm_reg, off, 0);
                        break;
                    case 0:
                        pc_off = 2 + 2*(*ext_words);
                        ptr = EMIT_GetOffsetPC(ptr, &pc_off);
                        *ptr++ = add_immed(*arm_reg, REG_PC, pc_off);
                        break;
                }
            }
        }
    }

    return ptr;
}

/*
    Emits ARM insns to load effective address and store value from specified register to the EA.

    Inputs:
        ptr     pointer to ARM instruction stream
        size    size of data for store operation, can be 4 (long), 2 (short) or 1 (byte).
                If size of 0 is specified the function does not store a value from register into
                EA but rather loads the EA into that register.
                If postincrement or predecrement modes are used and size 0 is specified, then
                the instruction translator is reponsible for increasing/decreasing the address
                register, otherwise it is done in this function!
        arm_reg ARM register to store the EA or value from EA into
        ea      EA encoded field.
        m68k_ptr pointer to m68k instruction stream past the instruction opcode itself. It may
                be increased if further bytes from m68k side are read

    Output:
        ptr     pointer to ARM instruction stream after the newly generated code
*/
uint32_t *EMIT_StoreToEffectiveAddress(uint32_t *ptr, uint8_t size, uint8_t *arm_reg, uint8_t ea, uint16_t *m68k_ptr, uint8_t *ext_words, int sign_extend)
{
    uint8_t mode = ea >> 3;
    uint8_t src_reg = ea & 7;
    (void)ext_words;
    (void)m68k_ptr;
    if (size == 0)
        *arm_reg = RA_AllocARMRegister(&ptr);

    if (mode == 0) /* Mode 000: Dn */
    {
        uint8_t reg_dest;
        switch (size)
        {
            case 4:
                reg_dest = RA_MapM68kRegisterForWrite(&ptr, src_reg);
                *ptr++ = mov_reg(reg_dest, *arm_reg);
                break;
            case 2:
                if (sign_extend)
                {
                    reg_dest = RA_MapM68kRegisterForWrite(&ptr, src_reg);
                    *ptr++ = sxth(reg_dest, *arm_reg);
                }
                else
                {
                    reg_dest = RA_MapM68kRegister(&ptr, src_reg);
                    RA_SetDirtyM68kRegister(&ptr, src_reg);
                    *ptr++ = bfi(reg_dest, *arm_reg, 0, 16);
                }
                break;
            case 1:
                if (sign_extend)
                {
                    reg_dest = RA_MapM68kRegisterForWrite(&ptr, src_reg);
                    *ptr++ = sxtb(reg_dest, *arm_reg);
                }
                else
                {
                    reg_dest = RA_MapM68kRegister(&ptr, src_reg);
                    RA_SetDirtyM68kRegister(&ptr, src_reg);
                    *ptr++ = bfi(reg_dest, *arm_reg, 0, 8);
                }
                break;
            case 0:
                kprintf("Store to EA with wrong operand size 0\n");
                break;
            default:
                kprintf("Wrong size\n");
                break;
        }
    }
    else if (mode == 1) /* Mode 001: An */
    {
        uint8_t reg_dest;
        switch (size)
        {
            case 4:
                reg_dest = RA_MapM68kRegisterForWrite(&ptr, 8 + src_reg);
                *ptr++ = mov_reg(reg_dest, *arm_reg);
                break;
            case 2:
                if (sign_extend)
                {
                    reg_dest = RA_MapM68kRegisterForWrite(&ptr, 8 + src_reg);
                    *ptr++ = sxth(reg_dest, *arm_reg);
                }
                else
                {
                    reg_dest = RA_MapM68kRegister(&ptr, 8 + src_reg);
                    RA_SetDirtyM68kRegister(&ptr, src_reg + 8);
                    *ptr++ = bfi(reg_dest, *arm_reg, 0, 16);
                }
                break;
            case 0:
                kprintf("Store to EA with wrong operand size 0\n");
                break;
            default:
                kprintf("Wrong size\n");
                break;
        }
    }
    else
    {
        if (mode == 2) /* Mode 002: (An) */
        {
            if (size == 0) {
                uint8_t tmp = RA_MapM68kRegister(&ptr, src_reg + 8);
                *ptr++ = mov_reg(*arm_reg, tmp);
            }
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                ptr = store_reg_to_addr_offset(ptr, size, reg_An, *arm_reg, 0, 0);
            }
        }
        else if (mode == 3) /* Mode 003: (An)+ */
        {
            if (size == 0) {
                RA_FreeARMRegister(&ptr, *arm_reg);
                *arm_reg = RA_MapM68kRegister(&ptr, src_reg + 8);
            }
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);

                RA_SetDirtyM68kRegister(&ptr, 8 + src_reg);

                switch (size)
                {
                case 4:
                    *ptr++ = str_offset_postindex(reg_An, *arm_reg, 4);
                    break;
                case 2:
                    *ptr++ = strh_offset_postindex(reg_An, *arm_reg, 2);
                    break;
                case 1:
                    if (src_reg == 7)
                        *ptr++ = strb_offset_postindex(reg_An, *arm_reg, 2);
                    else
                        *ptr++ = strb_offset_postindex(reg_An, *arm_reg, 1);
                    break;
                default:
                    kprintf("Unknown size opcode\n");
                    break;
                }
            }
        }
        else if (mode == 4) /* Mode 004: -(An) */
        {
            if (size == 0) {
                RA_FreeARMRegister(&ptr, *arm_reg);
                *arm_reg = RA_MapM68kRegister(&ptr, src_reg + 8);
            }
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);

                RA_SetDirtyM68kRegister(&ptr, 8 + src_reg);

                switch (size)
                {
                case 4:
                    *ptr++ = str_offset_preindex(reg_An, *arm_reg, -4);
                    break;
                case 2:
                    *ptr++ = strh_offset_preindex(reg_An, *arm_reg, -2);
                    break;
                case 1:
                    if (src_reg == 7)
                        *ptr++ = strb_offset_preindex(reg_An, *arm_reg, -2);
                    else
                        *ptr++ = strb_offset_preindex(reg_An, *arm_reg, -1);
                    break;
                default:
                    kprintf("Unknown size opcode\n");
                    break;
                }
            }
        }
        else if (mode == 5) /* Mode 005: (d16, An) */
        {
            uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
            int16_t off16 = (int16_t)cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);

            ptr = store_reg_to_addr_offset(ptr, size, reg_An, *arm_reg, off16, 0);
        }
        else if (mode == 6) /* Mode 006: (d8, An, Xn.SIZE*SCALE) */
        {
            uint16_t brief = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
            uint8_t extra_reg = (brief >> 12) & 7;

            if ((brief & 0x0100) == 0)
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                uint8_t tmp1 = 0xff;
                uint8_t tmp2 = 0xff;
                int8_t displ = brief & 0xff;

                if (displ > 0)
                {
                    tmp1 = RA_AllocARMRegister(&ptr);
                    *ptr++ = add_immed(tmp1, reg_An, displ);
                }
                else if (displ < 0)
                {
                    tmp1 = RA_AllocARMRegister(&ptr);
                    *ptr++ = sub_immed(tmp1, reg_An, -displ);
                }

                if (brief & (1 << 11))
                {
                    if (brief & 0x8000)
                        tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg); // RA_CopyFromM68kRegister(&ptr, 8 + extra_reg);
                    else
                        tmp2 = RA_MapM68kRegister(&ptr, extra_reg); // RA_CopyFromM68kRegister(&ptr, extra_reg);
                }
                else
                {
                    if (brief & 0x8000)
                        tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                    else
                        tmp2 = RA_MapM68kRegister(&ptr, extra_reg);

                    uint8_t tmp3 = RA_AllocARMRegister(&ptr);
                    *ptr++ = sxth(tmp3, tmp2);

                    RA_FreeARMRegister(&ptr, tmp2);
                    tmp2 = tmp3;
                }

                ptr = store_reg_to_addr(ptr, size, displ ? tmp1 : reg_An, *arm_reg, tmp2, (brief >> 9) & 3);

                if (displ) RA_FreeARMRegister(&ptr, tmp1);
                RA_FreeARMRegister(&ptr, tmp2);
            }
            else
            {
                uint8_t bd_reg = 0xff;
                uint8_t outer_reg = 0xff;
                uint8_t base_reg = 0xff;
                uint8_t index_reg = 0xff;

                /* Check if base register is suppressed */
                if (!(brief & M68K_EA_BS))
                {
                    /* Base register in use. Alloc it and load its contents */
                    base_reg = RA_MapM68kRegister(&ptr, 8 + src_reg);
                }

                /* Check if index register is in use */
                if (!(brief & M68K_EA_IS))
                {
                    /* Index register in use. Alloc it and load its contents */
                    if (brief & (1 << 11))
                    {
                        if (brief & 0x8000)
                            index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg); // RA_CopyFromM68kRegister(&ptr, 8 + extra_reg);
                        else
                            index_reg = RA_MapM68kRegister(&ptr, extra_reg); // RA_CopyFromM68kRegister(&ptr, extra_reg);
                    }
                    else
                    {
                        if (brief & 0x8000)
                            index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                        else
                            index_reg = RA_MapM68kRegister(&ptr, extra_reg);

                        uint8_t tmp3 = RA_AllocARMRegister(&ptr);

                        *ptr++ = sxth(tmp3, index_reg);

                        RA_FreeARMRegister(&ptr, index_reg);
                        index_reg = tmp3;
                    }
                }
                uint16_t lo16, hi16;

                /* Check if base displacement needs to be fetched */
                switch ((brief & M68K_EA_BD_SIZE) >> 4)
                {
                    case 2: /* Word displacement */
                        bd_reg = RA_AllocARMRegister(&ptr);
                        lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                        ptr = load_s16_ext32(ptr, bd_reg, lo16);
                        break;
                    case 3: /* Long displacement */
                        bd_reg = RA_AllocARMRegister(&ptr);
                        hi16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                        lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                        *ptr++ = movw_immed_u16(bd_reg, lo16);
                        if (hi16)
                            *ptr++ = movt_immed_u16(bd_reg, hi16);
                        break;
                }

                /* Check if outer displacement needs to be fetched */
                switch ((brief & M68K_EA_IIS) & 3)
                {
                    case 2: /* Word outer displacement */
                        outer_reg = RA_AllocARMRegister(&ptr);
                        lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                        ptr = load_s16_ext32(ptr, outer_reg, lo16);
                        break;
                    case 3: /* Long outer displacement */
                        outer_reg = RA_AllocARMRegister(&ptr);
                        hi16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                        lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                        *ptr++ = movw_immed_u16(outer_reg, lo16);
                        if (hi16)
                            *ptr++ = movt_immed_u16(outer_reg, hi16);
                        break;
                }

                if ((brief & 0x0f) == 0)
                {
                    /* Address register indirect with index mode */
                    if (base_reg != 0xff && bd_reg != 0xff)
                    {
                        *ptr++ = add_reg(bd_reg, base_reg, bd_reg, LSL, 0);
                    }
                    else if (bd_reg == 0xff && base_reg != 0xff)
                    {
                        bd_reg = base_reg;
                    }
                    /*
                        Now, either base register or base displacement were given, if
                        index register was specified, use it.
                    */

                    ptr = store_reg_to_addr(ptr, size, bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                }
                else
                {
                    if (bd_reg == 0xff)
                    {
                        bd_reg = RA_AllocARMRegister(&ptr);
                        if (base_reg == 0xff)
                            *ptr++ = mov_reg(bd_reg, 31);
                        else
                            *ptr++ = mov_reg(bd_reg, base_reg);
                        base_reg = 0xff;
                    }

                    /* Postindexed mode */
                    if (brief & 0x04)
                    {
                        /* Fetch data from base reg */
                        if (base_reg == 0xff)
                            *ptr++ = ldr_offset(bd_reg, bd_reg, 0);
                        else
                            *ptr++ = ldr_regoffset(bd_reg, bd_reg, base_reg, UXTW, 0);
                        
                        if (outer_reg != 0xff)
                            *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, LSL, 0);

                        ptr = store_reg_to_addr(ptr, size, bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                    }
                    else /* Preindexed mode */
                    {
                        /* Fetch data from base reg with eventually applied index */
                        if (brief & M68K_EA_IS)
                        {
                            if (bd_reg == 0xff)
                            {
                                bd_reg = RA_AllocARMRegister(&ptr);
                                *ptr++ = ldr_offset(base_reg, bd_reg, 0);
                            }
                            else
                            {
                                if (base_reg == 0xff) {
                                    uint8_t t = RA_AllocARMRegister(&ptr);
                                    *ptr++ = mov_reg(t, 31);
                                    *ptr++ = ldr_regoffset(t, bd_reg, bd_reg, UXTW, 0);
                                    RA_FreeARMRegister(&ptr, t);
                                }
                                else
                                    *ptr++ = ldr_regoffset(base_reg, bd_reg, bd_reg, UXTW, 0);
                            }
                        }
                        else
                        {
                            if (bd_reg != 0xff && base_reg != 0xff)
                                *ptr++ = add_reg(bd_reg, base_reg, bd_reg, LSL, 0);
                            ptr = load_reg_from_addr(ptr, 4, bd_reg, bd_reg, index_reg, (brief >> 9) & 3, 0);
                        }

                        if (outer_reg != 0xff)
                            *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, LSL, 0);

                        ptr = store_reg_to_addr(ptr, size, bd_reg, *arm_reg, 0xff, 0);
                    }
                }

                if (bd_reg != 0xff)
                    RA_FreeARMRegister(&ptr, bd_reg);
                if (outer_reg != 0xff)
                    RA_FreeARMRegister(&ptr, outer_reg);
                if (base_reg != 0xff)
                    RA_FreeARMRegister(&ptr, base_reg);
                if (index_reg != 0xff)
                    RA_FreeARMRegister(&ptr, index_reg);
            }
        }
        else if (mode == 7)
        {
            if (src_reg == 2) /* (d16, PC) mode */
            {
                int8_t off = 2;
                int32_t off32 = (int16_t)cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                ptr = EMIT_GetOffsetPC(ptr, &off);
                off32 += off;

                ptr = store_reg_to_addr_offset(ptr, size, REG_PC, *arm_reg, off32, 1);
            }
            else if (src_reg == 3)
            {
                uint16_t brief = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                uint8_t extra_reg = (brief >> 12) & 7;

                if ((brief & 0x0100) == 0)
                {
                    uint8_t tmp1 = RA_AllocARMRegister(&ptr);
                    uint8_t tmp2 = 0xff;
                    int8_t displ = brief & 0xff;
                    int8_t off = 2;
                    int16_t full_off = 0;
                    ptr = EMIT_GetOffsetPC(ptr, &off);

                    full_off = off + displ;

                    if (full_off >= 0)
                    {
                        *ptr++ = add_immed(tmp1, REG_PC, full_off);
                    }
                    else
                    {
                        *ptr++ = sub_immed(tmp1, REG_PC, -full_off);
                    }

                    if (brief & (1 << 11))
                    {
                        if (brief & 0x8000)
                            tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg); // RA_CopyFromM68kRegister(&ptr, 8 + extra_reg);
                        else
                            tmp2 = RA_MapM68kRegister(&ptr, extra_reg); // RA_CopyFromM68kRegister(&ptr, extra_reg);
                    }
                    else
                    {
                        if (brief & 0x8000)
                            tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                        else
                            tmp2 = RA_MapM68kRegister(&ptr, extra_reg);

                        uint8_t tmp3 = RA_AllocARMRegister(&ptr);

                        *ptr++ = sxth(tmp3, tmp2);

                        RA_FreeARMRegister(&ptr, tmp2);
                        tmp2 = tmp3;
                    }

                    ptr = store_reg_to_addr(ptr, size, tmp1, *arm_reg, tmp2, (brief >> 9) & 3);

                    RA_FreeARMRegister(&ptr, tmp1);
                    RA_FreeARMRegister(&ptr, tmp2);
                }
                else
                {
                    uint8_t bd_reg = 0xff;
                    uint8_t outer_reg = 0xff;
                    uint8_t base_reg = 0xff;
                    uint8_t index_reg = 0xff;

                    /* Check if base register is suppressed */
                    if (!(brief & M68K_EA_BS))
                    {
                        /* Base register in use. Alloc it and load its contents */
                        base_reg = RA_AllocARMRegister(&ptr);
                        int8_t off = 2;
                        //ptr = EMIT_FlushPC(ptr);
                        ptr = EMIT_GetOffsetPC(ptr, &off);
                        if (off > 0)
                            *ptr++ = add_immed(base_reg, REG_PC, off);
                        else
                            *ptr++ = sub_immed(base_reg, REG_PC, -off);

                        //*ptr++ = add_immed(base_reg, REG_PC, 2);
                    }

                    /* Check if index register is in use */
                    if (!(brief & M68K_EA_IS))
                    {
                        /* Index register in use. Alloc it and load its contents */
                        if (brief & (1 << 11))
                        {
                            if (brief & 0x8000)
                                index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg); // RA_CopyFromM68kRegister(&ptr, 8 + extra_reg);
                            else
                                index_reg = RA_MapM68kRegister(&ptr, extra_reg); // RA_CopyFromM68kRegister(&ptr, extra_reg);
                        }
                        else
                        {
                            if (brief & 0x8000)
                                index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                            else
                                index_reg = RA_MapM68kRegister(&ptr, extra_reg);

                            uint8_t tmp3 = RA_AllocARMRegister(&ptr);

                            *ptr++ = sxth(tmp3, index_reg);

                            RA_FreeARMRegister(&ptr, index_reg);
                            index_reg = tmp3;
                        }
                    }

                    uint16_t lo16, hi16;

                    /* Check if base displacement needs to be fetched */
                    switch ((brief & M68K_EA_BD_SIZE) >> 4)
                    {
                        case 2: /* Word displacement */
                            bd_reg = RA_AllocARMRegister(&ptr);
                            lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                            ptr = load_s16_ext32(ptr, bd_reg, lo16);
                            break;
                        case 3: /* Long displacement */
                            bd_reg = RA_AllocARMRegister(&ptr);
                            hi16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                            lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                            *ptr++ = movw_immed_u16(bd_reg, lo16);
                            if (hi16)
                                *ptr++ = movt_immed_u16(bd_reg, hi16);
                            break;
                    }

                    /* Check if outer displacement needs to be fetched */
                    switch ((brief & M68K_EA_IIS) & 3)
                    {
                        case 2: /* Word outer displacement */
                            outer_reg = RA_AllocARMRegister(&ptr);
                            lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                            ptr = load_s16_ext32(ptr, outer_reg, lo16);
                            break;
                        case 3: /* Long outer displacement */
                            outer_reg = RA_AllocARMRegister(&ptr);
                            hi16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                            lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                            *ptr++ = movw_immed_u16(outer_reg, lo16);
                            if (hi16)
                                *ptr++ = movt_immed_u16(outer_reg, hi16);
                            break;
                    }

                    if ((brief & 0x0f) == 0)
                    {
                        /* Address register indirect with index mode */
                        if (base_reg != 0xff && bd_reg != 0xff)
                        {
                            *ptr++ = add_reg(bd_reg, base_reg, bd_reg, LSL, 0);
                        }
                        else if (bd_reg == 0xff && base_reg != 0xff)
                        {
                            bd_reg = base_reg;
                        }
                        /*
                            Now, either base register or base displacement were given, if
                            index register was specified, use it.
                        */
                        ptr = store_reg_to_addr(ptr, size, bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                    }
                    else
                    {
                        if (bd_reg == 0xff)
                        {
                            bd_reg = RA_AllocARMRegister(&ptr);
                            *ptr++ = mov_reg(bd_reg, base_reg);
                            base_reg = 0xff;
                        }

                        /* Postindexed mode */
                        if (brief & 0x04)
                        {
                            /* Fetch data from base reg */
                            if (base_reg == 0xff)
                                *ptr++ = ldr_offset(bd_reg, bd_reg, 0);
                            else
                                ptr = load_reg_from_addr(ptr, 4, bd_reg, bd_reg, base_reg, 0, 0);

                            if (outer_reg != 0xff)
                                *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, LSL, 0);

                            ptr = store_reg_to_addr(ptr, size, bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                        }
                        else /* Preindexed mode */
                        {
                            /* Fetch data from base reg with eventually applied index */
                            if (brief & M68K_EA_IS)
                            {
                                if (bd_reg == 0xff) {
                                    bd_reg = RA_AllocARMRegister(&ptr);
                                    *ptr++ = ldr_offset(base_reg, bd_reg, 0);
                                }
                                else
                                    ptr = load_reg_from_addr(ptr, 4, base_reg, bd_reg, bd_reg, 0, 0);
                            }
                            else
                            {
                                if (bd_reg != 0xff)
                                    *ptr++ = add_reg(bd_reg, base_reg, bd_reg, LSL, 0);

                                ptr = load_reg_from_addr(ptr, 4, bd_reg, bd_reg, index_reg, (brief >> 9) & 3, 0);
                            }

                            if (outer_reg != 0xff)
                                *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, LSL, 0);

                            ptr = store_reg_to_addr(ptr, size, bd_reg, *arm_reg, 0xff, 0);
                        }
                    }

                    if (bd_reg != 0xff)
                        RA_FreeARMRegister(&ptr, bd_reg);
                    if (outer_reg != 0xff)
                        RA_FreeARMRegister(&ptr, outer_reg);
                    if (base_reg != 0xff)
                        RA_FreeARMRegister(&ptr, base_reg);
                    if (index_reg != 0xff)
                        RA_FreeARMRegister(&ptr, index_reg);
                }
            }
            else if (src_reg == 0)
            {
                uint16_t lo16;
                lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);

                if (size == 0) {
                    ptr = load_s16_ext32(ptr, *arm_reg, lo16);
                }
                else
                {
                    uint8_t tmp_reg = RA_AllocARMRegister(&ptr);
                    ptr = load_s16_ext32(ptr, tmp_reg, lo16);
                    ptr = store_reg_to_addr(ptr, size, tmp_reg, *arm_reg, 0xff, 0);
                    RA_FreeARMRegister(&ptr, tmp_reg);
                }
            }
            else if (src_reg == 1)
            {
                uint16_t lo16, hi16;
                hi16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);
                lo16 = cache_read_16(ICACHE, (uintptr_t)&m68k_ptr[(*ext_words)++]);

                if (size == 0) {
                    if (lo16 == 0 && hi16 == 0)
                    {
                        *ptr++ = mov_reg(*arm_reg, 31);
                    }
                    else if (lo16 != 0)
                    {
                        *ptr++ = movw_immed_u16(*arm_reg, lo16);
                        if (hi16 != 0 || lo16 & 0x8000)
                            *ptr++ = movt_immed_u16(*arm_reg, hi16);
                    }
                    else
                    {
                        *ptr++ = mov_immed_u16(*arm_reg, hi16, 1);
                    }
                }
                else
                {
                    uint8_t tmp_reg = RA_AllocARMRegister(&ptr);

                    if (lo16 == 0 && hi16 == 0)
                    {
                        *ptr++ = mov_reg(tmp_reg, 31);
                    }
                    else if (lo16 != 0)
                    {
                        *ptr++ = movw_immed_u16(tmp_reg, lo16);
                        if (hi16 != 0 || lo16 & 0x8000)
                            *ptr++ = movt_immed_u16(tmp_reg, hi16);
                    }
                    else
                    {
                        *ptr++ = mov_immed_u16(tmp_reg, hi16, 1);
                    }

                    ptr = store_reg_to_addr(ptr, size, tmp_reg, *arm_reg, 0xff, 0);

                    RA_FreeARMRegister(&ptr, tmp_reg);
                }
            }
        }
    }

    return ptr;
}
