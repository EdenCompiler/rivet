/* RIVET — pit.h : Intel 8253/8254 Programmable Interval Timer.
 *
 * The PIT runs at 1.193182 MHz. Channel 0 is wired to IRQ0 on legacy
 * x86 systems and is the simplest periodic tick source: program it in
 * mode 2 (rate generator) or mode 3 (square wave) with a 16-bit divisor.
 *
 * Practical hz range: 19 Hz (max divisor 65536) .. 1.193 MHz (divisor 1).
 * Outside that range the divisor is clamped. */
#ifndef RIVET_PIT_H
#define RIVET_PIT_H

#include "core.h"
#include "io.h"

#if RIVET_ARCH_X86 || RIVET_ARCH_X86_64

#define RIV_PIT_FREQ_HZ   1193182u

#define RIV_PIT_CH0       0x40
#define RIV_PIT_CH1       0x41
#define RIV_PIT_CH2       0x42
#define RIV_PIT_CMD       0x43

/* Command byte: channel | access mode | operating mode | BCD. */
#define RIV_PIT_BIN       0x00
#define RIV_PIT_MODE2     0x04  /* rate generator */
#define RIV_PIT_MODE3     0x06  /* square wave */
#define RIV_PIT_LOHI      0x30  /* access: lo byte then hi byte */
#define RIV_PIT_SEL_CH0   0x00
#define RIV_PIT_SEL_CH2   0x80

/* Configure channel 0 to fire IRQ0 at `hz` per second using mode 2. */
RIV_ALWAYS void riv_pit_init(unsigned hz) {
    if (hz == 0) hz = 100;
    unsigned div = RIV_PIT_FREQ_HZ / hz;
    if (div > 0xFFFF) div = 0xFFFF;
    if (div < 1)      div = 1;

    riv_outb(RIV_PIT_CMD, RIV_PIT_SEL_CH0 | RIV_PIT_LOHI | RIV_PIT_MODE2 | RIV_PIT_BIN);
    riv_outb(RIV_PIT_CH0, (riv_u8)(div & 0xFF));
    riv_outb(RIV_PIT_CH0, (riv_u8)((div >> 8) & 0xFF));
}

/* Latch channel 0 and read its current 16-bit counter (high-res timing). */
RIV_ALWAYS riv_u16 riv_pit_read_ch0(void) {
    riv_outb(RIV_PIT_CMD, 0x00);              /* latch ch0 */
    riv_u8 lo = riv_inb(RIV_PIT_CH0);
    riv_u8 hi = riv_inb(RIV_PIT_CH0);
    return (riv_u16)(((riv_u16)hi << 8) | lo);
}

#endif /* x86 */

#endif /* RIVET_PIT_H */
