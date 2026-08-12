/* RIVET — heap.h : two allocators.
 *   1. Bump allocator (`riv_bump`) — fast, no free, perfect for boot
 *      stage / static lifetime objects.
 *   2. First-fit free-list allocator (`riv_heap`) — supports alloc/free
 *      with header-tagged blocks, suitable for small dynamic needs. */
#ifndef RIVET_HEAP_H
#define RIVET_HEAP_H

#include "core.h"
#include "mem.h"

/* ---------- bump ---------- */
typedef struct {
    riv_u8 *base, *cur, *end;
} riv_bump;

RIV_ALWAYS void riv_bump_init(riv_bump *b, void *mem, riv_size n) {
    b->base = b->cur = (riv_u8*)mem;
    b->end = (riv_u8*)mem + n;
}
RIV_ALWAYS void *riv_bump_alloc(riv_bump *b, riv_size size, riv_size align) {
    riv_uptr p = (riv_uptr)b->cur;
    riv_uptr a = (p + align - 1) & ~(align - 1);
    if (a + size > (riv_uptr)b->end) return (void*)0;
    b->cur = (riv_u8*)(a + size);
    return (void*)a;
}
RIV_ALWAYS void riv_bump_reset(riv_bump *b) { b->cur = b->base; }

/* ---------- first-fit free-list ---------- */
typedef struct riv_heap_blk {
    riv_size sz;                   /* payload size in bytes */
    struct riv_heap_blk *next;     /* free list link (only when free) */
    int free;
} riv_heap_blk;

typedef struct {
    riv_heap_blk *head;
} riv_heap;

#define RIV_HEAP_ALIGN 8
#define RIV_HEAP_HDR   sizeof(riv_heap_blk)

RIV_ALWAYS void riv_heap_init(riv_heap *h, void *mem, riv_size n) {
    riv_heap_blk *b = (riv_heap_blk*)mem;
    b->sz = n - RIV_HEAP_HDR;
    b->next = (riv_heap_blk*)0;
    b->free = 1;
    h->head = b;
}

RIV_ALWAYS void *riv_heap_alloc(riv_heap *h, riv_size sz) {
    sz = (sz + RIV_HEAP_ALIGN - 1) & ~(riv_size)(RIV_HEAP_ALIGN - 1);
    for (riv_heap_blk *b = h->head; b; b = b->next) {
        if (!b->free || b->sz < sz) continue;
        if (b->sz >= sz + RIV_HEAP_HDR + RIV_HEAP_ALIGN) {
            riv_heap_blk *n = (riv_heap_blk*)((riv_u8*)b + RIV_HEAP_HDR + sz);
            n->sz   = b->sz - sz - RIV_HEAP_HDR;
            n->next = b->next;
            n->free = 1;
            b->sz   = sz;
            b->next = n;
        }
        b->free = 0;
        return (riv_u8*)b + RIV_HEAP_HDR;
    }
    return (void*)0;
}

RIV_ALWAYS void riv_heap_free(riv_heap *h, void *p) {
    if (!p) return;
    riv_heap_blk *b = (riv_heap_blk*)((riv_u8*)p - RIV_HEAP_HDR);
    b->free = 1;
    /* coalesce adjacent free blocks */
    riv_heap_blk *prev = (riv_heap_blk*)0;
    for (riv_heap_blk *it = h->head; it; prev = it, it = it->next) {
        if (it->free && it->next && it->next->free) {
            it->sz += RIV_HEAP_HDR + it->next->sz;
            it->next = it->next->next;
        }
    }
    (void)prev;
}

#endif /* RIVET_HEAP_H */
