/* RIVET — tty.h : line-discipline TTY.
 *
 * Wraps a raw character device (UART, debugcon, framebuffer console)
 * with two modes:
 *   - raw mode: bytes pass through unmodified, no echo, no editing
 *   - cooked mode: line buffering, backspace handling, echo, EOL
 *     translation, until a CR/LF flushes the line
 *
 * Cooked mode is what you want for a shell prompt; raw mode is what
 * you want for binary I/O. */
#ifndef RIVET_TTY_H
#define RIVET_TTY_H

#include "core.h"
#include "ring.h"
#include "uart.h"

#define RIV_TTY_BUF_SZ  128

#define RIV_TTY_RAW     0
#define RIV_TTY_COOKED  1

typedef struct {
    riv_uart_ops *uart;
    int           mode;
    int           echo;
    riv_u8        line[RIV_TTY_BUF_SZ];
    riv_u32       line_len;
    riv_u8        ring_buf[256];
    riv_ring      ring;        /* completed lines / bytes for reader */
} riv_tty;

RIV_ALWAYS void riv_tty_init(riv_tty *t, riv_uart_ops *u, int mode) {
    t->uart = u;
    t->mode = mode;
    t->echo = (mode == RIV_TTY_COOKED) ? 1 : 0;
    t->line_len = 0;
    riv_ring_init(&t->ring, t->ring_buf, 256);
}

/* Push one input character — called when the UART produces a byte. */
RIV_ALWAYS void riv_tty_push(riv_tty *t, int c) {
    if (t->mode == RIV_TTY_RAW) {
        riv_ring_push(&t->ring, (riv_u8)c);
        if (t->echo && t->uart) t->uart->putc(t->uart, (char)c);
        return;
    }
    if (c == '\b' || c == 0x7F) {
        if (t->line_len > 0) {
            t->line_len--;
            if (t->echo && t->uart) {
                t->uart->putc(t->uart, '\b');
                t->uart->putc(t->uart, ' ');
                t->uart->putc(t->uart, '\b');
            }
        }
        return;
    }
    if (c == '\r' || c == '\n') {
        if (t->echo && t->uart) {
            t->uart->putc(t->uart, '\r');
            t->uart->putc(t->uart, '\n');
        }
        for (riv_u32 i = 0; i < t->line_len; ++i) riv_ring_push(&t->ring, t->line[i]);
        riv_ring_push(&t->ring, '\n');
        t->line_len = 0;
        return;
    }
    if (t->line_len + 1 < RIV_TTY_BUF_SZ) {
        t->line[t->line_len++] = (riv_u8)c;
        if (t->echo && t->uart) t->uart->putc(t->uart, (char)c);
    }
}

/* Non-blocking read of up to n bytes from the cooked/raw buffer. */
RIV_ALWAYS riv_u32 riv_tty_read(riv_tty *t, void *out, riv_u32 n) {
    return riv_ring_read(&t->ring, (riv_u8*)out, n);
}

RIV_ALWAYS void riv_tty_write(riv_tty *t, const void *src, riv_u32 n) {
    if (!t->uart) return;
    const char *s = (const char*)src;
    for (riv_u32 i = 0; i < n; ++i) t->uart->putc(t->uart, s[i]);
}

#endif /* RIVET_TTY_H */
