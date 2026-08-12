/* RIVET — buf.h : block-cache layer over a riv_disk_ops device.
 *
 * Caches a fixed number of N-byte blocks. Look-up is linear (small
 * cache sizes assumed). Provides clock-sweep eviction and dirty-write
 * back through the underlying disk vtable. */
#ifndef RIVET_BUF_H
#define RIVET_BUF_H

#include "core.h"
#include "disk.h"
#include "spin.h"
#include "mem.h"

#define RIV_BUF_BLOCKS  32
#define RIV_BUF_SZ      512

typedef struct {
    riv_u64  lba;
    riv_u8   data[RIV_BUF_SZ];
    riv_u32  refs;
    int      valid;
    int      dirty;
    riv_u32  last_use;
} riv_buf_block;

typedef struct {
    riv_disk_ops *disk;
    riv_buf_block blocks[RIV_BUF_BLOCKS];
    riv_u32       clock;
    riv_spin      lock;
} riv_buf_cache;

RIV_ALWAYS void riv_buf_init(riv_buf_cache *c, riv_disk_ops *d) {
    c->disk = d;
    c->clock = 0;
    riv_spin_init(&c->lock);
    for (int i = 0; i < RIV_BUF_BLOCKS; ++i) {
        c->blocks[i].lba   = 0;
        c->blocks[i].refs  = 0;
        c->blocks[i].valid = 0;
        c->blocks[i].dirty = 0;
    }
}

/* Acquire (read-into-cache) one block. Returns block pointer or NULL. */
RIV_ALWAYS riv_buf_block *riv_buf_get(riv_buf_cache *c, riv_u64 lba) {
    riv_buf_block *out = (riv_buf_block*)0;
    spin_locked(&c->lock) {
        /* hit */
        for (int i = 0; i < RIV_BUF_BLOCKS; ++i) {
            if (c->blocks[i].valid && c->blocks[i].lba == lba) {
                c->blocks[i].refs++;
                c->blocks[i].last_use = ++c->clock;
                out = &c->blocks[i];
                break;
            }
        }
        if (out) break;
        /* miss — evict a clean unreferenced block */
        for (int i = 0; i < RIV_BUF_BLOCKS; ++i) {
            riv_buf_block *b = &c->blocks[i];
            if (b->refs == 0 && !b->dirty) {
                if (riv_disk_read(c->disk, lba, 1, b->data) < 0) break;
                b->lba   = lba;
                b->refs  = 1;
                b->valid = 1;
                b->dirty = 0;
                b->last_use = ++c->clock;
                out = b;
                break;
            }
        }
    }
    return out;
}

RIV_ALWAYS void riv_buf_release(riv_buf_cache *c, riv_buf_block *b) {
    spin_locked(&c->lock) {
        if (b->refs > 0) b->refs--;
    }
}

RIV_ALWAYS void riv_buf_dirty(riv_buf_block *b) { b->dirty = 1; }

/* Flush all dirty blocks. */
RIV_ALWAYS int riv_buf_sync(riv_buf_cache *c) {
    int err = 0;
    spin_locked(&c->lock) {
        for (int i = 0; i < RIV_BUF_BLOCKS; ++i) {
            riv_buf_block *b = &c->blocks[i];
            if (!b->valid || !b->dirty) continue;
            if (riv_disk_write(c->disk, b->lba, 1, b->data) < 0) { err = -1; break; }
            b->dirty = 0;
        }
    }
    return err;
}

#endif /* RIVET_BUF_H */
