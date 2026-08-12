/* RIVET — irq.h : interrupt control */
#ifndef RIVET_IRQ_H
#define RIVET_IRQ_H

#include "core.h"

#if RIVET_ARCH_X86_64 || RIVET_ARCH_X86
RIV_ALWAYS void riv_irq_enable (void) { __asm__ __volatile__("sti" ::: "memory"); }
RIV_ALWAYS void riv_irq_disable(void) { __asm__ __volatile__("cli" ::: "memory"); }
RIV_ALWAYS void riv_cpu_halt   (void) { __asm__ __volatile__("hlt" ::: "memory"); }
#elif RIVET_ARCH_AARCH64
RIV_ALWAYS void riv_irq_enable (void) { __asm__ __volatile__("msr daifclr, #2" ::: "memory"); }
RIV_ALWAYS void riv_irq_disable(void) { __asm__ __volatile__("msr daifset, #2" ::: "memory"); }
RIV_ALWAYS void riv_cpu_halt   (void) { __asm__ __volatile__("wfi" ::: "memory"); }
#elif RIVET_ARCH_ARM
RIV_ALWAYS void riv_irq_enable (void) { __asm__ __volatile__("cpsie i" ::: "memory"); }
RIV_ALWAYS void riv_irq_disable(void) { __asm__ __volatile__("cpsid i" ::: "memory"); }
RIV_ALWAYS void riv_cpu_halt   (void) { __asm__ __volatile__("wfi" ::: "memory"); }
#elif RIVET_ARCH_RISCV
RIV_ALWAYS void riv_irq_enable (void) { __asm__ __volatile__("csrsi mstatus, 0x8" ::: "memory"); }
RIV_ALWAYS void riv_irq_disable(void) { __asm__ __volatile__("csrci mstatus, 0x8" ::: "memory"); }
RIV_ALWAYS void riv_cpu_halt   (void) { __asm__ __volatile__("wfi" ::: "memory"); }
#else
#  error "RIVET: irq.h: no IRQ control implementation for this architecture. " \
          "Define riv_irq_enable/disable/cpu_halt for your target."
#endif

/* critical section — save/restore IRQ state */
typedef riv_uptr riv_irq_state;

#if RIVET_ARCH_X86_64 || RIVET_ARCH_X86
RIV_ALWAYS riv_irq_state riv_irq_save(void) {
    riv_uptr f;
#  if RIVET_ARCH_X86_64
    __asm__ __volatile__("pushfq; popq %0; cli" : "=r"(f) :: "memory");
#  else
    __asm__ __volatile__("pushfl; popl %0; cli" : "=r"(f) :: "memory");
#  endif
    return f;
}
RIV_ALWAYS void riv_irq_restore(riv_irq_state s) {
#  if RIVET_ARCH_X86_64
    __asm__ __volatile__("pushq %0; popfq" :: "r"(s) : "memory","cc");
#  else
    __asm__ __volatile__("pushl %0; popfl" :: "r"(s) : "memory","cc");
#  endif
}
#elif RIVET_ARCH_AARCH64
RIV_ALWAYS riv_irq_state riv_irq_save(void) {
    riv_uptr d;
    __asm__ __volatile__("mrs %0, daif; msr daifset, #2" : "=r"(d) :: "memory");
    return d;
}
RIV_ALWAYS void riv_irq_restore(riv_irq_state s) {
    __asm__ __volatile__("msr daif, %0" :: "r"(s) : "memory");
}
#elif RIVET_ARCH_ARM
RIV_ALWAYS riv_irq_state riv_irq_save(void) {
    riv_uptr c;
    __asm__ __volatile__("mrs %0, cpsr; cpsid i" : "=r"(c) :: "memory");
    return c & 0x80;   /* I-bit only */
}
RIV_ALWAYS void riv_irq_restore(riv_irq_state s) {
    if (!(s & 0x80)) riv_irq_enable();
}
#elif RIVET_ARCH_RISCV
RIV_ALWAYS riv_irq_state riv_irq_save(void) {
    riv_uptr m;
    __asm__ __volatile__("csrrci %0, mstatus, 0x8" : "=r"(m) :: "memory");
    return m & 0x8;
}
RIV_ALWAYS void riv_irq_restore(riv_irq_state s) {
    if (s & 0x8) __asm__ __volatile__("csrsi mstatus, 0x8" ::: "memory");
}
#endif

/* DSL: scoped critical section */
#define critical_section \
    for (riv_irq_state _riv_s = riv_irq_save(), _riv_i = 0; \
         !_riv_i; _riv_i = 1, riv_irq_restore(_riv_s))

/* vector table declaration helper */
typedef void (*riv_isr_fn)(void);
#define isr_vector(name) RIV_USED RIV_ALIGN(4) static riv_isr_fn const name[]

#endif /* RIVET_IRQ_H */
