/* RIVET — gdt.h : x86_64 GDT + TSS structures and load helpers.
 *
 * In long mode the GDT only matters for code/data segment selectors,
 * but a valid TSS is still required to deliver privilege-level switches
 * and to host the IST table. Typical setup needs five entries: NULL,
 * kernel code, kernel data, user code, user data, plus the TSS (which
 * occupies two GDT slots since its descriptor is 16 bytes wide). */
#ifndef RIVET_GDT_H
#define RIVET_GDT_H

#include "core.h"

#if RIVET_ARCH_X86_64

/* 8-byte segment descriptor (used for null/code/data slots). */
typedef struct RIV_PACKED riv_gdt_entry {
    riv_u16 limit_low;
    riv_u16 base_low;
    riv_u8  base_mid;
    riv_u8  access;        /* P|DPL|S|TYPE */
    riv_u8  granularity;   /* G|D/B|L|AVL | limit_hi[19:16] */
    riv_u8  base_high;
} riv_gdt_entry;

/* 16-byte TSS descriptor occupies two consecutive GDT slots. */
typedef struct RIV_PACKED riv_tss_descriptor {
    riv_u16 limit_low;
    riv_u16 base_low;
    riv_u8  base_mid;
    riv_u8  access;
    riv_u8  granularity;
    riv_u8  base_high;
    riv_u32 base_upper;
    riv_u32 reserved;
} riv_tss_descriptor;

typedef struct RIV_PACKED riv_gdtr {
    riv_u16 limit;
    riv_u64 base;
} riv_gdtr;

/* Long-mode 64-bit TSS layout. */
typedef struct RIV_PACKED riv_tss64 {
    riv_u32 reserved0;
    riv_u64 rsp[3];        /* RSP0..RSP2 for ring 0..2 entry */
    riv_u64 reserved1;
    riv_u64 ist[7];        /* IST1..IST7 */
    riv_u64 reserved2;
    riv_u16 reserved3;
    riv_u16 iomap_base;
} riv_tss64;

/* Access-byte recipes for the common slot kinds. */
#define RIV_GDT_KCODE   0x9A    /* present, DPL=0, code, executable, readable */
#define RIV_GDT_KDATA   0x92    /* present, DPL=0, data, writable */
#define RIV_GDT_UCODE   0xFA    /* present, DPL=3, code, executable, readable */
#define RIV_GDT_UDATA   0xF2    /* present, DPL=3, data, writable */
#define RIV_GDT_TSS     0x89    /* present, DPL=0, system, 64-bit TSS */
#define RIV_GDT_L       0x20    /* long-mode code segment flag */
#define RIV_GDT_DB      0x40    /* 32-bit data flag */
#define RIV_GDT_G       0x80    /* page granularity */

RIV_ALWAYS void riv_gdt_set(riv_gdt_entry *e, riv_u32 base, riv_u32 limit,
                            riv_u8 access, riv_u8 flags) {
    e->limit_low   = (riv_u16)(limit & 0xFFFF);
    e->base_low    = (riv_u16)(base & 0xFFFF);
    e->base_mid    = (riv_u8)((base >> 16) & 0xFF);
    e->access      = access;
    e->granularity = (riv_u8)(((limit >> 16) & 0x0F) | (flags & 0xF0));
    e->base_high   = (riv_u8)((base >> 24) & 0xFF);
}

RIV_ALWAYS void riv_gdt_set_tss(riv_tss_descriptor *e, riv_uptr base,
                                 riv_u32 limit) {
    e->limit_low   = (riv_u16)(limit & 0xFFFF);
    e->base_low    = (riv_u16)(base & 0xFFFF);
    e->base_mid    = (riv_u8)((base >> 16) & 0xFF);
    e->access      = RIV_GDT_TSS;
    e->granularity = (riv_u8)((limit >> 16) & 0x0F);
    e->base_high   = (riv_u8)((base >> 24) & 0xFF);
    e->base_upper  = (riv_u32)(base >> 32);
    e->reserved    = 0;
}

RIV_ALWAYS void riv_gdt_load(void *gdt_base, riv_u16 limit) {
    riv_gdtr d;
    d.limit = limit;
    d.base  = (riv_u64)(riv_uptr)gdt_base;
    __asm__ __volatile__("lgdt %0" :: "m"(d));
}

RIV_ALWAYS void riv_tss_load(riv_u16 selector) {
    __asm__ __volatile__("ltr %0" :: "r"(selector));
}

#endif /* RIVET_ARCH_X86_64 */

#endif /* RIVET_GDT_H */
