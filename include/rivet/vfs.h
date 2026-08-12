/* RIVET — vfs.h : virtual file system interface.
 *
 * Generic file abstraction with a small vtable. Specific filesystems
 * (ramfs, devfs) plug into the global mount table.
 *
 * Per-process open file table is referenced from riv_proc.files. */
#ifndef RIVET_VFS_H
#define RIVET_VFS_H

#include "core.h"
#include "str.h"
#include "spin.h"

#define RIV_PATH_MAX     128
#define RIV_FILES_MAX    16          /* per process */
#define RIV_MOUNTS_MAX   8

#define RIV_O_RDONLY     0x0
#define RIV_O_WRONLY     0x1
#define RIV_O_RDWR       0x2
#define RIV_O_CREATE     0x40
#define RIV_O_TRUNC      0x200
#define RIV_O_APPEND     0x400

#define RIV_SEEK_SET     0
#define RIV_SEEK_CUR     1
#define RIV_SEEK_END     2

typedef struct riv_inode riv_inode;
typedef struct riv_file_ops riv_file_ops;
typedef struct riv_fs_ops   riv_fs_ops;

struct riv_inode {
    char        name[RIV_PATH_MAX];
    riv_u32     size;
    riv_u32     mode;
    void       *priv;         /* filesystem-specific */
    riv_fs_ops *fs;
};

typedef struct {
    riv_inode *ino;
    riv_u32    pos;
    riv_u32    flags;
    int        used;
} riv_file;

struct riv_file_table {
    riv_file files[RIV_FILES_MAX];
};

struct riv_fs_ops {
    const char *name;
    riv_inode *(*lookup)(struct riv_fs_ops *fs, const char *path);
    riv_inode *(*create)(struct riv_fs_ops *fs, const char *path, riv_u32 mode);
    int  (*read) (riv_inode *ino, riv_u32 off, void *buf, riv_u32 n);
    int  (*write)(riv_inode *ino, riv_u32 off, const void *buf, riv_u32 n);
    int  (*unlink)(struct riv_fs_ops *fs, const char *path);
};

typedef struct {
    char        mount_point[RIV_PATH_MAX];
    riv_fs_ops *fs;
} riv_mount;

extern riv_mount riv_mounts[RIV_MOUNTS_MAX];
extern int       riv_nmount;
extern riv_spin  riv_vfs_lock;

#define RIV_VFS_DECLARE() \
    riv_mount riv_mounts[RIV_MOUNTS_MAX] = {{ {0}, (riv_fs_ops*)0 }}; \
    int       riv_nmount = 0; \
    riv_spin  riv_vfs_lock = RIV_SPIN_INIT

RIV_ALWAYS int riv_mount_fs(const char *path, riv_fs_ops *fs) {
    int rc = -1;
    spin_locked(&riv_vfs_lock) {
        if (riv_nmount >= RIV_MOUNTS_MAX) break;
        riv_strlcpy(riv_mounts[riv_nmount].mount_point, path, RIV_PATH_MAX);
        riv_mounts[riv_nmount].fs = fs;
        riv_nmount++;
        rc = 0;
    }
    return rc;
}

RIV_ALWAYS riv_fs_ops *_riv_match_mount(const char *path, const char **rel) {
    riv_fs_ops *best = (riv_fs_ops*)0;
    riv_size best_len = 0;
    for (int i = 0; i < riv_nmount; ++i) {
        riv_size L = riv_strlen(riv_mounts[i].mount_point);
        if (riv_strncmp(path, riv_mounts[i].mount_point, L) == 0 && L >= best_len) {
            best = riv_mounts[i].fs;
            best_len = L;
            *rel = path + L;
            if (**rel == '/') (*rel)++;
        }
    }
    return best;
}

/* Per-process FD-table helpers. Caller passes the table (or current proc's). */
RIV_ALWAYS int riv_file_alloc_fd(struct riv_file_table *t) {
    if (!t) return -1;
    for (int i = 0; i < RIV_FILES_MAX; ++i)
        if (!t->files[i].used) { t->files[i].used = 1; return i; }
    return -1;
}

RIV_ALWAYS int riv_vfs_open(struct riv_file_table *t, const char *path, riv_u32 flags) {
    const char *rel = path;
    riv_fs_ops *fs = _riv_match_mount(path, &rel);
    if (!fs) return -1;
    riv_inode *ino = fs->lookup(fs, rel);
    if (!ino) {
        if (!(flags & RIV_O_CREATE)) return -1;
        ino = fs->create(fs, rel, 0);
        if (!ino) return -1;
    }
    int fd = riv_file_alloc_fd(t);
    if (fd < 0) return -1;
    t->files[fd].ino   = ino;
    t->files[fd].pos   = (flags & RIV_O_APPEND) ? ino->size : 0;
    t->files[fd].flags = flags;
    return fd;
}

RIV_ALWAYS int riv_vfs_close(struct riv_file_table *t, int fd) {
    if (!t || fd < 0 || fd >= RIV_FILES_MAX || !t->files[fd].used) return -1;
    t->files[fd].used = 0;
    t->files[fd].ino  = (riv_inode*)0;
    return 0;
}

RIV_ALWAYS int riv_vfs_read(struct riv_file_table *t, int fd, void *buf, riv_u32 n) {
    if (!t || fd < 0 || fd >= RIV_FILES_MAX || !t->files[fd].used) return -1;
    riv_file *f = &t->files[fd];
    int r = f->ino->fs->read(f->ino, f->pos, buf, n);
    if (r > 0) f->pos += (riv_u32)r;
    return r;
}

RIV_ALWAYS int riv_vfs_write(struct riv_file_table *t, int fd,
                              const void *buf, riv_u32 n) {
    if (!t || fd < 0 || fd >= RIV_FILES_MAX || !t->files[fd].used) return -1;
    riv_file *f = &t->files[fd];
    int r = f->ino->fs->write(f->ino, f->pos, buf, n);
    if (r > 0) f->pos += (riv_u32)r;
    return r;
}

RIV_ALWAYS riv_i64 riv_vfs_lseek(struct riv_file_table *t, int fd,
                                  riv_i64 off, int whence) {
    if (!t || fd < 0 || fd >= RIV_FILES_MAX || !t->files[fd].used) return -1;
    riv_file *f = &t->files[fd];
    riv_u32 newpos = 0;
    if (whence == RIV_SEEK_SET) newpos = (riv_u32)off;
    else if (whence == RIV_SEEK_CUR) newpos = f->pos + (riv_u32)off;
    else if (whence == RIV_SEEK_END) newpos = f->ino->size + (riv_u32)off;
    else return -1;
    f->pos = newpos;
    return (riv_i64)newpos;
}

#endif /* RIVET_VFS_H */
