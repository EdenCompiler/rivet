/* RIVET — boot.h : C runtime initialization helpers.
 * Linker must supply symbols __bss_start, __bss_end, __data_start,
 * __data_end, __data_load (LMA) for the helpers to be useful. */
#ifndef RIVET_BOOT_H
#define RIVET_BOOT_H

#include "core.h"
#include "link.h"
#include "mem.h"

typedef void (*riv_ctor_fn)(void);

/* Standard symbols expected from the linker script. */
extern char __bss_start[], __bss_end[];
extern char __data_start[], __data_end[], __data_load[];
extern riv_ctor_fn __init_array_start[], __init_array_end[];

/* Zero the .bss region. */
RIV_ALWAYS void riv_boot_clear_bss(void) {
    riv_zero_range((riv_uptr)__bss_start, (riv_uptr)__bss_end);
}
/* Copy .data from its load address to its run address. */
RIV_ALWAYS void riv_boot_copy_data(void) {
    riv_copy_range((riv_uptr)__data_start,
                   (riv_uptr)__data_load,
                   (riv_uptr)__data_end);
}
/* Run C++ / GCC-style static constructors. */
RIV_ALWAYS void riv_boot_run_ctors(void) {
    riv_ctor_fn *p = __init_array_start;
    while (p < __init_array_end) { (*p++)(); }
}

/* Full standard init sequence — call from kernel_entry before main. */
RIV_ALWAYS void riv_boot_runtime_init(void) {
    riv_boot_copy_data();
    riv_boot_clear_bss();
    riv_boot_run_ctors();
}

#endif /* RIVET_BOOT_H */
