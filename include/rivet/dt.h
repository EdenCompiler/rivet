/* RIVET — dt.h : flat device tree (FDT) reader.
 *
 * Reads the on-disk DTB blob format used by ARM/RISC-V boot stubs.
 * Provides walk + property lookup primitives without dynamic memory.
 *
 * All integers in an FDT are big-endian; helpers below convert. */
#ifndef RIVET_DT_H
#define RIVET_DT_H

#include "core.h"
#include "str.h"

#define RIV_FDT_MAGIC      0xd00dfeedu
#define RIV_FDT_BEGIN_NODE 0x1u
#define RIV_FDT_END_NODE   0x2u
#define RIV_FDT_PROP       0x3u
#define RIV_FDT_NOP        0x4u
#define RIV_FDT_END        0x9u

typedef struct RIV_PACKED {
    riv_u32 magic;
    riv_u32 totalsize;
    riv_u32 off_dt_struct;
    riv_u32 off_dt_strings;
    riv_u32 off_mem_rsvmap;
    riv_u32 version;
    riv_u32 last_compat_version;
    riv_u32 boot_cpuid_phys;
    riv_u32 size_dt_strings;
    riv_u32 size_dt_struct;
} riv_fdt_header;

RIV_ALWAYS riv_u32 riv_fdt_be32(riv_u32 v) {
    return  ((v >> 24) & 0xFF)
          | ((v >>  8) & 0xFF00)
          | ((v <<  8) & 0xFF0000)
          | ((v << 24) & 0xFF000000u);
}
RIV_ALWAYS riv_u64 riv_fdt_be64(riv_u64 v) {
    return ((riv_u64)riv_fdt_be32((riv_u32)v) << 32)
         | (riv_u64)riv_fdt_be32((riv_u32)(v >> 32));
}

RIV_ALWAYS int riv_fdt_valid(const void *blob) {
    const riv_fdt_header *h = (const riv_fdt_header*)blob;
    return riv_fdt_be32(h->magic) == RIV_FDT_MAGIC;
}
RIV_ALWAYS riv_u32 riv_fdt_size(const void *blob) {
    return riv_fdt_be32(((const riv_fdt_header*)blob)->totalsize);
}

/* Walk every node and call `cb` with its name + depth. Returning nonzero
 * from cb aborts the walk. Properties are not enumerated here; use
 * riv_fdt_find_prop to query a known node. */
typedef int (*riv_fdt_visit_fn)(const char *name, int depth, void *ctx);

RIV_ALWAYS int riv_fdt_walk(const void *blob, riv_fdt_visit_fn cb, void *ctx) {
    if (!riv_fdt_valid(blob)) return -1;
    const riv_fdt_header *h = (const riv_fdt_header*)blob;
    const char *base = (const char*)blob;
    const riv_u32 *p = (const riv_u32*)(base + riv_fdt_be32(h->off_dt_struct));
    int depth = 0;
    for (;;) {
        riv_u32 tag = riv_fdt_be32(*p++);
        if (tag == RIV_FDT_END) return 0;
        if (tag == RIV_FDT_NOP) continue;
        if (tag == RIV_FDT_BEGIN_NODE) {
            const char *name = (const char*)p;
            riv_size L = riv_strlen(name);
            int r = cb(name, depth, ctx);
            if (r) return r;
            depth++;
            p += (L + 4) / 4;
        } else if (tag == RIV_FDT_END_NODE) {
            depth--;
        } else if (tag == RIV_FDT_PROP) {
            riv_u32 plen = riv_fdt_be32(*p++);
            p++;                  /* skip nameoff */
            p += (plen + 3) / 4;
        } else {
            return -1;            /* malformed */
        }
    }
}

#endif /* RIVET_DT_H */
