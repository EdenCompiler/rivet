/* RIVET — bare.h : bare-metal-flavored DSL extensions.
 *
 * Adds higher-level macros that read like native keywords for embedded
 * and OS work: scoped IRQ blocks, MMIO region declarations, kernel
 * threads, deferred cleanup, instruction-level intrinsics, init/exit
 * sections, and a handful of useful attributes.
 *
 * Builds on the lower-level primitives in core.h / irq.h / mem.h. */
#ifndef RIVET_BARE_H
#define RIVET_BARE_H

#include "core.h"
#include "irq.h"
#include "mem.h"

/* --------------------------- attributes ----------------------------- */
#if defined(__GNUC__) || defined(__clang__)
#  define RIV_MUST_CHECK   __attribute__((warn_unused_result))
#  define RIV_PURE         __attribute__((pure))
#  define RIV_CONST        __attribute__((const))
#  define RIV_HOT          __attribute__((hot))
#  define RIV_COLD         __attribute__((cold))
#  define RIV_CLEANUP(fn)  __attribute__((cleanup(fn)))
#  define RIV_PRINTF_LIKE(fmt, va) __attribute__((format(printf, fmt, va)))
#else
#  define RIV_MUST_CHECK
#  define RIV_PURE
#  define RIV_CONST
#  define RIV_HOT
#  define RIV_COLD
#  define RIV_CLEANUP(fn)
#  define RIV_PRINTF_LIKE(fmt, va)
#endif

/* --------------------------- section keywords ----------------------- */
#define init_func        RIV_USED RIV_SECTION(".init.text")
#define exit_func        RIV_USED RIV_SECTION(".exit.text")
#define early_func       RIV_USED RIV_SECTION(".text.early")
#define irq_func         RIV_USED RIV_SECTION(".text.irq")
#define hot_func         RIV_USED RIV_SECTION(".text.hot")  RIV_HOT
#define cold_func        RIV_USED RIV_SECTION(".text.cold") RIV_COLD
#define persistent_data  RIV_USED RIV_SECTION(".data.persist")
#define no_init_data     RIV_USED RIV_SECTION(".bss.noinit")

#define place_at(addr)   RIV_USED __attribute__((section(".fixed_" #addr)))
#define aligned_as(n)    RIV_ALIGN(n)

/* --------------------------- scoped IRQ block ----------------------- */
/* Equivalent to `critical_section` (from irq.h) — provided here under
 * a name closer to common embedded-RTOS vocabulary. */
#define irq_safe         critical_section

/* --------------------------- scoped cleanup (defer) ----------------- */
/* Use as:
 *     static void cleanup_lock(riv_spin **p) { riv_spin_unlock(*p); }
 *     riv_spin *l defer(cleanup_lock) = &my_lock;
 *     riv_spin_lock(l);
 * Variable is automatically passed through cleanup_lock on scope exit. */
#define defer(fn)        RIV_CLEANUP(fn)

/* --------------------------- instruction intrinsics ---------------- */
RIV_ALWAYS void riv_pause(void) { riv_cpu_relax(); }

#if RIVET_ARCH_AARCH64 || RIVET_ARCH_ARM
RIV_ALWAYS void riv_wfe(void) { __asm__ __volatile__("wfe" ::: "memory"); }
RIV_ALWAYS void riv_sev(void) { __asm__ __volatile__("sev" ::: "memory"); }
RIV_ALWAYS void riv_dmb(void) { __asm__ __volatile__("dmb sy" ::: "memory"); }
RIV_ALWAYS void riv_dsb(void) { __asm__ __volatile__("dsb sy" ::: "memory"); }
RIV_ALWAYS void riv_isb(void) { __asm__ __volatile__("isb" ::: "memory"); }
#else
RIV_ALWAYS void riv_wfe(void) { riv_cpu_halt(); }
RIV_ALWAYS void riv_sev(void) { /* no-op outside ARM */ }
RIV_ALWAYS void riv_dmb(void) { riv_mb(); }
RIV_ALWAYS void riv_dsb(void) { riv_mb(); }
RIV_ALWAYS void riv_isb(void) { __asm__ __volatile__("" ::: "memory"); }
#endif

/* --------------------------- compile-time helpers ------------------- */
#define static_assert_size(type, n) \
    RIV_STATIC_ASSERT(sizeof(type) == (n), type##_must_be_##n##_bytes)

#define panic_on(cond, msg) do { \
        if (RIV_UNLIKELY(cond)) riv_panic(msg); \
    } while (0)

#define unreachable_default(switch_var) \
    default: riv_panic("unreachable switch"); (void)(switch_var); break

/* --------------------------- typed MMIO register -------------------- */
/* Declares a single 32-bit MMIO register at `addr` and produces inline
 * read/write helpers `name_read()` / `name_write(v)`. */
#define mmio_reg32(name, addr) \
    RIV_ALWAYS riv_u32 name##_read(void) { \
        return *(volatile riv_u32*)(riv_uptr)(addr); \
    } \
    RIV_ALWAYS void name##_write(riv_u32 v) { \
        *(volatile riv_u32*)(riv_uptr)(addr) = v; \
    }

#define mmio_reg16(name, addr) \
    RIV_ALWAYS riv_u16 name##_read(void) { \
        return *(volatile riv_u16*)(riv_uptr)(addr); \
    } \
    RIV_ALWAYS void name##_write(riv_u16 v) { \
        *(volatile riv_u16*)(riv_uptr)(addr) = v; \
    }

#define mmio_reg8(name, addr) \
    RIV_ALWAYS riv_u8 name##_read(void) { \
        return *(volatile riv_u8*)(riv_uptr)(addr); \
    } \
    RIV_ALWAYS void name##_write(riv_u8 v) { \
        *(volatile riv_u8*)(riv_uptr)(addr) = v; \
    }

/* --------------------------- kernel thread -------------------------- */
/* Declare and register a kernel thread entry function. Body runs as a
 * standalone task with no arguments; uses the cooperative task.h
 * machinery. Caller must still init the scheduler and tick source. */
#define kthread(name) \
    static void name(void *_unused_ctx_)

#define register_kthread(name, period_ticks) \
    { .run = (name), .ctx = (void*)0, \
      .period_ticks = (period_ticks), .next_tick = 0, .enabled = RIV_TRUE }

/* --------------------------- module lifecycle ----------------------- */
/* Linux-style `module_init(fn)` / `module_exit(fn)`: place fn pointer
 * in the `.init.fns` / `.exit.fns` array which the boot stub walks. */
typedef void (*riv_module_fn)(void);

#define module_init(fn) \
    static const riv_module_fn _riv_modinit_##fn \
        __attribute__((used, section(".init.fns"))) = fn

#define module_exit(fn) \
    static const riv_module_fn _riv_modexit_##fn \
        __attribute__((used, section(".exit.fns"))) = fn

/* --------------------------- device registry ------------------------ */
typedef struct riv_device_ops riv_device_ops;
struct riv_device_ops {
    const char *name;
    int  (*probe) (struct riv_device_ops *self);
    int  (*remove)(struct riv_device_ops *self);
    void *priv;
};

/* Place a `struct riv_device_ops *` pointer in the `.dev.tbl` section so
 * a generic enumerator can walk every device at boot time. `tag` is an
 * arbitrary identifier used to name the placement symbol — it just has
 * to be unique within the translation unit. */
#define device_register(tag, ops_ptr) \
    static struct riv_device_ops *_riv_dev_##tag \
        __attribute__((used, section(".dev.tbl"))) = (ops_ptr)

#endif /* RIVET_BARE_H */
