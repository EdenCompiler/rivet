/* RIVET — apic.h : Local APIC + IOAPIC register access (x86_64).
 *
 * Modern x86 interrupt path: each core has a Local APIC, all I/O lines
 * route through one or more IOAPICs. Local APIC base lives at the MSR
 * IA32_APIC_BASE (default phys 0xFEE00000). Registers are 4 bytes each,
 * 16-byte aligned within the page. */
#ifndef RIVET_APIC_H
#define RIVET_APIC_H

#include "core.h"
#include "mem.h"

#if RIVET_ARCH_X86_64

#define RIV_APIC_BASE_MSR      0x1B
#define RIV_APIC_ENABLE_BIT    (1u << 11)

#define RIV_LAPIC_ID           0x020
#define RIV_LAPIC_VERSION      0x030
#define RIV_LAPIC_TPR          0x080
#define RIV_LAPIC_EOI          0x0B0
#define RIV_LAPIC_LDR          0x0D0
#define RIV_LAPIC_DFR          0x0E0
#define RIV_LAPIC_SVR          0x0F0   /* spurious vector */
#define RIV_LAPIC_ESR          0x280
#define RIV_LAPIC_ICR_LO       0x300
#define RIV_LAPIC_ICR_HI       0x310
#define RIV_LAPIC_TIMER        0x320
#define RIV_LAPIC_LINT0        0x350
#define RIV_LAPIC_LINT1        0x360
#define RIV_LAPIC_ERROR        0x370
#define RIV_LAPIC_TIMER_INITCNT 0x380
#define RIV_LAPIC_TIMER_CURCNT  0x390
#define RIV_LAPIC_TIMER_DIV    0x3E0

/* Timer mode bits (in TIMER LVT). */
#define RIV_LAPIC_TIMER_ONESHOT  (0u << 17)
#define RIV_LAPIC_TIMER_PERIODIC (1u << 17)
#define RIV_LAPIC_TIMER_TSCDLINE (2u << 17)
#define RIV_LAPIC_LVT_MASK       (1u << 16)

RIV_ALWAYS riv_u64 _riv_rdmsr(riv_u32 msr) {
    riv_u32 lo, hi;
    __asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((riv_u64)hi << 32) | lo;
}
RIV_ALWAYS void _riv_wrmsr(riv_u32 msr, riv_u64 v) {
    __asm__ __volatile__("wrmsr" :: "c"(msr), "a"((riv_u32)v), "d"((riv_u32)(v >> 32)));
}

RIV_ALWAYS riv_uptr riv_lapic_phys_base(void) {
    return (riv_uptr)(_riv_rdmsr(RIV_APIC_BASE_MSR) & ~0xFFFull);
}

/* Software-enable the LAPIC; set MSR enable bit + SVR.SoftEnable. */
RIV_ALWAYS void riv_lapic_enable(volatile void *mmio, riv_u8 spurious_vec) {
    _riv_wrmsr(RIV_APIC_BASE_MSR,
               _riv_rdmsr(RIV_APIC_BASE_MSR) | RIV_APIC_ENABLE_BIT);
    volatile riv_u32 *p = (volatile riv_u32*)((char*)mmio + RIV_LAPIC_SVR);
    *p = (riv_u32)(spurious_vec | 0x100);  /* enable bit 8 */
}

RIV_ALWAYS riv_u32 riv_lapic_read (volatile void *mmio, riv_u32 reg) {
    return *(volatile riv_u32*)((char*)mmio + reg);
}
RIV_ALWAYS void    riv_lapic_write(volatile void *mmio, riv_u32 reg, riv_u32 v) {
    *(volatile riv_u32*)((char*)mmio + reg) = v;
}
RIV_ALWAYS void riv_lapic_eoi(volatile void *mmio) {
    riv_lapic_write(mmio, RIV_LAPIC_EOI, 0);
}

/* Configure periodic timer firing `vec` every `initcnt` LAPIC ticks
 * (after divisor). Divisor encoding follows the standard table. */
RIV_ALWAYS void riv_lapic_timer_periodic(volatile void *mmio,
                                          riv_u8 vec, riv_u32 initcnt) {
    riv_lapic_write(mmio, RIV_LAPIC_TIMER_DIV, 0x3);   /* divide by 16 */
    riv_lapic_write(mmio, RIV_LAPIC_TIMER,
                    vec | RIV_LAPIC_TIMER_PERIODIC);
    riv_lapic_write(mmio, RIV_LAPIC_TIMER_INITCNT, initcnt);
}

/* ----------------------- I/O APIC ------------------------------------ */
#define RIV_IOAPIC_REGSEL  0x00
#define RIV_IOAPIC_REGWIN  0x10
#define RIV_IOAPIC_REDTBL  0x10   /* IRQn lives at REDTBL + 2n */

RIV_ALWAYS riv_u32 riv_ioapic_read(volatile void *mmio, riv_u8 reg) {
    *(volatile riv_u32*)((char*)mmio + RIV_IOAPIC_REGSEL) = reg;
    return *(volatile riv_u32*)((char*)mmio + RIV_IOAPIC_REGWIN);
}
RIV_ALWAYS void riv_ioapic_write(volatile void *mmio, riv_u8 reg, riv_u32 v) {
    *(volatile riv_u32*)((char*)mmio + RIV_IOAPIC_REGSEL) = reg;
    *(volatile riv_u32*)((char*)mmio + RIV_IOAPIC_REGWIN) = v;
}

/* Route physical IRQ `irq` to APIC vector `vec` on CPU `apic_id`. */
RIV_ALWAYS void riv_ioapic_route(volatile void *mmio, riv_u8 irq,
                                  riv_u8 vec, riv_u8 apic_id) {
    riv_ioapic_write(mmio, (riv_u8)(RIV_IOAPIC_REDTBL + 2 * irq),
                     (riv_u32)vec);
    riv_ioapic_write(mmio, (riv_u8)(RIV_IOAPIC_REDTBL + 2 * irq + 1),
                     (riv_u32)apic_id << 24);
}

#endif /* x86_64 */

#endif /* RIVET_APIC_H */
