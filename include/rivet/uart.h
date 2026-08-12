/* RIVET — uart.h : generic UART driver interface + 16550A backend.
 * Provides riv_uart_putc / riv_uart_getc / riv_uart_write through a
 * function-pointer vtable so platform code can register its UART once. */
#ifndef RIVET_UART_H
#define RIVET_UART_H

#include "core.h"
#include "mmio.h"
#include "mem.h"

typedef struct riv_uart_ops {
    void (*putc)(struct riv_uart_ops *u, char c);
    int  (*getc)(struct riv_uart_ops *u);     /* -1 if no data */
    int  (*rx_ready)(struct riv_uart_ops *u);
    int  (*tx_ready)(struct riv_uart_ops *u);
} riv_uart_ops;

extern riv_uart_ops *riv_uart_default;
#define RIV_UART_DECLARE()  riv_uart_ops *riv_uart_default = (riv_uart_ops*)0

RIV_ALWAYS void riv_uart_set_default(riv_uart_ops *u) { riv_uart_default = u; }

RIV_ALWAYS void riv_uart_putc(char c) {
    if (riv_uart_default) riv_uart_default->putc(riv_uart_default, c);
}
RIV_ALWAYS void riv_uart_write(const char *s, riv_size n) {
    if (!riv_uart_default) return;
    for (riv_size i = 0; i < n; ++i) riv_uart_default->putc(riv_uart_default, s[i]);
}
RIV_ALWAYS void riv_uart_puts(const char *s) {
    while (*s) riv_uart_putc(*s++);
}

/* ---------- 16550A backend ---------- */
typedef struct {
    riv_uart_ops ops;
    riv_uptr     base;     /* MMIO base */
    int          shift;    /* register stride: 0 (byte) or 2 (4-byte) */
} riv_uart_16550;

#define UART_RBR  0
#define UART_THR  0
#define UART_IER  1
#define UART_FCR  2
#define UART_LCR  3
#define UART_LSR  5

RIV_ALWAYS riv_u8 _riv_16550_rd(const riv_uart_16550 *u, int reg) {
    return *(volatile riv_u8*)(u->base + ((riv_uptr)reg << u->shift));
}
RIV_ALWAYS void _riv_16550_wr(const riv_uart_16550 *u, int reg, riv_u8 v) {
    *(volatile riv_u8*)(u->base + ((riv_uptr)reg << u->shift)) = v;
}

static inline void _riv_16550_putc(riv_uart_ops *o, char c) {
    riv_uart_16550 *u = (riv_uart_16550*)o;
    while (!(_riv_16550_rd(u, UART_LSR) & 0x20)) { }
    _riv_16550_wr(u, UART_THR, (riv_u8)c);
}
static inline int _riv_16550_getc(riv_uart_ops *o) {
    riv_uart_16550 *u = (riv_uart_16550*)o;
    if (!(_riv_16550_rd(u, UART_LSR) & 0x01)) return -1;
    return _riv_16550_rd(u, UART_RBR);
}
static inline int _riv_16550_rx_ready(riv_uart_ops *o) {
    riv_uart_16550 *u = (riv_uart_16550*)o;
    return _riv_16550_rd(u, UART_LSR) & 0x01;
}
static inline int _riv_16550_tx_ready(riv_uart_ops *o) {
    riv_uart_16550 *u = (riv_uart_16550*)o;
    return (_riv_16550_rd(u, UART_LSR) & 0x20) ? 1 : 0;
}

RIV_ALWAYS void riv_uart_16550_init(riv_uart_16550 *u, riv_uptr base, int reg_shift) {
    u->base  = base;
    u->shift = reg_shift;
    u->ops.putc     = _riv_16550_putc;
    u->ops.getc     = _riv_16550_getc;
    u->ops.rx_ready = _riv_16550_rx_ready;
    u->ops.tx_ready = _riv_16550_tx_ready;
    _riv_16550_wr(u, UART_IER, 0x00);     /* disable IRQs */
    _riv_16550_wr(u, UART_FCR, 0xC7);     /* enable+clear FIFO, 14-byte trig */
    _riv_16550_wr(u, UART_LCR, 0x03);     /* 8N1 */
}

/* ---------- ARM PL011 backend (QEMU virt aarch64, RPi mini-UART, etc) -- */

typedef struct {
    riv_uart_ops ops;
    riv_uptr     base;
} riv_uart_pl011;

#define PL011_DR    0x00
#define PL011_FR    0x18    /* TXFF bit5, RXFE bit4 */
#define PL011_IBRD  0x24
#define PL011_FBRD  0x28
#define PL011_LCRH  0x2C
#define PL011_CR    0x30
#define PL011_IMSC  0x38

static inline riv_u32 _riv_pl011_rd(const riv_uart_pl011 *u, int reg) {
    return *(volatile riv_u32*)(u->base + reg);
}
static inline void _riv_pl011_wr(const riv_uart_pl011 *u, int reg, riv_u32 v) {
    *(volatile riv_u32*)(u->base + reg) = v;
}

static inline void _riv_pl011_putc(riv_uart_ops *o, char c) {
    riv_uart_pl011 *u = (riv_uart_pl011*)o;
    while (_riv_pl011_rd(u, PL011_FR) & (1u << 5)) { }  /* TXFF */
    _riv_pl011_wr(u, PL011_DR, (riv_u32)(riv_u8)c);
}
static inline int _riv_pl011_getc(riv_uart_ops *o) {
    riv_uart_pl011 *u = (riv_uart_pl011*)o;
    if (_riv_pl011_rd(u, PL011_FR) & (1u << 4)) return -1;  /* RXFE */
    return (int)(_riv_pl011_rd(u, PL011_DR) & 0xFFu);
}
static inline int _riv_pl011_rx_ready(riv_uart_ops *o) {
    riv_uart_pl011 *u = (riv_uart_pl011*)o;
    return !(_riv_pl011_rd(u, PL011_FR) & (1u << 4));
}
static inline int _riv_pl011_tx_ready(riv_uart_ops *o) {
    riv_uart_pl011 *u = (riv_uart_pl011*)o;
    return !(_riv_pl011_rd(u, PL011_FR) & (1u << 5));
}

/* Init PL011. Set baud_rate to 0 to skip baud programming (useful when
 * the firmware/bootloader has already configured the UART, e.g. QEMU). */
RIV_ALWAYS void riv_uart_pl011_init(riv_uart_pl011 *u, riv_uptr base,
                                    riv_u32 uart_clk_hz, riv_u32 baud_rate) {
    u->base = base;
    u->ops.putc     = _riv_pl011_putc;
    u->ops.getc     = _riv_pl011_getc;
    u->ops.rx_ready = _riv_pl011_rx_ready;
    u->ops.tx_ready = _riv_pl011_tx_ready;

    if (baud_rate) {
        _riv_pl011_wr(u, PL011_CR,   0x0000);             /* disable */
        riv_u32 div = (uart_clk_hz * 4u) / baud_rate;
        _riv_pl011_wr(u, PL011_IBRD, div >> 6);
        _riv_pl011_wr(u, PL011_FBRD, div & 0x3F);
        _riv_pl011_wr(u, PL011_LCRH, (3u << 5) | (1u << 4)); /* 8N1, FIFO */
        _riv_pl011_wr(u, PL011_IMSC, 0x0000);             /* mask all IRQs */
        _riv_pl011_wr(u, PL011_CR,   (1u << 0) | (1u << 8) | (1u << 9)); /* UARTEN|TXE|RXE */
    }
}

/* ---------- x86 debug console (QEMU/Bochs port 0xE9, write-only) ----- */

#if RIVET_ARCH_X86 || RIVET_ARCH_X86_64
typedef struct { riv_uart_ops ops; } riv_uart_debugcon;

static inline void _riv_dbg_putc(riv_uart_ops *o, char c) {
    (void)o;
    __asm__ __volatile__("outb %0, $0xE9" :: "a"((unsigned char)c));
}
static inline int _riv_dbg_getc(riv_uart_ops *o) { (void)o; return -1; }
static inline int _riv_dbg_rx_ready(riv_uart_ops *o) { (void)o; return 0; }
static inline int _riv_dbg_tx_ready(riv_uart_ops *o) { (void)o; return 1; }

RIV_ALWAYS void riv_uart_debugcon_init(riv_uart_debugcon *u) {
    u->ops.putc     = _riv_dbg_putc;
    u->ops.getc     = _riv_dbg_getc;
    u->ops.rx_ready = _riv_dbg_rx_ready;
    u->ops.tx_ready = _riv_dbg_tx_ready;
}
#endif

#endif /* RIVET_UART_H */
