#include "mmu030.h"
#include <string.h>

static EmuMMU g_mmu;
static bool g_enabled = false;

bool emu_mmu_enabled(void){ return g_enabled; }
void emu_mmu_set_enabled(bool en){ g_enabled = en; }

void emu_mmu_reset(EmuMMU *m){
    memset(&g_mmu, 0, sizeof(g_mmu));
    (void)m;
}

static inline uint32_t page_index(uint32_t la, uint32_t mask){
    // simplistic direct-mapped index by page number
    return (la >> 12) & mask;
}

EmuTLBEntry* emu_tlb_lookup(EmuTLB *tlb, uint32_t la){
    if (!tlb || !tlb->entries) return NULL;
    EmuTLBEntry *e = &tlb->entries[ page_index(la, tlb->mask) ];
    // very naive tag check for scaffold
    if ((e->tag ^ (la & ~0xFFFu)) == 0) return e;
    return NULL;
}

EmuTLBEntry* emu_tlb_fill(EmuMMU *m, bool is_instr, uint32_t la, bool is_write, bool super, bool *trap){
    (void)m; (void)is_instr; (void)is_write; (void)super;
    if (trap) *trap = false;
    return NULL; // no mapping yet – force trap/slow-path in caller
}

// Slow-path load/store – currently identity map for scaffold.
uint8_t  emu_mmu_ld8 (uint32_t la, bool is_instr, bool super, bool *trap){ (void)is_instr; (void)super; if (trap) *trap=false; return *((volatile uint8_t*) (uintptr_t) la); }
uint16_t emu_mmu_ld16(uint32_t la, bool is_instr, bool super, bool *trap){ (void)is_instr; (void)super; if (trap) *trap=false; return *((volatile uint16_t*)(uintptr_t) la); }
uint32_t emu_mmu_ld32(uint32_t la, bool is_instr, bool super, bool *trap){ (void)is_instr; (void)super; if (trap) *trap=false; return *((volatile uint32_t*)(uintptr_t) la); }
void     emu_mmu_st8 (uint32_t la, uint8_t  v, bool super, bool *trap){ (void)super; if (trap) *trap=false; *((volatile uint8_t*) (uintptr_t) la) = v; }
void     emu_mmu_st16(uint32_t la, uint16_t v, bool super, bool *trap){ (void)super; if (trap) *trap=false; *((volatile uint16_t*)(uintptr_t) la) = v; }
void     emu_mmu_st32(uint32_t la, uint32_t v, bool super, bool *trap){ (void)super; if (trap) *trap=false; *((volatile uint32_t*)(uintptr_t) la) = v; }
