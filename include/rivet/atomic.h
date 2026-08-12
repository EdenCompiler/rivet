/* RIVET — atomic.h : portable atomic primitives.
 * Uses GCC/Clang __atomic builtins (C11 model) when available, else
 * arch-specific asm fallbacks. Sequentially-consistent by default —
 * callers needing weaker orderings should use riv_atomic_*_explicit. */
#ifndef RIVET_ATOMIC_H
#define RIVET_ATOMIC_H

#include "core.h"

#if defined(__GNUC__) || defined(__clang__)

typedef struct { volatile riv_u32 v; } riv_atomic32;
typedef struct { volatile riv_uptr v; } riv_atomic_ptr;

RIV_ALWAYS riv_u32 riv_atomic_load (const riv_atomic32 *a) {
    return __atomic_load_n(&a->v, __ATOMIC_SEQ_CST);
}
RIV_ALWAYS void riv_atomic_store(riv_atomic32 *a, riv_u32 v) {
    __atomic_store_n(&a->v, v, __ATOMIC_SEQ_CST);
}
RIV_ALWAYS riv_u32 riv_atomic_add(riv_atomic32 *a, riv_u32 v) {
    return __atomic_fetch_add(&a->v, v, __ATOMIC_SEQ_CST);
}
RIV_ALWAYS riv_u32 riv_atomic_sub(riv_atomic32 *a, riv_u32 v) {
    return __atomic_fetch_sub(&a->v, v, __ATOMIC_SEQ_CST);
}
RIV_ALWAYS riv_u32 riv_atomic_or (riv_atomic32 *a, riv_u32 v) {
    return __atomic_fetch_or (&a->v, v, __ATOMIC_SEQ_CST);
}
RIV_ALWAYS riv_u32 riv_atomic_and(riv_atomic32 *a, riv_u32 v) {
    return __atomic_fetch_and(&a->v, v, __ATOMIC_SEQ_CST);
}
RIV_ALWAYS riv_u32 riv_atomic_xchg(riv_atomic32 *a, riv_u32 v) {
    return __atomic_exchange_n(&a->v, v, __ATOMIC_SEQ_CST);
}
/* compare_exchange — returns true on success; *expected is updated on fail */
RIV_ALWAYS int riv_atomic_cas(riv_atomic32 *a, riv_u32 *expected, riv_u32 desired) {
    return __atomic_compare_exchange_n(&a->v, expected, desired,
                                       0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

#define RIV_ATOMIC_INIT(x)  { (x) }

#else
#  error "rivet/atomic.h needs GCC/Clang"
#endif

#endif /* RIVET_ATOMIC_H */
