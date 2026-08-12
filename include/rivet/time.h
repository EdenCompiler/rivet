/* RIVET — time.h : tick counter + monotonic timestamp + busy-wait. */
#ifndef RIVET_TIME_H
#define RIVET_TIME_H

#include "core.h"
#include "mem.h"

/* User wires the tick source: either bump riv_ticks from a SysTick ISR,
 * or implement riv_now_ticks() to read a hardware counter directly. */
extern volatile riv_u64 riv_ticks;
extern riv_u32          riv_ticks_per_sec;

#define RIV_TIME_DECLARE(hz)  \
    volatile riv_u64 riv_ticks = 0; \
    riv_u32          riv_ticks_per_sec = (hz)

RIV_ALWAYS riv_u64 riv_now_ticks(void) { return riv_ticks; }
RIV_ALWAYS riv_u64 riv_now_ms   (void) {
    return (riv_ticks * 1000ull) / riv_ticks_per_sec;
}
RIV_ALWAYS riv_u64 riv_now_us(void) {
    return (riv_ticks * 1000000ull) / riv_ticks_per_sec;
}

RIV_ALWAYS void riv_tick(void) { riv_ticks++; }

/* tick from ISR */
RIV_ALWAYS void riv_time_isr(void) { riv_tick(); }

/* Hardware cycle counter. Returns monotonically-increasing cycle count.
 * Requires the counter to be enabled by the runtime (true on most cores
 * out of reset; on ARMv7 a userspace PMU enable is needed). */
RIV_ALWAYS riv_u64 riv_cycle_count(void) {
#if RIVET_ARCH_X86_64 || RIVET_ARCH_X86
    riv_u32 lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((riv_u64)hi << 32) | lo;
#elif RIVET_ARCH_AARCH64
    riv_u64 v;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(v));
    return v;
#elif RIVET_ARCH_RISCV
#  if RIVET_WORD_BITS == 64
    riv_u64 v;
    __asm__ __volatile__("rdcycle %0" : "=r"(v));
    return v;
#  else
    riv_u32 lo, hi, hi2;
    do {
        __asm__ __volatile__("rdcycleh %0" : "=r"(hi));
        __asm__ __volatile__("rdcycle  %0" : "=r"(lo));
        __asm__ __volatile__("rdcycleh %0" : "=r"(hi2));
    } while (hi != hi2);
    return ((riv_u64)hi << 32) | lo;
#  endif
#elif RIVET_ARCH_ARM
    riv_u32 v;
    __asm__ __volatile__("mrc p15, 0, %0, c9, c13, 0" : "=r"(v));
    return v;
#else
    return riv_ticks;
#endif
}

/* Busy-wait for `cycles` core cycles, using the hardware cycle counter
 * for accuracy. Works without IRQs / scheduler ticks. */
RIV_ALWAYS void riv_busy_wait_cycles(riv_u64 cycles) {
    riv_u64 start = riv_cycle_count();
    while ((riv_cycle_count() - start) < cycles) {
        riv_compiler_barrier();
    }
}

/* Busy-wait for an approximate microsecond count, given a known CPU
 * frequency in Hz. */
RIV_ALWAYS void riv_busy_wait_us(riv_u32 us, riv_u64 cpu_hz) {
    riv_busy_wait_cycles((cpu_hz / 1000000ull) * us);
}

/* sleep using tick counter (only works if ticks are advancing) */
RIV_ALWAYS void riv_delay_ms(riv_u32 ms) {
    riv_u64 end = riv_now_ms() + ms;
    while (riv_now_ms() < end) riv_cpu_relax();
}

/* time-deadline helper */
typedef struct { riv_u64 deadline; } riv_deadline;
RIV_ALWAYS riv_deadline riv_deadline_ms(riv_u32 ms) {
    riv_deadline d; d.deadline = riv_now_ms() + ms; return d;
}
RIV_ALWAYS int riv_deadline_expired(riv_deadline d) {
    return riv_now_ms() >= d.deadline;
}

#endif /* RIVET_TIME_H */
