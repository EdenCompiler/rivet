/* RIVET — flash.h : NOR/SPI flash erase + program + read.
 *
 * Erase granularity is sector-size (driver-defined); program/read are
 * byte-addressable. Implementations may serialize internally — caller
 * just polls `is_busy` if needed. */
#ifndef RIVET_FLASH_H
#define RIVET_FLASH_H

#include "core.h"

typedef struct {
    riv_u32 page_size;        /* program granularity */
    riv_u32 sector_size;      /* erase granularity */
    riv_u32 total_size;
    riv_u8  erase_value;      /* 0xFF for NOR, 0x00 for NAND */
} riv_flash_info;

typedef struct riv_flash_ops {
    const char *name;
    int  (*info)    (struct riv_flash_ops *self, riv_flash_info *out);
    int  (*read)    (struct riv_flash_ops *self, riv_u32 off,
                     void *buf, riv_u32 n);
    int  (*program) (struct riv_flash_ops *self, riv_u32 off,
                     const void *buf, riv_u32 n);
    int  (*erase)   (struct riv_flash_ops *self, riv_u32 off, riv_u32 n);
    int  (*is_busy) (struct riv_flash_ops *self);
} riv_flash_ops;

RIV_ALWAYS int riv_flash_read(riv_flash_ops *f, riv_u32 off,
                               void *buf, riv_u32 n) {
    return f && f->read ? f->read(f, off, buf, n) : -1;
}
RIV_ALWAYS int riv_flash_program(riv_flash_ops *f, riv_u32 off,
                                  const void *buf, riv_u32 n) {
    return f && f->program ? f->program(f, off, buf, n) : -1;
}
RIV_ALWAYS int riv_flash_erase(riv_flash_ops *f, riv_u32 off, riv_u32 n) {
    return f && f->erase ? f->erase(f, off, n) : -1;
}

#endif /* RIVET_FLASH_H */
