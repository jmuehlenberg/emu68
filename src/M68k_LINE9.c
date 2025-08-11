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

/* Line9 is one large SUBX/SUB/SUBA */

/****************************************************************************
* 1001regmmmxxxxxx SUB                                                      *
*****************************************************************************
* SUB <ea>,Dn|Dn,<ea>                                                       *
*                                                                           *
* Operation: dest - src → dest                                              *
*      X|N|Z|V|C                                                            *
* CC: (*|*|*|*|*)                                                           *
*                                                                           *
* Description: Subtracts the src operand from the dest operand and stores   *
* the result in the dest. The size of the operation is specified as byte,   *
* word or long. The mode of the instruction indicates which operand is      *
* the src and which is the dest, and which is the operand size.             *
****************************************************************************/

uint32_t *EMIT_SUB_ext(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_SUB_reg")));
uint32_t *EMIT_SUB_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_SUB_reg")));
uint32_t *EMIT_SUB_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t direction = (opcode >> 8) & 1; // 0: Ea+Dn->Dn, 1: Ea+Dn->Ea
    uint8_t ext_words = 0;
    uint8_t tmp = 0xff;

    if (direction == 0)
    {
        uint8_t dest = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
        uint8_t src = 0xff;

        RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);
        if (size == 4 || update_mask == 0 || update_mask == SR_Z || update_mask == SR_N)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
        else
            ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

        switch(size)
        {
            case 4:
                *ptr++ = subs_reg(dest, dest, src, LSL, 0);
                break;
            case 2:
                if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = sub_reg(tmp, dest, src, LSL, 0);
                    *ptr++ = bfxil(dest, tmp, 0, 16);
                    RA_FreeARMRegister(&ptr, tmp);
                }
                else {
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = lsl(tmp, dest, 16);
                    *ptr++ = subs_reg(src, tmp, src, LSL, 16);
                    *ptr++ = bfxil(dest, src, 16, 16);
                    RA_FreeARMRegister(&ptr, tmp);
                }
                break;
            case 1:
                if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = sub_reg(tmp, dest, src, LSL, 0);
                    *ptr++ = bfxil(dest, tmp, 0, 8);
                    RA_FreeARMRegister(&ptr, tmp);
                }
                else {
                    tmp = RA_AllocARMRegister(&ptr);
                    *ptr++ = lsl(tmp, dest, 24);
                    *ptr++ = subs_reg(src, tmp, src, LSL, 24);
                    *ptr++ = bfxil(dest, src, 24, 8);
                    RA_FreeARMRegister(&ptr, tmp);
                }
                break;
        }

        if (size < 4 && update_mask == SR_Z)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_ClearFlags(ptr, cc, SR_Z);

            switch (size) {
                case 2:
                    *ptr++ = ands_immed(31, dest, 16, 0);
                    break;
                case 1:
                    *ptr++ = ands_immed(31, dest, 8, 0);
                    break;
            }
            
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, A64_CC_EQ);
            update_mask = 0;
        }

        if (size < 4 && update_mask == SR_N)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_ClearFlags(ptr, cc, SR_N);

            switch (size) {
                case 2:
                    *ptr++ = ands_immed(31, dest, 1, 32-15);
                    break;
                case 1:
                    *ptr++ = ands_immed(31, dest, 1, 32-7);
                    break;
            }
            
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, A64_CC_NE);
            update_mask = 0;
        }

        RA_FreeARMRegister(&ptr, src);
    }
    else
    {
        uint8_t dest = 0xff;
        uint8_t src = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;

        if (mode == 4 || mode == 3)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);
        else
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0, &dest, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

        /* Fetch data into temporary register, perform add, store it back */
        switch (size)
        {
        case 4:
            if (mode == 4)
            {
                *ptr++ = ldr_offset_preindex(dest, tmp, -4);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldr_offset(dest, tmp, 0);

            /* Perform calcualtion */
            *ptr++ = subs_reg(tmp, tmp, src, LSL, 0);

            /* Store back */
            if (mode == 3)
            {
                *ptr++ = str_offset_postindex(dest, tmp, 4);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = str_offset(dest, tmp, 0);
            break;
        
        case 2:
            if (mode == 4)
            {
                *ptr++ = ldrh_offset_preindex(dest, tmp, -2);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldrh_offset(dest, tmp, 0);
            
            /* Perform calcualtion */
            if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                *ptr++ = sub_reg(tmp, tmp, src, LSL, 0);
            }
            else {
                *ptr++ = lsl(tmp, tmp, 16);
                *ptr++ = subs_reg(tmp, tmp, src, LSL, 16);
                *ptr++ = lsr(tmp, tmp, 16);
            }

            /* Store back */
            if (mode == 3)
            {
                *ptr++ = strh_offset_postindex(dest, tmp, 2);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strh_offset(dest, tmp, 0);
            break;
        
        case 1:
            if (mode == 4)
            {
                *ptr++ = ldrb_offset_preindex(dest, tmp, (opcode & 7) == 7 ? -2 : -1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = ldrb_offset(dest, tmp, 0);

            /* Perform calcualtion */
            if (update_mask == 0 || update_mask == SR_Z || update_mask == SR_N) {
                *ptr++ = sub_reg(tmp, tmp, src, LSL, 0);
            }
            else {
                *ptr++ = lsl(tmp, tmp, 24);
                *ptr++ = subs_reg(tmp, tmp, src, LSL, 24);
                *ptr++ = lsr(tmp, tmp, 24);
            }

            /* Store back */
            if (mode == 3)
            {
                *ptr++ = strb_offset_postindex(dest, tmp, (opcode & 7) == 7 ? 2 : 1);
                RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
            }
            else
                *ptr++ = strb_offset(dest, tmp, 0);
            break;
        }

        if (size < 4 && update_mask == SR_Z)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_ClearFlags(ptr, cc, SR_Z);

            switch (size) {
                case 2:
                    *ptr++ = ands_immed(31, tmp, 16, 0);
                    break;
                case 1:
                    *ptr++ = ands_immed(31, tmp, 8, 0);
                    break;
            }
            
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, A64_CC_EQ);
            update_mask = 0;
        }

        if (size < 4 && update_mask == SR_N)
        {
            uint8_t cc = RA_ModifyCC(&ptr);
            ptr = EMIT_ClearFlags(ptr, cc, SR_N);

            switch (size) {
                case 2:
                    *ptr++ = ands_immed(31, tmp, 1, 32-15);
                    break;
                case 1:
                    *ptr++ = ands_immed(31, tmp, 1, 32-7);
                    break;
            }
            
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, A64_CC_NE);
            update_mask = 0;
        }

        RA_FreeARMRegister(&ptr, dest);
        RA_FreeARMRegister(&ptr, tmp);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        if (update_mask & SR_X)
            ptr = EMIT_GetNZnCVX(ptr, cc, &update_mask);
        else
            ptr = EMIT_GetNZnCV(ptr, cc, &update_mask);

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
        if (update_mask & SR_V)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Valt, ARM_CC_VS);
        if (update_mask & (SR_X | SR_C)) {
            if ((update_mask & (SR_X | SR_C)) == SR_X)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_X, ARM_CC_CC);
            else if ((update_mask & (SR_X | SR_C)) == SR_C)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Calt, ARM_CC_CC);
            else
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Calt | SR_X, ARM_CC_CC);
        }
    }

    return ptr;
}

/****************************************************************************
* 1001regm11xxxxxx SUBA                                                     *
*****************************************************************************
* SUBA <ea>,An                                                              *
*                                                                           *
* Operation: dest - src → dest                                              *
*      X|N|Z|V|C                                                            *
* CC: (-|-|-|-|-)                                                           *
*                                                                           *
* Description: Subtracts the src operand from the dest operand and stores   *
* the result in the dest. The size of the operation is specified as word or *
* long. The mode of the instruction defines operand size, word operants are *
* sign extended to long.                                                    *
****************************************************************************/

uint32_t *EMIT_SUBA_ext(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_SUBA_reg")));
uint32_t *EMIT_SUBA_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_SUBA_reg")));
uint32_t *EMIT_SUBA_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t ext_words = 0;
    uint8_t size = (opcode & 0x0100) == 0x0100 ? 4 : 2;
    uint8_t reg = RA_MapM68kRegister(&ptr, ((opcode >> 9) & 7) + 8);
    uint8_t tmp = 0xff;
    uint8_t immed = (opcode & 0x3f) == 0x3c;

    RA_SetDirtyM68kRegister(&ptr, ((opcode >> 9) & 7) + 8);

    if (size == 2)
    {
        if (immed)
        {
            int16_t offset = (int16_t)cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);

            if (offset >= 0 && offset < 4096)
            {
                *ptr++ = sub_immed(reg, reg, offset);
                ext_words = 1;
            }
            else if (offset > -4096 && offset < 0)
            {
                *ptr++ = add_immed(reg, reg, -offset);
                ext_words = 1;
            }
            else immed = 0;
        }

        if (immed == 0)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, 0x80 | size, &tmp, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);
    }
    else
    {
        int32_t offset;
        if (immed)
        {
            offset = ((int16_t)cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]) << 16) | (uint16_t)cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[1]);
            
            if (offset >= 0 && offset < 4096)
            {
                *ptr++ = sub_immed(reg, reg, offset);
                ext_words = 2;
            }
            else if ((offset & 0xff000fff) == 0)
            {
                *ptr++ = sub_immed_lsl12(reg, reg, offset >> 12);
                ext_words = 2;
            }
            else
            {
                immed = 0;
            }
        }

        if (immed == 0)
            ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &tmp, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
    }
        

    if (immed == 0)
        *ptr++ = sub_reg(reg, reg, tmp, LSL, 0);

    RA_FreeARMRegister(&ptr, tmp);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    return ptr;
}

/****************************************************************************
* 1001reg1mm00mxxx SUBX                                                     *
*****************************************************************************
* SUBX Dx,Dy|-(Ax),-(Ay)                                                    *
*                                                                           *
* Operation: dest - src - X → dest                                          *
*      X|N|Z|V|C                                                            *
* CC: (*|*|*|*|*)                                                           *
*                                                                           *
* Description: Subtracts the src operand and the extended bit from the dest *
* operand and stores the result in the dest. The instructions has two modes.*
* The size of the operation is specified as byte, word or long. The mode of *
* the instruction indicates if it pertains a reg to reg or mem to mem       *
* operation, and which is the operand size.                                 *
****************************************************************************/

// BROKEN!!!!
uint32_t *EMIT_SUBX_ext(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_SUBX_reg")));
uint32_t *EMIT_SUBX_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_SUBX_reg")));
uint32_t *EMIT_SUBX_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t size = (opcode >> 6) & 3;
    /* Move negated C flag to ARM flags */

    uint8_t cc = RA_GetCC(&ptr);
    if (size == 2) {
        uint8_t tmp = RA_AllocARMRegister(&ptr);

        *ptr++ = mvn_reg(tmp, cc, ROR, 7);
        *ptr++ = set_nzcv(tmp);

        RA_FreeARMRegister(&ptr, tmp);
    } else {
        *ptr++ = tst_immed(cc, 1, 31 & (32 - SRB_X));
    }

    /* Register to register */
    if ((opcode & 0x0008) == 0)
    {
        uint8_t regx = RA_MapM68kRegister(&ptr, opcode & 7);
        uint8_t regy = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
        uint8_t tmp = 0xff;
        uint8_t tmp_2 = 0xff;

        RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

        switch (size)
        {
            case 0: /* Byte */
                tmp = RA_AllocARMRegister(&ptr);
                tmp_2 = RA_AllocARMRegister(&ptr);
                *ptr++ = and_immed(tmp, regx, 8, 0);
                *ptr++ = and_immed(tmp_2, regy, 8, 0);
                *ptr++ = sub_reg(tmp, tmp_2, tmp, LSL, 0);
                *ptr++ = b_cc(A64_CC_EQ, 2);
                *ptr++ = sub_immed(tmp, tmp, 1);

                if (update_mask & SR_XVC) {
                    uint8_t tmp_3 = RA_AllocARMRegister(&ptr);

                    *ptr++ = eor_reg(tmp_3, tmp_2, regx, LSL, 0); // D ^ S -> tmp_3
                    *ptr++ = eor_reg(tmp_2, tmp_2, tmp, LSL, 0);  // D ^ R -> tmp_2
                    *ptr++ = and_reg(tmp_3, tmp_2, tmp_3, LSL, 0); // V = (D^S) & (D^R), bit 7
                    *ptr++ = mov_reg(tmp_2, tmp);                // tmp_2 <-- tmp. C at position 8
                    *ptr++ = bfi(tmp_2, tmp_3, 0, 8);            // C at position 8, V at position 7
                    *ptr++ = bfxil(cc, tmp_2, 7, 2);

                    if (update_mask & SR_X) {
                        *ptr++ = ror(0, cc, 1);
                        *ptr++ = bfi(cc, 0, 4, 1);
                    }

                    RA_FreeARMRegister(&ptr, tmp_3);

                    update_mask &= ~SR_XVC;
                }

                if (update_mask & SR_NZ) {
                    *ptr++ = adds_reg(31, 31, tmp, LSL, 24);
                }

                *ptr++ = bfxil(regy, tmp, 0, 8);
                RA_FreeARMRegister(&ptr, tmp);
                RA_FreeARMRegister(&ptr, tmp_2);
                break;

            case 1: /* Word */
                tmp = RA_AllocARMRegister(&ptr);
                tmp_2 = RA_AllocARMRegister(&ptr);
                *ptr++ = and_immed(tmp, regx, 16, 0);
                *ptr++ = and_immed(tmp_2, regy, 16, 0);
                *ptr++ = sub_reg(tmp, tmp_2, tmp, LSL, 0);
                *ptr++ = b_cc(A64_CC_EQ, 2);
                *ptr++ = sub_immed(tmp, tmp, 1);

                if (update_mask & SR_XVC) {
                    uint8_t tmp_3 = RA_AllocARMRegister(&ptr);

                    *ptr++ = eor_reg(tmp_3, tmp_2, regx, LSL, 0); // D ^ S -> tmp_3
                    *ptr++ = eor_reg(tmp_2, tmp_2, tmp, LSL, 0);  // D ^ R -> tmp_2
                    *ptr++ = and_reg(tmp_3, tmp_2, tmp_3, LSL, 0); // V = (D^S) & (D^R), bit 15
                    *ptr++ = mov_reg(tmp_2, tmp);                 // tmp_2 <-- tmp. C at position 16
                    *ptr++ = bfi(tmp_2, tmp_3, 0, 16);            // C at position 16, V at position 15
                    *ptr++ = bfxil(cc, tmp_2, 15, 2);

                    if (update_mask & SR_X) {
                        *ptr++ = ror(0, cc, 1);
                        *ptr++ = bfi(cc, 0, 4, 1);
                    }

                    RA_FreeARMRegister(&ptr, tmp_3);

                    update_mask &= ~SR_XVC;
                }

                if (update_mask & SR_NZ) {
                    *ptr++ = adds_reg(31, 31, tmp, LSL, 16);
                }

                *ptr++ = bfxil(regy, tmp, 0, 16);
                RA_FreeARMRegister(&ptr, tmp);
                RA_FreeARMRegister(&ptr, tmp_2);
                break;

            case 2: /* Long */
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = sbcs(regy, regy, regx);
                RA_FreeARMRegister(&ptr, tmp);
                break;
        }
    }
    /* memory to memory */
    else
    {
        uint8_t regx = RA_MapM68kRegister(&ptr, 8 + (opcode & 7));
        uint8_t regy = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
        uint8_t dest = RA_AllocARMRegister(&ptr);
        uint8_t src = RA_AllocARMRegister(&ptr);
        uint8_t tmp = 0xff;

        RA_SetDirtyM68kRegister(&ptr, 8 + (opcode & 7));
        RA_SetDirtyM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));

        switch (size)
        {
            case 0: /* Byte */
                *ptr++ = ldrb_offset_preindex(regx, src, (opcode & 7) == 7 ? -2 : -1);
                *ptr++ = ldrb_offset_preindex(regy, dest, ((opcode >> 9) & 7) == 7 ? -2 : -1);

                tmp = RA_AllocARMRegister(&ptr);

                *ptr++ = sub_reg(tmp, dest, src, LSL, 0);
                *ptr++ = b_cc(A64_CC_EQ, 2);
                *ptr++ = sub_immed(tmp, tmp, 1);

                if (update_mask & SR_XVC) {
                    uint8_t tmp_3 = RA_AllocARMRegister(&ptr);
                    uint8_t tmp_2 = RA_AllocARMRegister(&ptr);

                    *ptr++ = eor_reg(tmp_3, dest, src, LSL, 0); // D ^ S -> tmp_3
                    *ptr++ = eor_reg(tmp_2, dest, tmp, LSL, 0);  // D ^ R -> tmp_2
                    *ptr++ = and_reg(tmp_3, tmp_2, tmp_3, LSL, 0); // V = (D^S) & (D^R), bit 7
                    *ptr++ = mov_reg(tmp_2, tmp);                // C at position 8
                    *ptr++ = bfi(tmp_2, tmp_3, 0, 8);            // C at position 8, V at position 7
                    *ptr++ = bfxil(cc, tmp_2, 7, 2);

                    if (update_mask & SR_X) {
                        *ptr++ = ror(0, cc, 1);
                        *ptr++ = bfi(cc, 0, 4, 1);
                    }

                    RA_FreeARMRegister(&ptr, tmp_3);
                    RA_FreeARMRegister(&ptr, tmp_2);

                    update_mask &= ~SR_XVC;
                }

                if (update_mask & SR_NZ) {
                    *ptr++ = adds_reg(31, 31, tmp, LSL, 24);
                }

                *ptr++ = strb_offset(regy, tmp, 0);
                RA_FreeARMRegister(&ptr, tmp);
                break;

            case 1: /* Word */
                *ptr++ = ldrh_offset_preindex(regx, src, -2);
                *ptr++ = ldrh_offset_preindex(regy, dest, -2);
                tmp = RA_AllocARMRegister(&ptr);

                *ptr++ = sub_reg(tmp, dest, src, LSL, 0);
                *ptr++ = b_cc(A64_CC_EQ, 2);
                *ptr++ = sub_immed(tmp, tmp, 1);

                if (update_mask & SR_XVC) {
                    uint8_t tmp_3 = RA_AllocARMRegister(&ptr);
                    uint8_t tmp_2 = RA_AllocARMRegister(&ptr);

                    *ptr++ = eor_reg(tmp_3, dest, src, LSL, 0); // D ^ S -> tmp_3
                    *ptr++ = eor_reg(tmp_2, dest, tmp, LSL, 0);  // D ^ R -> tmp_2
                    *ptr++ = and_reg(tmp_3, tmp_2, tmp_3, LSL, 0); // V = (D^S) & (D^R), bit 15
                    *ptr++ = mov_reg(tmp_2, tmp);                 // tmp_2 <-- tmp. C at position 16
                    *ptr++ = bfi(tmp_2, tmp_3, 0, 16);            // C at position 16, V at position 15
                    *ptr++ = bfxil(cc, tmp_2, 15, 2);

                    if (update_mask & SR_X) {
                        *ptr++ = ror(0, cc, 1);
                        *ptr++ = bfi(cc, 0, 4, 1);
                    }

                    RA_FreeARMRegister(&ptr, tmp_3);
                    RA_FreeARMRegister(&ptr, tmp_2);

                    update_mask &= ~SR_XVC;
                }

                if (update_mask & SR_NZ) {
                    *ptr++ = adds_reg(31, 31, tmp, LSL, 16);
                }

                *ptr++ = strh_offset(regy, tmp, 0);
                RA_FreeARMRegister(&ptr, tmp);
                break;

            case 2: /* Long */
                *ptr++ = ldr_offset_preindex(regx, src, -4);
                *ptr++ = ldr_offset_preindex(regy, dest, -4);

                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = sbcs(dest, dest, src);
                *ptr++ = str_offset(regy, dest, 0);
                RA_FreeARMRegister(&ptr, tmp);
                break;
        }

        RA_FreeARMRegister(&ptr, dest);
        RA_FreeARMRegister(&ptr, src);
    }

    ptr = EMIT_AdvancePC(ptr, 2);

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);

        /* If result is non-zero, clear Z flag, leave unchanged otherwise */
        if (update_mask & SR_Z) {
            *ptr++ = b_cc(ARM_CC_EQ, 2);
            *ptr++ = bic_immed(cc, cc, 1, 30);
            update_mask &= ~SR_Z;
        }
        
        uint8_t alt_flags = update_mask;
        if ((alt_flags & 3) != 0 && (alt_flags & 3) < 3)
            alt_flags ^= 3;
        ptr = EMIT_ClearFlags(ptr, cc, alt_flags);

        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
        if (update_mask & SR_V)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Valt, ARM_CC_VS);
        if (update_mask & (SR_X | SR_C)) {
            if ((update_mask & (SR_X | SR_C)) == SR_X)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_X, ARM_CC_CC);
            else if ((update_mask & (SR_X | SR_C)) == SR_C)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Calt, ARM_CC_CC);
            else
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Calt | SR_X, ARM_CC_CC);
        }
    }


    return ptr;
}

static struct OpcodeDef InsnTable[512] = {
	[0000 ... 0007] = { { EMIT_SUB_reg }, NULL, 0, SR_CCR, 1, 0, 1 },  //D0 Destination
	[0020 ... 0047] = { { EMIT_SUB_mem }, NULL, 0, SR_CCR, 1, 0, 1 },
	[0050 ... 0074] = { { EMIT_SUB_ext }, NULL, 0, SR_CCR, 1, 1, 1 },
	[0100 ... 0117] = { { EMIT_SUB_reg }, NULL, 0, SR_CCR, 1, 0, 2 },
	[0120 ... 0147] = { { EMIT_SUB_mem }, NULL, 0, SR_CCR, 1, 0, 2 },
	[0150 ... 0174] = { { EMIT_SUB_ext }, NULL, 0, SR_CCR, 1, 1, 2 },
	[0200 ... 0217] = { { EMIT_SUB_reg }, NULL, 0, SR_CCR, 1, 0, 4 },
	[0220 ... 0247] = { { EMIT_SUB_mem }, NULL, 0, SR_CCR, 1, 0, 4 },
	[0250 ... 0274] = { { EMIT_SUB_ext }, NULL, 0, SR_CCR, 1, 1, 4 },

	[0300 ... 0317] = { { EMIT_SUBA_reg }, NULL, 0, 0, 1, 0, 2 },
	[0320 ... 0347] = { { EMIT_SUBA_mem }, NULL, 0, 0, 1, 0, 2 },
	[0350 ... 0374] = { { EMIT_SUBA_ext }, NULL, 0, 0, 1, 1, 2 }, //Word

	[0400 ... 0407] = { { EMIT_SUBX_reg }, NULL, SR_XZ, SR_CCR, 1, 0, 1 },
	[0410 ... 0417] = { { EMIT_SUBX_mem }, NULL, SR_XZ, SR_CCR, 1, 0, 1 }, //R0
	[0500 ... 0507] = { { EMIT_SUBX_reg }, NULL, SR_XZ, SR_CCR, 1, 0, 2 },
	[0510 ... 0517] = { { EMIT_SUBX_mem }, NULL, SR_XZ, SR_CCR, 1, 0, 2 },
	[0600 ... 0607] = { { EMIT_SUBX_reg }, NULL, SR_XZ, SR_CCR, 1, 0, 4 },
	[0610 ... 0617] = { { EMIT_SUBX_mem }, NULL, SR_XZ, SR_CCR, 1, 0, 4 },

	[0420 ... 0447] = { { EMIT_SUB_mem }, NULL, 0, SR_CCR, 1, 0, 1 },
	[0450 ... 0471] = { { EMIT_SUB_ext }, NULL, 0, SR_CCR, 1, 1, 1 },  //D0 Source
	[0520 ... 0547] = { { EMIT_SUB_mem }, NULL, 0, SR_CCR, 1, 0, 2 },
	[0550 ... 0571] = { { EMIT_SUB_ext }, NULL, 0, SR_CCR, 1, 1, 2 },
	[0620 ... 0647] = { { EMIT_SUB_mem }, NULL, 0, SR_CCR, 1, 0, 4 },
	[0650 ... 0671] = { { EMIT_SUB_ext }, NULL, 0, SR_CCR, 1, 1, 4 },
	
	[0700 ... 0717] = { { EMIT_SUBA_reg }, NULL, 0, 0, 1, 0, 4 },
	[0720 ... 0747] = { { EMIT_SUBA_mem }, NULL, 0, 0, 1, 0, 4 },
	[0750 ... 0774] = { { EMIT_SUBA_ext }, NULL, 0, 0, 1, 1, 4 }, //Long
};

uint32_t *EMIT_line9(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    (*m68k_ptr)++;
    *insn_consumed = 1;

    if (InsnTable[opcode & 0x1ff].od_Emit) {
        ptr = InsnTable[opcode & 0x1ff].od_Emit(ptr, opcode, m68k_ptr);
    }
    else
    {
        ptr = EMIT_FlushPC(ptr);
        ptr = EMIT_InjectDebugString(ptr, "[JIT] opcode %04x at %08x not implemented\n", opcode, *m68k_ptr - 1);
        *ptr++ = svc(0x100);
        *ptr++ = svc(0x101);
        *ptr++ = svc(0x103);
        *ptr++ = (uint32_t)(uintptr_t)(*m68k_ptr - 8);
        *ptr++ = 48;
        ptr = EMIT_Exception(ptr, VECTOR_ILLEGAL_INSTRUCTION, 0);
        *ptr++ = INSN_TO_LE(0xffffffff);
    }

    return ptr;
}

uint32_t GetSR_Line9(uint16_t opcode)
{
    /* If instruction is in the table, return what flags it needs (shifted 16 bits left) and flags it sets */
    if (InsnTable[opcode & 0x1ff].od_Emit) {
        return (InsnTable[opcode & 0x1ff].od_SRNeeds << 16) | InsnTable[opcode & 0x1ff].od_SRSets;
    }
    /* Instruction not found, i.e. it needs all flags and sets none (ILLEGAL INSTRUCTION exception) */
    else {
        kprintf("Undefined Line9\n");
        return SR_CCR << 16;
    }
}

int M68K_GetLine9Length(uint16_t *insn_stream)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)insn_stream);
    
    int length = 0;
    int need_ea = 0;
    int opsize = 0;

    if (InsnTable[opcode & 0x1ff].od_Emit) {
        length = InsnTable[opcode & 0x1ff].od_BaseLength;
        need_ea = InsnTable[opcode & 0x1ff].od_HasEA;
        opsize = InsnTable[opcode & 0x1ff].od_OpSize;
    }

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    return length;
}