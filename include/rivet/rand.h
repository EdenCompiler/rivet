/* RIVET — rand.h : tiny pseudo-random number generators.
 *
 * Two flavors:
 *   - xorshift64*  : fast, decent quality, 64-bit state
 *   - splitmix64   : useful for seeding xorshift from a single u64
 *
 * Not cryptographically secure. For boot-time entropy, mix in
 * riv_cycle_count() or a hardware RNG source. */
#ifndef RIVET_RAND_H
#define RIVET_RAND_H

#include "core.h"

typedef struct { riv_u64 state; } riv_rng;

RIV_ALWAYS riv_u64 riv_splitmix64(riv_u64 *s) {
    *s += 0x9E3779B97F4A7C15ull;
    riv_u64 z = *s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

RIV_ALWAYS void riv_rng_seed(riv_rng *r, riv_u64 seed) {
    /* Run seed through splitmix once so a zero seed still works. */
    r->state = riv_splitmix64(&seed);
    if (r->state == 0) r->state = 1;
}

RIV_ALWAYS riv_u64 riv_rng_next(riv_rng *r) {
    riv_u64 x = r->state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    r->state = x;
    return x * 0x2545F4914F6CDD1Dull;
}

RIV_ALWAYS riv_u32 riv_rng_u32(riv_rng *r) { return (riv_u32)riv_rng_next(r); }

/* Uniform integer in [0, bound). Uses Lemire's bit-shift reduction. */
RIV_ALWAYS riv_u32 riv_rng_range(riv_rng *r, riv_u32 bound) {
    if (bound == 0) return 0;
    riv_u64 m = (riv_u64)riv_rng_u32(r) * bound;
    return (riv_u32)(m >> 32);
}

/* Floating-style helper without using doubles — returns a fixed-point
 * Q0.32 value in [0, 1). */
RIV_ALWAYS riv_u32 riv_rng_q32(riv_rng *r) { return riv_rng_u32(r); }

#endif /* RIVET_RAND_H */
