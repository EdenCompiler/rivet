/* RIVET demo: panic handler + cycle counter + debugcon UART.
 *
 * Boots under qemu-system-x86_64 via the lm64 multiboot trampoline.
 * Sanity-checks the cycle counter, exercises the logging path, then
 * exits cleanly via the QEMU isa-debug-exit device on port 0xF4.
 *
 * To force a panic, build with `-DRIVET_DEMO_FORCE_PANIC`. */

#define RIVET_PANIC_IMPL
#include "rivet.h"

RIV_UART_DECLARE();
RIV_LOG_DECLARE();
RIV_TIME_DECLARE(1000);

static riv_uart_debugcon dbg_uart;

static void log_sink(const char *s, riv_size n) {
    riv_uart_write(s, n);
}

RIV_NORETURN static void qemu_shutdown(riv_u8 code) {
    riv_outb(0xF4, code);
    forever { riv_cpu_halt(); }
}

/* Entry from examples/qemu/lm64_boot.S. */
void kmain(unsigned magic, unsigned mb_info) {
    (void)mb_info;

    riv_uart_debugcon_init(&dbg_uart);
    riv_uart_set_default(&dbg_uart.ops);
    riv_log_set_sink(log_sink);
    riv_log_set_level(RIV_LOG_DEBUG);

    riv_log_info("init", "RIVET v%d.%d.%d up (multiboot magic 0x%x)",
                 RIVET_VERSION_MAJOR, RIVET_VERSION_MINOR,
                 RIVET_VERSION_PATCH, magic);

    riv_u64 c0 = riv_cycle_count();
    riv_busy_wait_cycles(1000);
    riv_u64 c1 = riv_cycle_count();
    if (c1 == c0) {
        riv_log_err("time", "cycle counter not advancing");
        qemu_shutdown(2);
    }
    riv_log_info("time", "cycle counter advanced by %llu over 1000-cycle wait",
                 (unsigned long long)(c1 - c0));

#ifdef RIVET_DEMO_FORCE_PANIC
    riv_assert(2 + 2 == 5);
#endif

    riv_log_info("kernel", "demo complete, shutting down");
    qemu_shutdown(0);
}
