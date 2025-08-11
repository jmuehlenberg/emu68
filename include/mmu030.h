#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Minimal 68030-like MMU interface (soft-MMU).
// NOTE: This is a scaffold. Real translation logic TBD.

typedef struct {
    uint32_t tag;      // logical page tag incl. AS & S/U
    uint32_t pa;       // physical page base
    uint16_t flags;    // bit0 R, bit1 W, bit2 X, bit3 A, bit4 M, bit5 S
} EmuTLBEntry;

typedef struct {
    EmuTLBEntry *entries;
    uint32_t mask; // size-1, power of two
} EmuTLB;

typedef struct {
    EmuTLB itlb;
    EmuTLB dtlb;
    uint32_t tcr;     // mirrors M68KState.TCR
    uint32_t srp;     // mirrors M68KState.SRP
    uint32_t mmusr;   // mirrors M68KState.MMUSR
    uint32_t itt0, itt1, dtt0, dtt1; // transparent translations
} EmuMMU;

bool emu_mmu_enabled(void);
void emu_mmu_set_enabled(bool en);

void emu_mmu_reset(EmuMMU *m);
EmuTLBEntry* emu_tlb_lookup(EmuTLB *tlb, uint32_t la);
EmuTLBEntry* emu_tlb_fill(EmuMMU *m, bool is_instr, uint32_t la, bool is_write, bool super, bool *trap);

// Slow-path helpers for loads/stores used by JIT callouts.
uint8_t  emu_mmu_ld8 (uint32_t la, bool is_instr, bool super, bool *trap);
uint16_t emu_mmu_ld16(uint32_t la, bool is_instr, bool super, bool *trap);
uint32_t emu_mmu_ld32(uint32_t la, bool is_instr, bool super, bool *trap);
void     emu_mmu_st8 (uint32_t la, uint8_t  v, bool super, bool *trap);
void     emu_mmu_st16(uint32_t la, uint16_t v, bool super, bool *trap);
void     emu_mmu_st32(uint32_t la, uint32_t v, bool super, bool *trap);

#ifdef __cplusplus
}
#endif
