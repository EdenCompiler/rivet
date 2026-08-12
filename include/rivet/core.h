/* RIVET — core.h : base types, attributes, compile-time helpers */
#ifndef RIVET_CORE_H
#define RIVET_CORE_H

#include "arch.h"

/* freestanding-friendly fixed-width types (no <stdint.h> required) */
typedef unsigned char       riv_u8;
typedef signed   char       riv_i8;
typedef unsigned short      riv_u16;
typedef signed   short      riv_i16;
typedef unsigned int        riv_u32;
typedef signed   int        riv_i32;
typedef unsigned long long  riv_u64;
typedef signed   long long  riv_i64;

#if RIVET_WORD_BITS == 64
typedef riv_u64 riv_uptr;
typedef riv_i64 riv_iptr;
typedef riv_u64 riv_size;
#else
typedef riv_u32 riv_uptr;
typedef riv_i32 riv_iptr;
typedef riv_u32 riv_size;
#endif

typedef unsigned char riv_bool;
#define RIV_TRUE  ((riv_bool)1)
#define RIV_FALSE ((riv_bool)0)
#define RIV_NULL  ((void*)0)

/* attributes — DSL keywords */
#if defined(__GNUC__) || defined(__clang__)
#  define RIV_NAKED        __attribute__((naked))
#  define RIV_NOINLINE     __attribute__((noinline))
#  define RIV_ALWAYS       static inline __attribute__((always_inline))
#  define RIV_PACKED       __attribute__((packed))
#  define RIV_ALIGN(n)     __attribute__((aligned(n)))
#  define RIV_SECTION(s)   __attribute__((section(s)))
#  define RIV_USED         __attribute__((used))
#  define RIV_UNUSED       __attribute__((unused))
#  define RIV_NORETURN     __attribute__((noreturn))
#  define RIV_WEAK         __attribute__((weak))
#  define RIV_INTERRUPT    __attribute__((interrupt))
#  define RIV_LIKELY(x)    __builtin_expect(!!(x), 1)
#  define RIV_UNLIKELY(x)  __builtin_expect(!!(x), 0)
#else
#  define RIV_NAKED
#  define RIV_NOINLINE
#  define RIV_ALWAYS       static inline
#  define RIV_PACKED
#  define RIV_ALIGN(n)
#  define RIV_SECTION(s)
#  define RIV_USED
#  define RIV_UNUSED
#  define RIV_NORETURN
#  define RIV_WEAK
#  define RIV_INTERRUPT
#  define RIV_LIKELY(x)    (x)
#  define RIV_UNLIKELY(x)  (x)
#endif

/* compile-time assert (C99) */
#define RIV_STATIC_ASSERT(cond, name) \
    typedef char riv_static_assert_##name[(cond) ? 1 : -1]

/* helpers */
#define RIV_ARRLEN(a)        (sizeof(a) / sizeof((a)[0]))
#define RIV_MIN(a,b)         ((a) < (b) ? (a) : (b))
#define RIV_MAX(a,b)         ((a) > (b) ? (a) : (b))
#define RIV_BIT(n)           (1u << (n))
#define RIV_MASK(hi,lo)      ((~0u << (lo)) & (~0u >> (31 - (hi))))
#define RIV_ALIGN_UP(x,a)    (((x) + ((a) - 1)) & ~((a) - 1))
#define RIV_ALIGN_DOWN(x,a)  ((x) & ~((a) - 1))
#define RIV_CONTAINER_OF(ptr, type, member) \
    ((type*)((char*)(ptr) - (riv_uptr)&((type*)0)->member))

/* DSL-style keywords (macros).
 * `isr` is intentionally a plain export — the compiler's `interrupt`
 * attribute requires arch-specific signatures (esp. x86-64), so we
 * leave that opt-in via `isr_attr` for users who want it. */
#define kernel_entry    RIV_USED RIV_SECTION(".text.entry")
#define isr             RIV_USED
#define isr_attr        RIV_USED RIV_INTERRUPT
#define naked           RIV_NAKED
#define packed_struct   struct RIV_PACKED
#define forever         for(;;)
#define unreachable()   __builtin_unreachable()

#endif /* RIVET_CORE_H */
