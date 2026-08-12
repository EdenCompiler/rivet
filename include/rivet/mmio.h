/* RIVET — mmio.h : memory-mapped I/O, register fields */
#ifndef RIVET_MMIO_H
#define RIVET_MMIO_H

#include "core.h"
#include "mem.h"

/* volatile MMIO accessors — never optimized away, never reordered */
RIV_ALWAYS riv_u8  riv_mmio_r8 (riv_uptr a) { return *(volatile riv_u8 *)a; }
RIV_ALWAYS riv_u16 riv_mmio_r16(riv_uptr a) { return *(volatile riv_u16*)a; }
RIV_ALWAYS riv_u32 riv_mmio_r32(riv_uptr a) { return *(volatile riv_u32*)a; }
RIV_ALWAYS riv_u64 riv_mmio_r64(riv_uptr a) { return *(volatile riv_u64*)a; }

RIV_ALWAYS void riv_mmio_w8 (riv_uptr a, riv_u8  v) { *(volatile riv_u8 *)a = v; }
RIV_ALWAYS void riv_mmio_w16(riv_uptr a, riv_u16 v) { *(volatile riv_u16*)a = v; }
RIV_ALWAYS void riv_mmio_w32(riv_uptr a, riv_u32 v) { *(volatile riv_u32*)a = v; }
RIV_ALWAYS void riv_mmio_w64(riv_uptr a, riv_u64 v) { *(volatile riv_u64*)a = v; }

/* read-modify-write with mask */
RIV_ALWAYS void riv_mmio_rmw32(riv_uptr a, riv_u32 mask, riv_u32 val) {
    riv_u32 v = riv_mmio_r32(a);
    v = (v & ~mask) | (val & mask);
    riv_mmio_w32(a, v);
}

RIV_ALWAYS void riv_mmio_setbits32  (riv_uptr a, riv_u32 m) { riv_mmio_w32(a, riv_mmio_r32(a) |  m); }
RIV_ALWAYS void riv_mmio_clearbits32(riv_uptr a, riv_u32 m) { riv_mmio_w32(a, riv_mmio_r32(a) & ~m); }

/* DSL: declare an MMIO register block */
#define mmio_block(name, base) \
    static volatile riv_u32 * const name = (volatile riv_u32*)(base)

/* DSL: field extract/insert */
#define riv_field(reg, hi, lo)         (((reg) >> (lo)) & ((1u << ((hi)-(lo)+1)) - 1))
#define riv_field_set(val, hi, lo)     (((val) & ((1u << ((hi)-(lo)+1)) - 1)) << (lo))

/* register-bank record type — struct of volatile u32s, then cast base */
#define mmio_bank(typename, ...) \
    typedef volatile struct typename { __VA_ARGS__ } typename; \
    RIV_STATIC_ASSERT(sizeof(typename) % 4 == 0, typename##_aligned)

#endif /* RIVET_MMIO_H */
