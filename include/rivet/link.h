/* RIVET — link.h : linker section / symbol helpers */
#ifndef RIVET_LINK_H
#define RIVET_LINK_H

#include "core.h"

/* declare extern linker symbol (address-only) */
#define riv_link_sym(name)    extern char name[]
#define riv_link_addr(name)   ((riv_uptr)(name))

/* place a thing in a named section */
#define in_section(s)        RIV_USED RIV_SECTION(s)

/* common vector / boot sections */
#define vectors_section      in_section(".vectors")
#define boot_section         in_section(".text.boot")
#define entry_section        in_section(".text.entry")
#define rodata_keep          in_section(".rodata.keep")

/* zero the BSS region between two linker symbols */
RIV_ALWAYS void riv_zero_range(riv_uptr begin, riv_uptr end) {
    volatile riv_u8 *p = (volatile riv_u8*)begin;
    while ((riv_uptr)p < end) *p++ = 0;
}

/* copy initialized data from LMA -> VMA */
RIV_ALWAYS void riv_copy_range(riv_uptr dst, riv_uptr src, riv_uptr end) {
    volatile riv_u8 *d = (volatile riv_u8*)dst;
    const volatile riv_u8 *s = (const volatile riv_u8*)src;
    while ((riv_uptr)d < end) *d++ = *s++;
}

#endif /* RIVET_LINK_H */
