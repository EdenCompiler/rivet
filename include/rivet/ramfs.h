/* RIVET — ramfs.h : tiny in-memory file system, suitable for a boot
 * image or scratch FS. Files stored in a fixed-size pool of inodes,
 * each with a fixed maximum data size. Lookups are linear; intended
 * for very small file counts (<=32 typical). */
#ifndef RIVET_RAMFS_H
#define RIVET_RAMFS_H

#include "vfs.h"
#include "mem.h"

#define RAMFS_MAX_FILES   32
#define RAMFS_MAX_DATA    4096

typedef struct {
    riv_inode ino;
    riv_u8    data[RAMFS_MAX_DATA];
    int       used;
} ramfs_entry;

typedef struct {
    riv_fs_ops  ops;
    ramfs_entry table[RAMFS_MAX_FILES];
    riv_spin    lock;
} riv_ramfs;

static inline riv_inode *_ramfs_lookup(riv_fs_ops *fs, const char *path) {
    riv_ramfs *r = (riv_ramfs*)fs;
    riv_inode *out = (riv_inode*)0;
    spin_locked(&r->lock) {
        for (int i = 0; i < RAMFS_MAX_FILES; ++i) {
            if (!r->table[i].used) continue;
            if (riv_strcmp(r->table[i].ino.name, path) == 0) {
                out = &r->table[i].ino; break;
            }
        }
    }
    return out;
}

static inline riv_inode *_ramfs_create(riv_fs_ops *fs, const char *path, riv_u32 mode) {
    riv_ramfs *r = (riv_ramfs*)fs;
    riv_inode *out = (riv_inode*)0;
    spin_locked(&r->lock) {
        for (int i = 0; i < RAMFS_MAX_FILES; ++i) {
            if (r->table[i].used) continue;
            ramfs_entry *e = &r->table[i];
            e->used = 1;
            riv_strlcpy(e->ino.name, path, RIV_PATH_MAX);
            e->ino.size = 0;
            e->ino.mode = mode;
            e->ino.priv = e->data;
            e->ino.fs   = fs;
            out = &e->ino;
            break;
        }
    }
    return out;
}

static inline int _ramfs_read(riv_inode *ino, riv_u32 off, void *buf, riv_u32 n) {
    if (off >= ino->size) return 0;
    riv_u32 left = ino->size - off;
    if (n > left) n = left;
    riv_memcpy(buf, (const riv_u8*)ino->priv + off, n);
    return (int)n;
}

static inline int _ramfs_write(riv_inode *ino, riv_u32 off, const void *buf, riv_u32 n) {
    if (off + n > RAMFS_MAX_DATA) {
        if (off >= RAMFS_MAX_DATA) return -1;
        n = RAMFS_MAX_DATA - off;
    }
    riv_memcpy((riv_u8*)ino->priv + off, buf, n);
    if (off + n > ino->size) ino->size = off + n;
    return (int)n;
}

static inline int _ramfs_unlink(riv_fs_ops *fs, const char *path) {
    riv_ramfs *r = (riv_ramfs*)fs;
    int rc = -1;
    spin_locked(&r->lock) {
        for (int i = 0; i < RAMFS_MAX_FILES; ++i) {
            if (!r->table[i].used) continue;
            if (riv_strcmp(r->table[i].ino.name, path) == 0) {
                r->table[i].used = 0;
                rc = 0; break;
            }
        }
    }
    return rc;
}

RIV_ALWAYS void riv_ramfs_init(riv_ramfs *r) {
    r->ops.name   = "ramfs";
    r->ops.lookup = _ramfs_lookup;
    r->ops.create = _ramfs_create;
    r->ops.read   = _ramfs_read;
    r->ops.write  = _ramfs_write;
    r->ops.unlink = _ramfs_unlink;
    riv_spin_init(&r->lock);
    for (int i = 0; i < RAMFS_MAX_FILES; ++i) r->table[i].used = 0;
}

#endif /* RIVET_RAMFS_H */
