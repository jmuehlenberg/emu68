# EMU68 MMU (68030) – Scaffold

Dies ist ein erster Wurf für eine integrierte Soft‑MMU (68030‑ähnlich) in Emu68.
Aktuell enthalten:
- Header `include/mmu030.h`
- Grundgerüst `src/mmu/mmu030.c` (TLB‑Stubs, Identity‑Mapping)
- Patch in `src/M68k_LINE4.c`, der das Erzwingen von MMU=off beim Schreiben nach TCR entfernt
- CMake‑Erweiterung, um die neuen Dateien zu bauen

**Nächste Schritte (v0.1 → v0.2):**
1. State‑Bindung: Spiegelung von TCR/SRP/MMUSR/TT0..TT1 aus `struct M68KState` nach `EmuMMU`.
2. TLB: Direct‑Mapped 1 Ki Einträge, getrennt I/D, einfache LRU/Clock.
3. Pagewalk: 68030‑Deskriptoren, Setzen von A/M‑Bits, Fault‑Frames.
4. JIT‑Callouts: Für Mem‑Miss/Trap `emu_mmu_ld*/st*` verwenden (Slow‑Path). Fast‑Path folgt.
5. `PTEST/PLOAD/MOVEC/PMOVE`: Semantik vervollständigen und TLB‑Prefill.
6. Transparente Translation (ITT0/ITT1/DTT0/DTT1) für I/O/Chip‑RAM.

Siehe auch: Kommentare in `include/mmu030.h`.
