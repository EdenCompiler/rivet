/* RIVET demo: tiny bootable x86_64 kernel.
 *
 * Writes a banner to BOTH the legacy VGA text framebuffer (0xB8000) and
 * the QEMU debug console (port 0xE9), then exits via isa-debug-exit so
 * the host can collect the output without a graphical window.
 *
 * Boots via examples/qemu/lm64_boot.S under qemu-system-x86_64 -kernel. */

#include "rivet.h"

RIV_UART_DECLARE();
static riv_uart_debugcon dbg_uart;

#define VGA_BASE  0xB8000UL
#define VGA_COLS  80
#define VGA_ROWS  25
static riv_u16 vga_pos = 0;

static void vga_putc(char c) {
    volatile riv_u16 *fb = (volatile riv_u16*)VGA_BASE;
    if (c == '\n') { vga_pos = (vga_pos / VGA_COLS + 1) * VGA_COLS; }
    else           { fb[vga_pos++] = (riv_u16)0x0F00 | (riv_u8)c; }
    if (vga_pos >= VGA_COLS * VGA_ROWS) vga_pos = 0;
}
static void banner(const char *s) {
    while (*s) {
        vga_putc(*s);
        riv_uart_putc(*s);
        ++s;
    }
}

/* linker-defined BSS bounds (declared by lm64.ld) */
riv_link_sym(__bss_start);
riv_link_sym(__bss_end);

RIV_NORETURN static void qemu_shutdown(riv_u8 code) {
    riv_outb(0xF4, code);
    forever { riv_cpu_halt(); }
}

/* Entry from examples/qemu/multiboot.S. */
void kmain(unsigned magic, unsigned mb_info) {
    (void)magic; (void)mb_info;

    /* riv_zero_range(__bss_start, __bss_end) is intentionally NOT called
     * here — the active stack lives in .bss; the multiboot loader is
     * trusted to have zeroed the image's BSS before control transfer. */

    riv_uart_debugcon_init(&dbg_uart);
    riv_uart_set_default(&dbg_uart.ops);

    banner("RIVET kernel boot OK on ");
    banner(RIVET_ARCH_NAME);
    banner("\n");

    riv_assert(sizeof(riv_uptr) == sizeof(void*));

    qemu_shutdown(0);
}
