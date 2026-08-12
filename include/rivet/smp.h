/* RIVET — smp.h : multi-core helpers + per-CPU storage.
 *
 * Provides:
 *   - riv_cpu_id()              : current logical CPU index
 *   - riv_smp_count() / _set    : declared core count
 *   - per-CPU statically-sized arrays via RIV_PERCPU(type, name)
 *
 * The actual SMP startup IPI/INIT sequence is arch-specific and lives
 * in the caller's boot code; this header just gives the data
 * abstractions that arch-neutral kernel code needs. */
#ifndef RIVET_SMP_H
#define RIVET_SMP_H

#include "core.h"

#define RIV_MAX_CPUS 16

extern riv_u32 riv_smp_ncpu;
#define RIV_SMP_DECLARE() riv_u32 riv_smp_ncpu = 1

RIV_ALWAYS void   riv_smp_set_count(riv_u32 n) { riv_smp_ncpu = n; }
RIV_ALWAYS riv_u32 riv_smp_count(void)         { return riv_smp_ncpu; }

#if RIVET_ARCH_X86_64
RIV_ALWAYS riv_u32 riv_cpu_id(void) {
    riv_u32 a, b, c, d;
    __asm__ __volatile__("cpuid"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "a"(1));
    return (b >> 24) & 0xFF;     /* initial APIC ID */
}
#elif RIVET_ARCH_AARCH64
RIV_ALWAYS riv_u32 riv_cpu_id(void) {
    riv_u64 v;
    __asm__ __volatile__("mrs %0, mpidr_el1" : "=r"(v));
    return (riv_u32)(v & 0xFF);
}
#elif RIVET_ARCH_RISCV
RIV_ALWAYS riv_u32 riv_cpu_id(void) {
    riv_u32 v;
    __asm__ __volatile__("csrr %0, mhartid" : "=r"(v));
    return v;
}
#else
RIV_ALWAYS riv_u32 riv_cpu_id(void) { return 0; }
#endif

/* Per-CPU storage: a small flat array of `type` indexed by CPU id.
 * Access with riv_percpu(name) -> reference for the current CPU. */
#define RIV_PERCPU(type, name) static type name[RIV_MAX_CPUS]
#define riv_percpu(name)       ((name)[riv_cpu_id()])

#endif /* RIVET_SMP_H */
