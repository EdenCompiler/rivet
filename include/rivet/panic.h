/* RIVET — panic.h : panic / assert / halt.
 *
 * The default panic handler (enabled by `#define RIVET_PANIC_IMPL` in
 * exactly one TU) writes a formatted diagnostic message to the registered
 * UART backend (rivet/uart.h) and, on x86, additionally to the Bochs/QEMU
 * debug console port 0xE9. Then disables IRQs and halts forever. */
#ifndef RIVET_PANIC_H
#define RIVET_PANIC_H

#include "core.h"
#include "irq.h"

/* User-overridable. Default provided when RIVET_PANIC_IMPL is set. */
RIV_NORETURN void riv_panic_handler(const char *msg, const char *file, int line);

RIV_NORETURN RIV_ALWAYS void riv_halt_forever(void) {
    riv_irq_disable();
    forever { riv_cpu_halt(); }
}

#define riv_panic(msg)  do { \
        riv_panic_handler((msg), __FILE__, __LINE__); \
        unreachable(); \
    } while (0)

#define riv_assert(cond) do { \
        if (RIV_UNLIKELY(!(cond))) riv_panic("assert: " #cond); \
    } while (0)

#define riv_bug()  riv_panic("BUG: unreachable")

/* --- default implementation (gated; one TU only) ---------------------- */
#ifdef RIVET_PANIC_IMPL
#include "str.h"
#include "uart.h"

#if RIVET_ARCH_X86 || RIVET_ARCH_X86_64
static inline void _riv_panic_debugcon(const char *s, riv_size n) {
    for (riv_size i = 0; i < n; ++i)
        __asm__ __volatile__("outb %0, $0xE9" :: "a"((unsigned char)s[i]));
}
#endif

RIV_WEAK RIV_NORETURN void riv_panic_handler(const char *msg, const char *file, int line) {
    char buf[256];
    int n = riv_snprintf(buf, sizeof(buf),
        "\n*** RIVET PANIC: %s @ %s:%d ***\n",
        msg  ? msg  : "(null)",
        file ? file : "?",
        line);
    if (n < 0) n = 0;
    if ((riv_size)n >= sizeof(buf)) n = (int)sizeof(buf) - 1;

    if (riv_uart_default) {
        riv_uart_write(buf, (riv_size)n);
    }
#if RIVET_ARCH_X86 || RIVET_ARCH_X86_64
    _riv_panic_debugcon(buf, (riv_size)n);
#endif
    riv_halt_forever();
}
#endif /* RIVET_PANIC_IMPL */

#endif /* RIVET_PANIC_H */
