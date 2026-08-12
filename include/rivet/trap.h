/* RIVET — trap.h : interrupt / exception entry plumbing.
 *
 * Provides:
 *   riv_trap_frame    register snapshot pushed by the entry stub
 *   riv_trap_handler  C-level handler signature
 *   riv_trap_set      install handler by vector
 *   riv_idt_load      arch-specific IDT/IVT activation
 *
 * Entry stubs themselves must be written in assembly per arch — RIVET
 * provides the data structures and dispatch table; the user (or boot
 * stub) wires the asm to call riv_trap_dispatch(). */
#ifndef RIVET_TRAP_H
#define RIVET_TRAP_H

#include "core.h"
#include "irq.h"

/* Generic trap frame — supersets of arch-specific layouts. The arch
 * entry stub fills the relevant fields and zeros the rest. */
typedef struct riv_trap_frame {
    riv_uptr regs[32];   /* general-purpose registers, arch-defined order */
    riv_uptr pc;
    riv_uptr sp;
    riv_uptr flags;
    riv_uptr vector;     /* interrupt or exception number */
    riv_uptr error;      /* CPU-pushed error code, 0 if none */
} riv_trap_frame;

typedef void (*riv_trap_handler)(riv_trap_frame *tf);

#define RIV_TRAP_MAX 256
extern riv_trap_handler riv_trap_table[RIV_TRAP_MAX];

#define RIV_TRAP_DECLARE() \
    riv_trap_handler riv_trap_table[RIV_TRAP_MAX] = { (riv_trap_handler)0 }

RIV_ALWAYS void riv_trap_set(unsigned vec, riv_trap_handler h) {
    if (vec < RIV_TRAP_MAX) riv_trap_table[vec] = h;
}
RIV_ALWAYS riv_trap_handler riv_trap_get(unsigned vec) {
    return (vec < RIV_TRAP_MAX) ? riv_trap_table[vec] : (riv_trap_handler)0;
}

/* C dispatch — call from the asm entry stub after pushing the frame. */
RIV_ALWAYS void riv_trap_dispatch(riv_trap_frame *tf) {
    riv_trap_handler h = (tf->vector < RIV_TRAP_MAX)
                       ? riv_trap_table[tf->vector] : (riv_trap_handler)0;
    if (h) h(tf);
}

#if RIVET_ARCH_X86_64
/* ----------------------- x86_64 IDT ---------------------------------- */

typedef struct RIV_PACKED riv_idt_entry {
    riv_u16 offset_lo;
    riv_u16 selector;
    riv_u8  ist;
    riv_u8  type_attr;
    riv_u16 offset_mid;
    riv_u32 offset_hi;
    riv_u32 zero;
} riv_idt_entry;

typedef struct RIV_PACKED riv_idtr {
    riv_u16 limit;
    riv_u64 base;
} riv_idtr;

#define RIV_IDT_INTR  0x8E  /* present | DPL=0 | type=interrupt gate */
#define RIV_IDT_TRAP  0x8F  /* present | DPL=0 | type=trap gate */

RIV_ALWAYS void riv_idt_set_entry(riv_idt_entry *e, riv_uptr handler,
                                  riv_u16 sel, riv_u8 type) {
    e->offset_lo  = (riv_u16)(handler & 0xFFFF);
    e->selector   = sel;
    e->ist        = 0;
    e->type_attr  = type;
    e->offset_mid = (riv_u16)((handler >> 16) & 0xFFFF);
    e->offset_hi  = (riv_u32)(handler >> 32);
    e->zero       = 0;
}
RIV_ALWAYS void riv_idt_load(riv_idt_entry *idt, riv_u32 count) {
    riv_idtr d;
    d.limit = (riv_u16)(count * sizeof(riv_idt_entry) - 1);
    d.base  = (riv_u64)(riv_uptr)idt;
    __asm__ __volatile__("lidt %0" :: "m"(d));
}

#elif RIVET_ARCH_RISCV
/* ----------------------- RISC-V trap vector -------------------------- */

/* On RISC-V mtvec/stvec points to a single trap handler. Cause register
 * disambiguates: interrupts vs exceptions, code in low bits. */
RIV_ALWAYS void riv_trap_load(void (*entry)(void), int vectored_mode) {
    riv_uptr v = (riv_uptr)entry;
    if (vectored_mode) v |= 1;
    __asm__ __volatile__("csrw mtvec, %0" :: "r"(v) : "memory");
}

#endif

#endif /* RIVET_TRAP_H */
