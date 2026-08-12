/* RIVET — bitmap.h : fixed-width bit array operations. */
#ifndef RIVET_BITMAP_H
#define RIVET_BITMAP_H

#include "core.h"

#define RIV_BITMAP_WORDS(nbits)  (((nbits) + 31) / 32)
#define RIV_BITMAP(name, nbits)  riv_u32 name[RIV_BITMAP_WORDS(nbits)]

RIV_ALWAYS void riv_bm_set  (riv_u32 *bm, riv_u32 i) { bm[i >> 5] |=  (1u << (i & 31)); }
RIV_ALWAYS void riv_bm_clear(riv_u32 *bm, riv_u32 i) { bm[i >> 5] &= ~(1u << (i & 31)); }
RIV_ALWAYS int  riv_bm_test (const riv_u32 *bm, riv_u32 i) {
    return (bm[i >> 5] >> (i & 31)) & 1;
}
RIV_ALWAYS void riv_bm_flip (riv_u32 *bm, riv_u32 i) { bm[i >> 5] ^=  (1u << (i & 31)); }

RIV_ALWAYS void riv_bm_zero(riv_u32 *bm, riv_u32 nbits) {
    riv_u32 w = RIV_BITMAP_WORDS(nbits);
    for (riv_u32 i = 0; i < w; ++i) bm[i] = 0;
}

/* find first set bit, returns index or nbits if none */
RIV_ALWAYS riv_u32 riv_bm_ffs(const riv_u32 *bm, riv_u32 nbits) {
    riv_u32 w = RIV_BITMAP_WORDS(nbits);
    for (riv_u32 i = 0; i < w; ++i) {
        if (bm[i]) return i * 32 + (riv_u32)__builtin_ctz(bm[i]);
    }
    return nbits;
}
/* find first zero bit */
RIV_ALWAYS riv_u32 riv_bm_ffz(const riv_u32 *bm, riv_u32 nbits) {
    riv_u32 w = RIV_BITMAP_WORDS(nbits);
    for (riv_u32 i = 0; i < w; ++i) {
        riv_u32 v = ~bm[i];
        if (v) {
            riv_u32 bit = i * 32 + (riv_u32)__builtin_ctz(v);
            return bit < nbits ? bit : nbits;
        }
    }
    return nbits;
}

/* population count */
RIV_ALWAYS riv_u32 riv_bm_popcount(const riv_u32 *bm, riv_u32 nbits) {
    riv_u32 w = RIV_BITMAP_WORDS(nbits), c = 0;
    for (riv_u32 i = 0; i < w; ++i) c += (riv_u32)__builtin_popcount(bm[i]);
    return c;
}

#endif /* RIVET_BITMAP_H */
