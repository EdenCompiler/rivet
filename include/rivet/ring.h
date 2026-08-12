/* RIVET — ring.h : single-producer / single-consumer lockless byte ring.
 * Capacity must be a power of two. Producer touches `head`, consumer
 * touches `tail`. Use atomic loads/stores + barriers for cross-context. */
#ifndef RIVET_RING_H
#define RIVET_RING_H

#include "core.h"
#include "mem.h"
#include "atomic.h"

typedef struct {
    riv_u8       *buf;
    riv_u32       mask;          /* capacity - 1; capacity is pow2 */
    riv_atomic32  head;          /* write index (producer) */
    riv_atomic32  tail;          /* read index  (consumer) */
} riv_ring;

RIV_ALWAYS int riv_ring_init(riv_ring *r, riv_u8 *buf, riv_u32 cap_pow2) {
    if (cap_pow2 == 0 || (cap_pow2 & (cap_pow2 - 1))) return -1;
    r->buf = buf;
    r->mask = cap_pow2 - 1;
    riv_atomic_store(&r->head, 0);
    riv_atomic_store(&r->tail, 0);
    return 0;
}

RIV_ALWAYS riv_u32 riv_ring_count(const riv_ring *r) {
    riv_u32 h = riv_atomic_load((riv_atomic32*)&r->head);
    riv_u32 t = riv_atomic_load((riv_atomic32*)&r->tail);
    return h - t;
}
RIV_ALWAYS int riv_ring_empty(const riv_ring *r) { return riv_ring_count(r) == 0; }
RIV_ALWAYS int riv_ring_full (const riv_ring *r) { return riv_ring_count(r) > r->mask; }

/* push one byte; returns 1 on success, 0 if full */
RIV_ALWAYS int riv_ring_push(riv_ring *r, riv_u8 b) {
    riv_u32 h = riv_atomic_load(&r->head);
    riv_u32 t = riv_atomic_load(&r->tail);
    if ((h - t) > r->mask) return 0;
    r->buf[h & r->mask] = b;
    riv_wmb();
    riv_atomic_store(&r->head, h + 1);
    return 1;
}

/* pop one byte into *out; returns 1 on success, 0 if empty */
RIV_ALWAYS int riv_ring_pop(riv_ring *r, riv_u8 *out) {
    riv_u32 t = riv_atomic_load(&r->tail);
    riv_u32 h = riv_atomic_load(&r->head);
    if (h == t) return 0;
    *out = r->buf[t & r->mask];
    riv_rmb();
    riv_atomic_store(&r->tail, t + 1);
    return 1;
}

/* bulk push — returns number of bytes actually written */
RIV_ALWAYS riv_u32 riv_ring_write(riv_ring *r, const riv_u8 *src, riv_u32 n) {
    riv_u32 i;
    for (i = 0; i < n; ++i) if (!riv_ring_push(r, src[i])) break;
    return i;
}
/* bulk pop — returns number of bytes actually read */
RIV_ALWAYS riv_u32 riv_ring_read(riv_ring *r, riv_u8 *dst, riv_u32 n) {
    riv_u32 i;
    for (i = 0; i < n; ++i) if (!riv_ring_pop(r, &dst[i])) break;
    return i;
}

#endif /* RIVET_RING_H */
