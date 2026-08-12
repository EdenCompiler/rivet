/* CINDER kernel demo exercising the new high-level subsystems:
 *   paging, traps, processes, syscalls, VFS+ramfs.
 *
 * This is a compile-only check that all headers cohere; running it
 * requires an arch-specific boot stub + real RAM layout. */

#define RIVET_PANIC_IMPL
#include "rivet.h"

/* one-TU declarations for the global tables */
RIV_UART_DECLARE();
RIV_LOG_DECLARE();
RIV_TIME_DECLARE(1000);
RIV_TRAP_DECLARE();
RIV_PROC_DECLARE();
RIV_SYSCALL_DECLARE();
RIV_VFS_DECLARE();

/* user-supplied context-switch stub for proc.h (real one would be asm) */
void riv_ctx_switch(riv_context *save, riv_context *load) { (void)save; (void)load; }

/* ----- syscall handlers ------------------------------------------------ */
static riv_i64 sys_write(riv_i64 fd, riv_i64 buf, riv_i64 n,
                         riv_i64 d, riv_i64 e, riv_i64 f) {
    (void)d; (void)e; (void)f;
    riv_proc *p = riv_proc_current();
    if (!p || !p->files) return -1;
    return riv_vfs_write(p->files, (int)fd, (const void*)(riv_uptr)buf, (riv_u32)n);
}
static riv_i64 sys_read(riv_i64 fd, riv_i64 buf, riv_i64 n,
                        riv_i64 d, riv_i64 e, riv_i64 f) {
    (void)d; (void)e; (void)f;
    riv_proc *p = riv_proc_current();
    if (!p || !p->files) return -1;
    return riv_vfs_read(p->files, (int)fd, (void*)(riv_uptr)buf, (riv_u32)n);
}
static riv_i64 sys_open(riv_i64 path, riv_i64 flags, riv_i64 c,
                        riv_i64 d, riv_i64 e, riv_i64 f) {
    (void)c; (void)d; (void)e; (void)f;
    riv_proc *p = riv_proc_current();
    if (!p || !p->files) return -1;
    return riv_vfs_open(p->files, (const char*)(riv_uptr)path, (riv_u32)flags);
}
static riv_i64 sys_getpid(riv_i64 a, riv_i64 b, riv_i64 c,
                          riv_i64 d, riv_i64 e, riv_i64 f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    return riv_proc_current() ? riv_proc_current()->pid : -1;
}
static riv_i64 sys_exit(riv_i64 code, riv_i64 b, riv_i64 c,
                        riv_i64 d, riv_i64 e, riv_i64 f) {
    (void)b; (void)c; (void)d; (void)e; (void)f;
    riv_proc_exit((int)code);
}
static riv_i64 sys_yield(riv_i64 a, riv_i64 b, riv_i64 c,
                         riv_i64 d, riv_i64 e, riv_i64 f) {
    (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
    riv_proc_yield();
    return 0;
}

/* ----- timer + page fault handlers ------------------------------------- */
static void timer_isr(riv_trap_frame *tf) {
    (void)tf;
    riv_tick();
    riv_sched_tick_irq();
}
static void page_fault(riv_trap_frame *tf) {
    riv_log_err("mm", "page fault @ pc=0x%p err=0x%x",
                (void*)tf->pc, (unsigned)tf->error);
    riv_panic("unrecoverable page fault");
}

/* ----- ramfs storage --------------------------------------------------- */
static riv_ramfs rootfs;
static struct riv_file_table init_files;

/* ----- demo "init" process body --------------------------------------- */
static void proc_init_body_inline(void) {
    int fd = riv_vfs_open(&init_files, "/hello.txt", RIV_O_RDWR | RIV_O_CREATE);
    if (fd >= 0) {
        const char *msg = "Hello from init process\n";
        riv_vfs_write(&init_files, fd, msg, (riv_u32)riv_strlen(msg));
        char buf[64];
        riv_vfs_lseek(&init_files, fd, 0, RIV_SEEK_SET);
        int n = riv_vfs_read(&init_files, fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = 0;
            riv_log_info("init", "read back: %s", buf);
        }
        riv_vfs_close(&init_files, fd);
    }
}

/* ----- PMM backing storage (1 MiB managed) ---------------------------- */
RIV_ALIGN(4096) static riv_u8 pmm_region[1024 * 1024];
static riv_u32 pmm_bitmap[1024 / 32];   /* 256 frames of 4 KiB = 1 MiB */
static riv_pmm pmm;

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

    riv_pmm_init(&pmm, pmm_bitmap, (riv_uptr)pmm_region, 256);
    riv_ramfs_init(&rootfs);
    riv_mount_fs("/", &rootfs.ops);

    riv_syscall_register(RIV_SYS_WRITE,  sys_write);
    riv_syscall_register(RIV_SYS_READ,   sys_read);
    riv_syscall_register(RIV_SYS_OPEN,   sys_open);
    riv_syscall_register(RIV_SYS_GETPID, sys_getpid);
    riv_syscall_register(RIV_SYS_EXIT,   sys_exit);
    riv_syscall_register(RIV_SYS_YIELD,  sys_yield);

    riv_trap_set(14, page_fault);
    riv_trap_set(32, timer_isr);

    riv_proc *init = riv_proc_alloc();
    if (!init) {
        riv_log_err("kernel", "no PCB slot");
        qemu_shutdown(1);
    }
    riv_strlcpy(init->name, "init", RIV_PROC_NAME_LEN);
    init->files = &init_files;
    riv_proc_ready(init);

    /* Exercise the user-mode "init" body inline to show VFS work. */
    proc_init_body_inline();

    riv_log_info("kernel", "RIVET v%d.%d boot ok",
                 RIVET_VERSION_MAJOR, RIVET_VERSION_MINOR);
    qemu_shutdown(0);
}
