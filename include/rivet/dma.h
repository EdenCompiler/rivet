/* RIVET — dma.h : DMA channel abstraction.
 *
 * Models a single DMA channel with one outstanding transfer at a time.
 * Drivers register a `riv_dma_ops` per controller; channels are
 * addressed by numeric ID. Both linear and circular modes supported,
 * along with an optional completion callback. */
#ifndef RIVET_DMA_H
#define RIVET_DMA_H

#include "core.h"
#include "mem.h"

typedef enum {
    RIV_DMA_M2M = 0,    /* memory to memory */
    RIV_DMA_M2P = 1,    /* memory to peripheral */
    RIV_DMA_P2M = 2,    /* peripheral to memory */
} riv_dma_dir;

typedef enum {
    RIV_DMA_BURST_BYTE = 1,
    RIV_DMA_BURST_HALF = 2,
    RIV_DMA_BURST_WORD = 4,
} riv_dma_burst;

typedef struct {
    riv_uptr      src;
    riv_uptr      dst;
    riv_u32       len_bytes;
    riv_dma_dir   dir;
    riv_dma_burst burst;
    int           circular;
    int           src_inc;
    int           dst_inc;
} riv_dma_cfg;

typedef void (*riv_dma_done_fn)(unsigned channel, int err, void *ctx);

typedef struct riv_dma_ops {
    const char *name;
    int  (*start)  (struct riv_dma_ops *self, unsigned ch,
                    const riv_dma_cfg *cfg,
                    riv_dma_done_fn cb, void *ctx);
    int  (*stop)   (struct riv_dma_ops *self, unsigned ch);
    int  (*busy)   (struct riv_dma_ops *self, unsigned ch);
    int  (*remain) (struct riv_dma_ops *self, unsigned ch);
} riv_dma_ops;

RIV_ALWAYS int riv_dma_start(riv_dma_ops *d, unsigned ch,
                              const riv_dma_cfg *cfg,
                              riv_dma_done_fn cb, void *ctx) {
    return d && d->start ? d->start(d, ch, cfg, cb, ctx) : -1;
}

RIV_ALWAYS int riv_dma_wait(riv_dma_ops *d, unsigned ch) {
    if (!d || !d->busy) return -1;
    while (d->busy(d, ch)) riv_cpu_relax();
    return 0;
}

#endif /* RIVET_DMA_H */
