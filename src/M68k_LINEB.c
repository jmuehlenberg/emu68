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

static uint32_t *EMIT_CMPA(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_CMPA_reg")));
static uint32_t *EMIT_CMPA_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_CMPA_mem")));
static uint32_t *EMIT_CMPA_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_CMPA_ext")));
static uint32_t *EMIT_CMPA_ext(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
 {
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t size = ((opcode >> 8) & 1) ? 4 : 2;
    uint8_t src = 0xff;
    uint8_t dst = RA_MapM68kRegister(&ptr, 8 + ((opcode >> 9) & 7));
    uint8_t ext_words = 0;

    if (size == 4)
        ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
    else
        ptr = EMIT_LoadFromEffectiveAddress(ptr, 0x80 | size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

    *ptr++ = cmp_reg(dst, src, LSL, 0);

    RA_FreeARMRegister(&ptr, src);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        
        if (__builtin_popcount(update_mask) != 0)
        {
            ptr = EMIT_GetNZnCV(ptr, cc, &update_mask);
            
            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Valt, ARM_CC_VS);
            if (update_mask & SR_C)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Calt, ARM_CC_CC);
        }
    }

    return ptr;
}


static uint32_t *EMIT_CMPM(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t src = 0xff;
    uint8_t dst = 0xff;
    uint8_t ext_words = 0;
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, 0x18 | (opcode & 7), *m68k_ptr, &ext_words, 1, NULL);
    ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &dst, 0x18 | ((opcode >> 9) & 7), *m68k_ptr, &ext_words, 1, NULL);

    switch (size)
    {
        case 4:
            *ptr++ = subs_reg(tmp, dst, src, LSL, 0);
            break;
        case 2:
            *ptr++ = lsl(tmp, dst, 16);
            *ptr++ = subs_reg(tmp, tmp, src, LSL, 16);
            break;
        case 1:
            *ptr++ = lsl(tmp, dst, 24);
            *ptr++ = subs_reg(tmp, tmp, src, LSL, 24);
            break;
    }

    RA_FreeARMRegister(&ptr, tmp);
    RA_FreeARMRegister(&ptr, src);
    RA_FreeARMRegister(&ptr, dst);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        
        if (__builtin_popcount(update_mask) != 0)
        {
            ptr = EMIT_GetNZnCV(ptr, cc, &update_mask);
            
            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Valt, ARM_CC_VS);
            if (update_mask & SR_C)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Calt, ARM_CC_CC);
        }
    }

    return ptr;
}


static uint32_t *EMIT_CMP(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_CMP_reg")));
static uint32_t *EMIT_CMP_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_CMP_mem")));
static uint32_t *EMIT_CMP_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_CMP_ext")));
static uint32_t *EMIT_CMP_ext(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t src = 0xff;
    uint8_t dst = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    uint8_t ext_words = 0;
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    ptr = EMIT_LoadFromEffectiveAddress(ptr, size, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

    switch(size)
    {
        case 4:
            *ptr++ = subs_reg(31, dst, src, LSL, 0);
            break;
        case 2:
            *ptr++ = lsl(tmp, dst, 16);
            *ptr++ = subs_reg(31, tmp, src, LSL, 16);
            break;
        case 1:
            *ptr++ = lsl(tmp, dst, 24);
            *ptr++ = subs_reg(31, tmp, src, LSL, 24);
            break;
    }

    RA_FreeARMRegister(&ptr, tmp);
    RA_FreeARMRegister(&ptr, src);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);

        if (__builtin_popcount(update_mask) != 0)
        {
            ptr = EMIT_GetNZnCV(ptr, cc, &update_mask);

            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_V)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Valt, ARM_CC_VS);
            if (update_mask & SR_C)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Calt, ARM_CC_CC);
        }
    }

    return ptr;
}


static uint32_t *EMIT_EOR(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_EOR_reg")));
static uint32_t *EMIT_EOR_reg(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_EOR_mem")));
static uint32_t *EMIT_EOR_mem(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
__attribute__((alias("EMIT_EOR_ext")));
static uint32_t *EMIT_EOR_ext(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t size = 1 << ((opcode >> 6) & 3);
    uint8_t ext_words = 0;
    uint8_t test_register;

    if ((opcode & 0x38) == 0)
    {
        uint8_t src = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
        uint8_t dest = RA_MapM68kRegister(&ptr, (opcode) & 7);
        uint8_t tmp = 0xff;

        test_register = dest;

        RA_SetDirtyM68kRegister(&ptr, opcode & 7);

        switch (size)
        {
            case 4:
                *ptr++ = eor_reg(dest, dest, src, LSL, 0);
                break;
            case 2:
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_reg(tmp, src, dest, LSL, 0);
                *ptr++ = bfi(dest, tmp, 0, 16);
                RA_FreeARMRegister(&ptr, tmp);
                break;
            case 1:
                tmp = RA_AllocARMRegister(&ptr);
                *ptr++ = eor_reg(tmp, src, dest, LSL, 0);
                *ptr++ = bfi(dest, tmp, 0, 8);
                RA_FreeARMRegister(&ptr, tmp);
                break;
        }

        RA_FreeARMRegister(&ptr, src);
    }
    else
    {
        uint8_t dest = 0xff;
        uint8_t src = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t mode = (opcode & 0x0038) >> 3;

        test_register = tmp;

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
            *ptr++ = eor_reg(tmp, tmp, src, LSL, 0);

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
            *ptr++ = eor_reg(tmp, tmp, src, LSL, 0);

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
            *ptr++ = eor_reg(tmp, tmp, src, LSL, 0);

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

        RA_FreeARMRegister(&ptr, dest);
    }

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        switch(size)
        {
            case 4:
                *ptr++ = cmn_reg(31, test_register, LSL, 0);
                break;
            case 2:
                *ptr++ = cmn_reg(31, test_register, LSL, 16);
                break;
            case 1:
                *ptr++ = cmn_reg(31, test_register, LSL, 24);
                break;
        }
        
        uint8_t cc = RA_ModifyCC(&ptr);
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);

        if (update_mask & SR_Z)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        if (update_mask & SR_N)
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
    }
    RA_FreeARMRegister(&ptr, test_register);

    return ptr;
}

static struct OpcodeDef InsnTable[512] = {
    [0000 ... 0007] = { { EMIT_CMP_reg }, NULL, 0, SR_NZVC, 1, 0, 1 }, //D0 destination, Byte
    [0020 ... 0047] = { { EMIT_CMP_mem }, NULL, 0, SR_NZVC, 1, 0, 1 }, //(An)
    [0050 ... 0074] = { { EMIT_CMP_ext }, NULL, 0, SR_NZVC, 1, 1, 1 }, //memory indirect
    [0100 ... 0117] = { { EMIT_CMP_reg }, NULL, 0, SR_NZVC, 1, 0, 2 }, //register, Word
    [0120 ... 0147] = { { EMIT_CMP_mem }, NULL, 0, SR_NZVC, 1, 0, 2 }, //(An)
    [0150 ... 0174] = { { EMIT_CMP_ext }, NULL, 0, SR_NZVC, 1, 1, 2 }, //memory indirect
    [0200 ... 0217] = { { EMIT_CMP_reg }, NULL, 0, SR_NZVC, 1, 0, 4 }, //register Long
    [0220 ... 0247] = { { EMIT_CMP_mem }, NULL, 0, SR_NZVC, 1, 0, 4 }, //(An)
    [0250 ... 0274] = { { EMIT_CMP_ext }, NULL, 0, SR_NZVC, 1, 1, 4 }, //memory indirect

    [0300 ... 0317] = { { EMIT_CMPA_reg }, NULL, 0, SR_NZVC, 1, 0, 2 }, //A0, Word
    [0320 ... 0347] = { { EMIT_CMPA_mem }, NULL, 0, SR_NZVC, 1, 0, 2 }, //(An)
    [0350 ... 0374] = { { EMIT_CMPA_ext }, NULL, 0, SR_NZVC, 1, 1, 2 }, //memory indirect
 
    [0400 ... 0407] = { { EMIT_EOR_reg }, NULL, 0, SR_NZVC, 1, 0, 1 }, //D0, Byte
    [0410 ... 0417] = { { EMIT_CMPM }, NULL, 0, SR_NZVC, 1, 0, 1 },
    [0420 ... 0447] = { { EMIT_EOR_mem }, NULL, 0, SR_NZVC, 1, 0, 1 },
    [0450 ... 0471] = { { EMIT_EOR_ext }, NULL, 0, SR_NZVC, 1, 1, 1 },
    [0500 ... 0507] = { { EMIT_EOR_reg }, NULL, 0, SR_NZVC, 1, 0, 2 }, //D0, Word
    [0510 ... 0517] = { { EMIT_CMPM }, NULL, 0, SR_NZVC, 1, 0, 2 },
    [0520 ... 0547] = { { EMIT_EOR_mem }, NULL, 0, SR_NZVC, 1, 0, 2 },
    [0550 ... 0571] = { { EMIT_EOR_ext }, NULL, 0, SR_NZVC, 1, 1, 2 },
    [0600 ... 0607] = { { EMIT_EOR_reg }, NULL, 0, SR_NZVC, 1, 0, 4 }, //D0, Long
    [0610 ... 0617] = { { EMIT_CMPM }, NULL, 0, SR_NZVC, 1, 0, 4 },
    [0620 ... 0647] = { { EMIT_EOR_mem }, NULL, 0, SR_NZVC, 1, 0, 4 },
    [0650 ... 0671] = { { EMIT_EOR_ext }, NULL, 0, SR_NZVC, 1, 1, 4 },

    [0700 ... 0717] = { { EMIT_CMPA_reg }, NULL, 0, SR_NZVC, 1, 0, 4 }, //A0, Long
    [0720 ... 0747] = { { EMIT_CMPA_mem }, NULL, 0, SR_NZVC, 1, 0, 4 }, //(An)
    [0750 ... 0774] = { { EMIT_CMPA_ext }, NULL, 0, SR_NZVC, 1, 1, 4 }, //memory indirect
};

uint32_t *EMIT_lineB(uint32_t *ptr, uint16_t **m68k_ptr, uint16_t *insn_consumed)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)&(*m68k_ptr)[0]);
    (*m68k_ptr)++;
    *insn_consumed = 1;

    /* 1011xxxx11xxxxxx - CMPA */
    if (InsnTable[opcode & 00777].od_Emit)
    {
        ptr = InsnTable[opcode & 00777].od_Emit(ptr, opcode, m68k_ptr);
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

uint32_t GetSR_LineB(uint16_t opcode)
{
    /* If instruction is in the table, return what flags it needs (shifted 16 bits left) and flags it sets */
    if (InsnTable[opcode & 00777].od_Emit) {
        return (InsnTable[opcode & 00777].od_SRNeeds << 16) | InsnTable[opcode & 00777].od_SRSets;
    }
    /* Instruction not found, i.e. it needs all flags and sets none (ILLEGAL INSTRUCTION exception) */
    else {
        kprintf("Undefined LineB\n");
        return SR_CCR << 16;
    }
}


int M68K_GetLineBLength(uint16_t *insn_stream)
{
    uint16_t opcode = cache_read_16(ICACHE, (uintptr_t)insn_stream);
    
    int length = 0;
    int need_ea = 0;
    int opsize = 0;

    if (InsnTable[opcode & 00777].od_Emit) {
        length = InsnTable[opcode & 00777].od_BaseLength;
        need_ea = InsnTable[opcode & 00777].od_HasEA;
        opsize = InsnTable[opcode & 00777].od_OpSize;
    }

    if (need_ea) {
        length += SR_GetEALength(&insn_stream[length], opcode & 0x3f, opsize);
    }

    return length;
}