/* RIVET — pic.h : legacy 8259A PIC remap / mask / EOI.
 *
 * On x86 systems with legacy interrupts (no APIC), the master+slave
 * 8259s map IRQ0..IRQ15 to CPU vectors 0x20..0x2F after remapping.
 * Before paging is set up, the BIOS leaves IRQ0..7 → INT 0x08..0x0F,
 * which collides with CPU exceptions. The remap below moves them out
 * of the exception range. */
#ifndef RIVET_PIC_H
#define RIVET_PIC_H

#include "core.h"
#include "io.h"

#if RIVET_ARCH_X86 || RIVET_ARCH_X86_64

#define RIV_PIC1_CMD    0x20
#define RIV_PIC1_DATA   0x21
#define RIV_PIC2_CMD    0xA0
#define RIV_PIC2_DATA   0xA1
#define RIV_PIC_EOI     0x20

/* ICW1: init, expecting ICW4; ICW4: 8086 mode. */
#define RIV_ICW1_INIT   0x11
#define RIV_ICW4_8086   0x01

/* Remap master to `off1` (typical 0x20) and slave to `off2` (0x28). */
RIV_ALWAYS void riv_pic_remap(int off1, int off2) {
    riv_u8 m1 = riv_inb(RIV_PIC1_DATA);
    riv_u8 m2 = riv_inb(RIV_PIC2_DATA);

    riv_outb(RIV_PIC1_CMD, RIV_ICW1_INIT); riv_io_wait();
    riv_outb(RIV_PIC2_CMD, RIV_ICW1_INIT); riv_io_wait();
    riv_outb(RIV_PIC1_DATA, (riv_u8)off1); riv_io_wait();
    riv_outb(RIV_PIC2_DATA, (riv_u8)off2); riv_io_wait();
    riv_outb(RIV_PIC1_DATA, 4);            riv_io_wait();  /* slave at IRQ2 */
    riv_outb(RIV_PIC2_DATA, 2);            riv_io_wait();
    riv_outb(RIV_PIC1_DATA, RIV_ICW4_8086);riv_io_wait();
    riv_outb(RIV_PIC2_DATA, RIV_ICW4_8086);riv_io_wait();

    riv_outb(RIV_PIC1_DATA, m1);
    riv_outb(RIV_PIC2_DATA, m2);
}

/* Mask/unmask a single IRQ line (0..15). */
RIV_ALWAYS void riv_pic_set_mask(int irq, int masked) {
    riv_u16 port = (irq < 8) ? RIV_PIC1_DATA : RIV_PIC2_DATA;
    int     bit  = (irq < 8) ? irq : (irq - 8);
    riv_u8  v = riv_inb(port);
    v = masked ? (riv_u8)(v |  (1u << bit))
               : (riv_u8)(v & ~(1u << bit));
    riv_outb(port, v);
}

/* Acknowledge an interrupt. Slave IRQs (>=8) need EOI on both PICs. */
RIV_ALWAYS void riv_pic_eoi(int irq) {
    if (irq >= 8) riv_outb(RIV_PIC2_CMD, RIV_PIC_EOI);
    riv_outb(RIV_PIC1_CMD, RIV_PIC_EOI);
}

/* Mask all lines — useful before APIC takeover. */
RIV_ALWAYS void riv_pic_mask_all(void) {
    riv_outb(RIV_PIC1_DATA, 0xFF);
    riv_outb(RIV_PIC2_DATA, 0xFF);
}

#endif /* x86 */

#endif /* RIVET_PIC_H */
