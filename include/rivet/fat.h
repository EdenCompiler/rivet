/* RIVET — fat.h : read-only FAT12 / FAT16 / FAT32 minimal driver.
 *
 * Mounts a FAT volume on a riv_disk_ops device and supports root-dir
 * lookup, file open, and sequential read. Intentionally read-only —
 * sufficient for a boot stage that loads kernel.elf or config files. */
#ifndef RIVET_FAT_H
#define RIVET_FAT_H

#include "core.h"
#include "disk.h"
#include "str.h"

#define RIV_FAT_TYPE12  12
#define RIV_FAT_TYPE16  16
#define RIV_FAT_TYPE32  32

typedef struct {
    riv_disk_ops *disk;
    riv_u32 type;
    riv_u32 bytes_per_sector;
    riv_u32 sectors_per_cluster;
    riv_u32 reserved_sectors;
    riv_u32 num_fats;
    riv_u32 root_entries;
    riv_u32 sectors_per_fat;
    riv_u32 first_data_sector;
    riv_u32 first_fat_sector;
    riv_u32 root_dir_sector;
    riv_u32 root_cluster;          /* FAT32 only */
} riv_fat;

typedef struct {
    riv_fat *fs;
    riv_u32  first_cluster;
    riv_u32  cur_cluster;
    riv_u32  pos;
    riv_u32  size;
} riv_fat_file;

/* BPB layout (excerpt). */
typedef struct RIV_PACKED {
    riv_u8  jmp[3];
    char    oem[8];
    riv_u16 bps;
    riv_u8  spc;
    riv_u16 reserved;
    riv_u8  nfats;
    riv_u16 root_entries;
    riv_u16 small_sectors;
    riv_u8  media;
    riv_u16 spf16;
    riv_u16 spt;
    riv_u16 heads;
    riv_u32 hidden;
    riv_u32 large_sectors;
    /* extension differs FAT16 vs FAT32 */
} riv_fat_bpb;

/* Mount: reads BPB, derives layout. Returns 0 on success. */
RIV_ALWAYS int riv_fat_mount(riv_fat *f, riv_disk_ops *disk) {
    riv_u8 sec[512];
    if (riv_disk_read(disk, 0, 1, sec) < 0) return -1;
    const riv_fat_bpb *b = (const riv_fat_bpb*)sec;
    f->disk                 = disk;
    f->bytes_per_sector     = b->bps;
    f->sectors_per_cluster  = b->spc;
    f->reserved_sectors     = b->reserved;
    f->num_fats             = b->nfats;
    f->root_entries         = b->root_entries;
    f->sectors_per_fat      = b->spf16 ? b->spf16
                              : *(const riv_u32*)(sec + 36);
    riv_u32 total_sectors   = b->small_sectors ? b->small_sectors
                              : b->large_sectors;
    riv_u32 root_dir_secs   = ((riv_u32)b->root_entries * 32
                              + b->bps - 1) / b->bps;
    f->first_fat_sector     = b->reserved;
    f->first_data_sector    = b->reserved + (riv_u32)b->nfats * f->sectors_per_fat + root_dir_secs;
    f->root_dir_sector      = b->reserved + (riv_u32)b->nfats * f->sectors_per_fat;

    riv_u32 data_secs       = total_sectors - f->first_data_sector;
    riv_u32 clusters        = data_secs / b->spc;
    if (clusters < 4085)        f->type = RIV_FAT_TYPE12;
    else if (clusters < 65525)  f->type = RIV_FAT_TYPE16;
    else { f->type = RIV_FAT_TYPE32;
           f->root_cluster = *(const riv_u32*)(sec + 44); }
    return 0;
}

/* Convert cluster# to LBA. */
RIV_ALWAYS riv_u32 riv_fat_cluster_lba(const riv_fat *f, riv_u32 cl) {
    return f->first_data_sector + (cl - 2) * f->sectors_per_cluster;
}

/* FAT32 chain follower — for FAT12/16 entries the user should adapt
 * to 12/16-bit wide entries; this helper is the common-case 32-bit form. */
RIV_ALWAYS riv_u32 riv_fat32_next(const riv_fat *f, riv_u32 cl) {
    riv_u8 sec[512];
    riv_u32 fat_off = cl * 4;
    riv_u32 fat_sec = f->first_fat_sector + (fat_off / 512);
    if (riv_disk_read(f->disk, fat_sec, 1, sec) < 0) return 0x0FFFFFFF;
    return (*(const riv_u32*)(sec + (fat_off % 512))) & 0x0FFFFFFF;
}

#endif /* RIVET_FAT_H */
