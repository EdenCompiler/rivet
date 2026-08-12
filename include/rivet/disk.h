/* RIVET — disk.h : block-device abstraction.
 *
 * Drivers for ATA, virtio-blk, SD/eMMC, etc. publish a riv_disk_ops
 * vtable. Block numbers are 0-based; block size is reported by the
 * driver but most users assume 512 bytes. */
#ifndef RIVET_DISK_H
#define RIVET_DISK_H

#include "core.h"

typedef struct riv_disk_ops {
    const char *name;
    riv_u32     block_size;        /* typically 512 */
    riv_u64     block_count;
    int  (*read) (struct riv_disk_ops *self, riv_u64 lba,
                  riv_u32 nblocks, void *out);
    int  (*write)(struct riv_disk_ops *self, riv_u64 lba,
                  riv_u32 nblocks, const void *in);
    int  (*flush)(struct riv_disk_ops *self);
} riv_disk_ops;

RIV_ALWAYS int riv_disk_read(riv_disk_ops *d, riv_u64 lba,
                              riv_u32 n, void *out) {
    return d && d->read ? d->read(d, lba, n, out) : -1;
}
RIV_ALWAYS int riv_disk_write(riv_disk_ops *d, riv_u64 lba,
                               riv_u32 n, const void *in) {
    return d && d->write ? d->write(d, lba, n, in) : -1;
}

#endif /* RIVET_DISK_H */
