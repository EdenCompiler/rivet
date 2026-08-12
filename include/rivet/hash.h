/* RIVET — hash.h : open-addressed hash table with linear probing.
 *
 * Stores fixed-size string keys → uintptr values. Capacity is fixed
 * (chosen at init time), no dynamic resize — appropriate for kernel
 * tables of bounded cardinality (mount points, device names, etc).
 *
 * Hash function is FNV-1a 64. */
#ifndef RIVET_HASH_H
#define RIVET_HASH_H

#include "core.h"
#include "str.h"

#define RIV_HASH_KEYLEN 32

typedef struct {
    char     key[RIV_HASH_KEYLEN];
    riv_uptr val;
    int      used;
} riv_hash_slot;

typedef struct {
    riv_hash_slot *slots;
    riv_u32        cap;       /* power-of-two */
    riv_u32        count;
} riv_hash;

RIV_ALWAYS riv_u64 riv_hash_fnv64(const char *s) {
    riv_u64 h = 0xcbf29ce484222325ull;
    while (*s) {
        h ^= (riv_u8)*s++;
        h *= 0x100000001b3ull;
    }
    return h;
}

RIV_ALWAYS int riv_hash_init(riv_hash *h, riv_hash_slot *slots, riv_u32 cap_pow2) {
    if (!cap_pow2 || (cap_pow2 & (cap_pow2 - 1))) return -1;
    h->slots = slots;
    h->cap   = cap_pow2;
    h->count = 0;
    for (riv_u32 i = 0; i < cap_pow2; ++i) slots[i].used = 0;
    return 0;
}

/* Insert or overwrite. Returns 0 on success, -1 if table full. */
RIV_ALWAYS int riv_hash_set(riv_hash *h, const char *key, riv_uptr val) {
    riv_u64 i = riv_hash_fnv64(key) & (h->cap - 1);
    for (riv_u32 step = 0; step < h->cap; ++step) {
        riv_hash_slot *s = &h->slots[i];
        if (!s->used) {
            riv_strlcpy(s->key, key, RIV_HASH_KEYLEN);
            s->val  = val;
            s->used = 1;
            h->count++;
            return 0;
        }
        if (riv_strcmp(s->key, key) == 0) { s->val = val; return 0; }
        i = (i + 1) & (h->cap - 1);
    }
    return -1;
}

/* Lookup: returns 1 and writes *out on hit, 0 on miss. */
RIV_ALWAYS int riv_hash_get(const riv_hash *h, const char *key, riv_uptr *out) {
    riv_u64 i = riv_hash_fnv64(key) & (h->cap - 1);
    for (riv_u32 step = 0; step < h->cap; ++step) {
        const riv_hash_slot *s = &h->slots[i];
        if (!s->used) return 0;
        if (riv_strcmp(s->key, key) == 0) { if (out) *out = s->val; return 1; }
        i = (i + 1) & (h->cap - 1);
    }
    return 0;
}

/* Tombstone-free delete: shift the probe cluster following the hole. */
RIV_ALWAYS int riv_hash_del(riv_hash *h, const char *key) {
    riv_u64 i = riv_hash_fnv64(key) & (h->cap - 1);
    riv_u32 step;
    for (step = 0; step < h->cap; ++step) {
        riv_hash_slot *s = &h->slots[i];
        if (!s->used) return -1;
        if (riv_strcmp(s->key, key) == 0) break;
        i = (i + 1) & (h->cap - 1);
    }
    if (step == h->cap) return -1;

    riv_u64 hole = i;
    for (;;) {
        riv_u64 next = (hole + 1) & (h->cap - 1);
        riv_hash_slot *ns = &h->slots[next];
        if (!ns->used) { h->slots[hole].used = 0; break; }
        riv_u64 desired = riv_hash_fnv64(ns->key) & (h->cap - 1);
        /* Move only if shifting would keep the cluster contiguous. */
        if ((next > hole && (desired <= hole || desired > next))
         || (next < hole && (desired <= hole && desired > next))) {
            h->slots[hole] = *ns;
            hole = next;
        } else {
            h->slots[hole].used = 0;
            break;
        }
    }
    h->count--;
    return 0;
}

#endif /* RIVET_HASH_H */
