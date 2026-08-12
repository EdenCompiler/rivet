/* RIVET — mem.h : memory ops, barriers, cache */
#ifndef RIVET_MEM_H
#define RIVET_MEM_H

#include "core.h"

/* freestanding memcpy / memset / memcmp — no libc */
RIV_ALWAYS void* riv_memset(void *dst, int v, riv_size n) {
    riv_u8 *p = (riv_u8*)dst;
    while (n--) *p++ = (riv_u8)v;
    return dst;
}
RIV_ALWAYS void* riv_memcpy(void *dst, const void *src, riv_size n) {
    riv_u8 *d = (riv_u8*)dst; const riv_u8 *s = (const riv_u8*)src;
    while (n--) *d++ = *s++;
    return dst;
}
RIV_ALWAYS int riv_memcmp(const void *a, const void *b, riv_size n) {
    const riv_u8 *x = (const riv_u8*)a, *y = (const riv_u8*)b;
    while (n--) { if (*x != *y) return (int)*x - (int)*y; ++x; ++y; }
    return 0;
}

/* compiler barrier — prevents reordering across point */
#define riv_compiler_barrier() __asm__ __volatile__("" ::: "memory")

/* full memory barrier (data sync) */
#if RIVET_ARCH_X86_64 || RIVET_ARCH_X86
#  define riv_mb()  __asm__ __volatile__("mfence" ::: "memory")
#  define riv_rmb() __asm__ __volatile__("lfence" ::: "memory")
#  define riv_wmb() __asm__ __volatile__("sfence" ::: "memory")
#elif RIVET_ARCH_AARCH64
#  define riv_mb()  __asm__ __volatile__("dsb sy" ::: "memory")
#  define riv_rmb() __asm__ __volatile__("dsb ld" ::: "memory")
#  define riv_wmb() __asm__ __volatile__("dsb st" ::: "memory")
#elif RIVET_ARCH_ARM
#  define riv_mb()  __asm__ __volatile__("dsb" ::: "memory")
#  define riv_rmb() riv_mb()
#  define riv_wmb() riv_mb()
#elif RIVET_ARCH_RISCV
#  define riv_mb()  __asm__ __volatile__("fence rw,rw" ::: "memory")
#  define riv_rmb() __asm__ __volatile__("fence r,r" ::: "memory")
#  define riv_wmb() __asm__ __volatile__("fence w,w" ::: "memory")
#else
#  define riv_mb()  riv_compiler_barrier()
#  define riv_rmb() riv_compiler_barrier()
#  define riv_wmb() riv_compiler_barrier()
#endif

/* CPU relax / pause */
#if RIVET_ARCH_X86_64 || RIVET_ARCH_X86
#  define riv_cpu_relax() __asm__ __volatile__("pause" ::: "memory")
#elif RIVET_ARCH_AARCH64 || RIVET_ARCH_ARM
#  define riv_cpu_relax() __asm__ __volatile__("yield" ::: "memory")
#else
#  define riv_cpu_relax() riv_compiler_barrier()
#endif

#endif /* RIVET_MEM_H */
