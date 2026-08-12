/* RIVET — cpu.h : CPU identification and feature detection.
 *
 * Provides per-arch query helpers:
 *   x86_64   : CPUID leaves 0/1, vendor string, feature bits.
 *   aarch64  : MIDR_EL1 / MPIDR_EL1.
 *   arm32    : MIDR via CP15.
 *   riscv    : misa, mvendorid, marchid, mimpid (M-mode only). */
#ifndef RIVET_CPU_H
#define RIVET_CPU_H

#include "core.h"

#if RIVET_ARCH_X86_64 || RIVET_ARCH_X86

RIV_ALWAYS void riv_cpuid(riv_u32 leaf, riv_u32 *a, riv_u32 *b,
                          riv_u32 *c, riv_u32 *d) {
    __asm__ __volatile__("cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(leaf), "c"(0));
}

/* Writes 12-byte vendor string ("GenuineIntel", "AuthenticAMD", ...) +
 * NUL into `out` (must be >= 13 bytes). */
RIV_ALWAYS void riv_cpu_vendor(char out[13]) {
    riv_u32 a, b, c, d;
    riv_cpuid(0, &a, &b, &c, &d);
    riv_u32 w[3] = { b, d, c };
    for (int i = 0; i < 12; ++i) out[i] = ((char*)w)[i];
    out[12] = 0;
}

RIV_ALWAYS riv_u32 riv_cpu_features_edx(void) {
    riv_u32 a, b, c, d;
    riv_cpuid(1, &a, &b, &c, &d);
    return d;
}
RIV_ALWAYS riv_u32 riv_cpu_features_ecx(void) {
    riv_u32 a, b, c, d;
    riv_cpuid(1, &a, &b, &c, &d);
    return c;
}

#elif RIVET_ARCH_AARCH64
RIV_ALWAYS riv_u64 riv_cpu_midr(void) {
    riv_u64 v;
    __asm__ __volatile__("mrs %0, midr_el1" : "=r"(v));
    return v;
}
RIV_ALWAYS riv_u64 riv_cpu_mpidr(void) {
    riv_u64 v;
    __asm__ __volatile__("mrs %0, mpidr_el1" : "=r"(v));
    return v;
}
#elif RIVET_ARCH_ARM
RIV_ALWAYS riv_u32 riv_cpu_midr(void) {
    riv_u32 v;
    __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 0" : "=r"(v));
    return v;
}
#elif RIVET_ARCH_RISCV
RIV_ALWAYS riv_u32 riv_cpu_misa(void) {
    riv_u32 v;
    __asm__ __volatile__("csrr %0, misa" : "=r"(v));
    return v;
}
RIV_ALWAYS riv_u32 riv_cpu_vendorid(void) {
    riv_u32 v;
    __asm__ __volatile__("csrr %0, mvendorid" : "=r"(v));
    return v;
}
#endif

#endif /* RIVET_CPU_H */
