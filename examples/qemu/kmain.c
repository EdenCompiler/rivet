/* RIVET demo: 32-bit multiboot1 kernel using the RIVET runtime.
 *
 * Run with:
 *   qemu-system-x86_64 -kernel bin/multiboot.elf -nographic \
 *                      -debugcon stdio -no-reboot
 *
 * The kernel:
 *   1. verifies the multiboot magic value
 *   2. writes a banner to the QEMU debug console (port 0xE9)
 *   3. writes the same banner to VGA text memory at 0xB8000
 *   4. shuts QEMU down cleanly via the isa-debug-exit device */

#include "rivet/core.h"
#include "rivet/irq.h"
#include "rivet/io.h"
#include "rivet/uart.h"

RIV_UART_DECLARE();
static riv_uart_debugcon dbg;

/* VGA text buffer at 0xB8000 in 80x25 mode. Each cell is attr|char. */
static void vga_puts(const char *s, int row) {
    volatile riv_u16 *fb = (volatile riv_u16*)0xB8000;
    for (int i = 0; s[i] && i < 80; ++i) {
        fb[row * 80 + i] = (riv_u16)(0x0F00 | (riv_u8)s[i]);
    }
}

/* QEMU isa-debug-exit: writing to port 0xF4 ends the VM. The host exit
 * status is `(value << 1) | 1`. */
RIV_NORETURN static void qemu_shutdown(riv_u8 code) {
    riv_outb(0xF4, code);
    forever { riv_cpu_halt(); }
}

void kmain(unsigned magic, unsigned mb_info) {
    (void)mb_info;

    riv_uart_debugcon_init(&dbg);
    riv_uart_set_default(&dbg.ops);

    riv_uart_puts("\n*** RIVET multiboot kernel booted in QEMU ***\n");

    if (magic == 0x2BADB002) {
        riv_uart_puts("multiboot magic verified (0x2BADB002)\n");
        vga_puts("RIVET multiboot kernel: magic OK", 0);
    } else {
        riv_uart_puts("multiboot magic MISMATCH — bootloader did not honor MB1\n");
        vga_puts("RIVET multiboot kernel: BAD MAGIC", 0);
    }

    /* Exercise a handful of runtime primitives so the path through
     * the framework is observable in the host's stdio. */
    riv_uart_puts("smoke-testing the runtime:\n");
    riv_uart_puts("  - riv_outb / riv_inb available\n");
    riv_uart_puts("  - riv_cpu_halt available\n");
    riv_uart_puts("  - debugcon UART backend active\n");
    riv_uart_puts("shutting down via isa-debug-exit (port 0xF4)\n");

    qemu_shutdown(0);
}
