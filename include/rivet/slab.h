/* RIVET — slab.h : fixed-size-object slab allocator.
 *
 * Cache of equal-sized objects carved out of a backing arena. Free
 * objects are threaded onto a singly-linked list for O(1) alloc/free.
 * Caller supplies the storage; no dynamic OS memory.
 *
 * Object size must be at least `sizeof(void*)` so the free-list link
 * can be embedded. Suitable for PCB pools, mbufs, dirent caches, etc. */
#ifndef RIVET_SLAB_H
#define RIVET_SLAB_H

#include "core.h"

typedef struct riv_slab_link { struct riv_slab_link *next; } riv_slab_link;

typedef struct {
    riv_u8        *base;
    riv_size       obj_size;
    riv_u32        obj_count;
    riv_slab_link *free_list;
    riv_u32        free_count;
} riv_slab;

RIV_ALWAYS int riv_slab_init(riv_slab *s, void *backing,
                              riv_size obj_size, riv_u32 obj_count) {
    if (obj_size < sizeof(riv_slab_link)) return -1;
    s->base       = (riv_u8*)backing;
    s->obj_size   = obj_size;
    s->obj_count  = obj_count;
    s->free_count = obj_count;
    s->free_list  = (riv_slab_link*)0;
    for (riv_u32 i = 0; i < obj_count; ++i) {
        riv_slab_link *l = (riv_slab_link*)(s->base + (riv_size)i * obj_size);
        l->next = s->free_list;
        s->free_list = l;
    }
    return 0;
}

RIV_ALWAYS void *riv_slab_alloc(riv_slab *s) {
    if (!s->free_list) return (void*)0;
    riv_slab_link *l = s->free_list;
    s->free_list = l->next;
    s->free_count--;
    return l;
}

RIV_ALWAYS void riv_slab_free(riv_slab *s, void *p) {
    if (!p) return;
    riv_slab_link *l = (riv_slab_link*)p;
    l->next = s->free_list;
    s->free_list = l;
    s->free_count++;
}

#endif /* RIVET_SLAB_H */
