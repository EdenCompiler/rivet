/* RIVET — crc.h : table-free CRC variants for boot/integrity checks. */
#ifndef RIVET_CRC_H
#define RIVET_CRC_H

#include "core.h"

/* CRC-32 (IEEE 802.3 / zlib) — bit-serial, no table. */
static inline riv_u32 riv_crc32_update(riv_u32 crc, const void *data, riv_size n) {
    const riv_u8 *p = (const riv_u8*)data;
    crc = ~crc;
    while (n--) {
        crc ^= *p++;
        for (int i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
    }
    return ~crc;
}
RIV_ALWAYS riv_u32 riv_crc32(const void *data, riv_size n) {
    return riv_crc32_update(0, data, n);
}

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF). */
static inline riv_u16 riv_crc16_update(riv_u16 crc, const void *data, riv_size n) {
    const riv_u8 *p = (const riv_u8*)data;
    while (n--) {
        crc ^= (riv_u16)*p++ << 8;
        for (int i = 0; i < 8; ++i)
            crc = (crc & 0x8000) ? (riv_u16)((crc << 1) ^ 0x1021) : (riv_u16)(crc << 1);
    }
    return crc;
}
RIV_ALWAYS riv_u16 riv_crc16(const void *data, riv_size n) {
    return riv_crc16_update(0xFFFF, data, n);
}

/* CRC-8/Maxim/Dallas (1-Wire). */
RIV_ALWAYS riv_u8 riv_crc8_1wire(const void *data, riv_size n) {
    const riv_u8 *p = (const riv_u8*)data;
    riv_u8 crc = 0;
    while (n--) {
        crc ^= *p++;
        for (int i = 0; i < 8; ++i)
            crc = (crc & 1) ? (riv_u8)((crc >> 1) ^ 0x8C) : (riv_u8)(crc >> 1);
    }
    return crc;
}

#endif /* RIVET_CRC_H */
