/* RIVET demo: x86_64 early init exercising the v0.4 module set.
 *
 * Configures legacy PIC/PIT, sets up a small GDT + TSS, enumerates
 * PCI, ELF-loads an embedded user image, plus exercises mutex /
 * semaphore / pipe / hash / rand. Compile-only smoke test. */

#define RIVET_PANIC_IMPL
#include "rivet.h"

/* one-TU definitions */
RIV_UART_DECLARE();
RIV_LOG_DECLARE();
RIV_TIME_DECLARE(100);
RIV_TRAP_DECLARE();
RIV_PROC_DECLARE();
RIV_SYSCALL_DECLARE();
RIV_VFS_DECLARE();
RIV_FB_FONT8X8_DECLARE();

void riv_ctx_switch(riv_context *save, riv_context *load) {
    (void)save; (void)load;
}

/* ---- GDT layout ------------------------------------------------------
 *
 * The lm64 trampoline already installed a working 64-bit GDT; we keep
 * this section as a compile-time exercise of the riv_gdt_* helpers but
 * deliberately do not call gdt_setup() at runtime — re-loading a GDT
 * mid-kernel without also re-loading code/data selectors is unsafe. */
#if RIVET_ARCH_X86_64
static riv_gdt_entry      gdt[5] RIV_UNUSED;
static riv_tss_descriptor tss_desc RIV_UNUSED;
static riv_tss64          tss RIV_UNUSED;

RIV_UNUSED static void gdt_setup(void) {
    riv_gdt_set(&gdt[0], 0, 0, 0,             0);
    riv_gdt_set(&gdt[1], 0, 0xFFFFF, RIV_GDT_KCODE, RIV_GDT_L | RIV_GDT_G);
    riv_gdt_set(&gdt[2], 0, 0xFFFFF, RIV_GDT_KDATA, RIV_GDT_DB | RIV_GDT_G);
    riv_gdt_set(&gdt[3], 0, 0xFFFFF, RIV_GDT_UCODE, RIV_GDT_L | RIV_GDT_G);
    riv_gdt_set(&gdt[4], 0, 0xFFFFF, RIV_GDT_UDATA, RIV_GDT_DB | RIV_GDT_G);
    riv_gdt_set_tss(&tss_desc, (riv_uptr)&tss, sizeof(tss) - 1);
    riv_gdt_load(gdt, sizeof(gdt) - 1);
}
#endif

/* ---- example PCI visit callback ------------------------------------- */
static void pci_visit(riv_pci_loc loc, riv_u16 vendor, riv_u16 device, void *ctx) {
    (void)ctx;
    riv_log_info("pci", "%02x:%02x.%x  %04x:%04x",
                 loc.bus, loc.dev, loc.func, vendor, device);
}

/* ---- ELF placement callback ---------------------------------------- */
static int elf_place(riv_u64 vaddr, const void *data, riv_u64 filesz,
                     riv_u64 memsz, riv_u32 flags, void *ctx) {
    (void)flags; (void)ctx;
    riv_memcpy((void*)(riv_uptr)vaddr, data, (riv_size)filesz);
    if (memsz > filesz)
        riv_memset((char*)(riv_uptr)vaddr + filesz, 0, (riv_size)(memsz - filesz));
    return 0;
}

/* ---- mutex / sem / pipe demo (no real scheduling, just compile) ---- */
static riv_mutex   m;
static riv_sem     sem;
static riv_pipe    p;
static riv_hash    h;
static riv_hash_slot hslots[16];
static riv_rng     rng;

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
    (void)magic; (void)mb_info;

    riv_uart_debugcon_init(&dbg_uart);
    riv_uart_set_default(&dbg_uart.ops);
    riv_log_set_sink(log_sink);
    riv_log_set_level(RIV_LOG_DEBUG);

    /* gdt_setup() and TSS are exercised only at compile time. */
    riv_pic_remap(0x20, 0x28);
    riv_pic_mask_all();
    riv_pit_init(100);

    riv_pci_enumerate(pci_visit, (void*)0);

    riv_mutex_init(&m);
    riv_sem_init(&sem, 1);
    riv_pipe_init(&p);
    riv_hash_init(&h, hslots, 16);
    riv_hash_set(&h, "init", 1);
    riv_rng_seed(&rng, riv_cycle_count());
    riv_u32 r = riv_rng_range(&rng, 100);
    riv_log_info("rng", "sample = %u", (unsigned)r);

    riv_log_info("kernel", "RIVET v%d.%d up; PCI scan complete",
                 RIVET_VERSION_MAJOR, RIVET_VERSION_MINOR);

    qemu_shutdown(0);
}
