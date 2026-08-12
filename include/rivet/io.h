/* RIVET — io.h : port I/O (x86 only).
 *
 * On non-x86 architectures port I/O is undefined and the functions
 * below are not declared — callers will get a compile-time error
 * rather than a silent zero/no-op return. */
#ifndef RIVET_IO_H
#define RIVET_IO_H

#include "core.h"

#if RIVET_ARCH_X86 || RIVET_ARCH_X86_64

RIV_ALWAYS riv_u8 riv_inb(riv_u16 port) {
    riv_u8 v; __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
RIV_ALWAYS riv_u16 riv_inw(riv_u16 port) {
    riv_u16 v; __asm__ __volatile__("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
RIV_ALWAYS riv_u32 riv_inl(riv_u16 port) {
    riv_u32 v; __asm__ __volatile__("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
RIV_ALWAYS void riv_outb(riv_u16 port, riv_u8  v) { __asm__ __volatile__("outb %0, %1" :: "a"(v), "Nd"(port)); }
RIV_ALWAYS void riv_outw(riv_u16 port, riv_u16 v) { __asm__ __volatile__("outw %0, %1" :: "a"(v), "Nd"(port)); }
RIV_ALWAYS void riv_outl(riv_u16 port, riv_u32 v) { __asm__ __volatile__("outl %0, %1" :: "a"(v), "Nd"(port)); }

/* IODELAY (~1 µs on 8-bit ISA): write to unused port 0x80. */
RIV_ALWAYS void riv_io_wait(void) { riv_outb(0x80, 0); }

#endif /* x86 */

#endif /* RIVET_IO_H */
