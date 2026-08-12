/* RIVET — acpi.h : minimal ACPI table walker.
 *
 * Provides RSDP/RSDT/XSDT discovery and a generic table-by-signature
 * lookup. Just enough to find MADT, FADT, HPET, MCFG, etc; parsing
 * those further is the caller's job. */
#ifndef RIVET_ACPI_H
#define RIVET_ACPI_H

#include "core.h"
#include "mem.h"
#include "str.h"

typedef struct RIV_PACKED {
    char    signature[8];   /* "RSD PTR " */
    riv_u8  checksum;
    char    oem_id[6];
    riv_u8  revision;
    riv_u32 rsdt_address;
    /* ACPI 2.0+ fields follow */
    riv_u32 length;
    riv_u64 xsdt_address;
    riv_u8  ext_checksum;
    riv_u8  reserved[3];
} riv_acpi_rsdp;

typedef struct RIV_PACKED {
    char    signature[4];
    riv_u32 length;
    riv_u8  revision;
    riv_u8  checksum;
    char    oem_id[6];
    char    oem_table_id[8];
    riv_u32 oem_revision;
    riv_u32 creator_id;
    riv_u32 creator_revision;
} riv_acpi_sdt;

#define RIV_ACPI_RSDP_SIG "RSD PTR "

/* Search a 16-byte-aligned region of physical memory for the RSDP. */
RIV_ALWAYS const riv_acpi_rsdp *riv_acpi_find_rsdp(const void *region,
                                                    riv_size n) {
    const riv_u8 *p = (const riv_u8*)region;
    for (riv_size i = 0; i + 20 <= n; i += 16) {
        if (riv_memcmp(p + i, RIV_ACPI_RSDP_SIG, 8) == 0) {
            return (const riv_acpi_rsdp*)(p + i);
        }
    }
    return (const riv_acpi_rsdp*)0;
}

/* Walk an RSDT (32-bit pointer array). For XSDT use riv_acpi_xsdt_find. */
RIV_ALWAYS const riv_acpi_sdt *riv_acpi_rsdt_find(const riv_acpi_sdt *rsdt,
                                                    const char sig[4]) {
    riv_u32 nentries = (rsdt->length - sizeof(*rsdt)) / 4;
    const riv_u32 *ptrs = (const riv_u32*)(rsdt + 1);
    for (riv_u32 i = 0; i < nentries; ++i) {
        const riv_acpi_sdt *t = (const riv_acpi_sdt*)(riv_uptr)ptrs[i];
        if (riv_memcmp(t->signature, sig, 4) == 0) return t;
    }
    return (const riv_acpi_sdt*)0;
}

RIV_ALWAYS const riv_acpi_sdt *riv_acpi_xsdt_find(const riv_acpi_sdt *xsdt,
                                                    const char sig[4]) {
    riv_u32 nentries = (xsdt->length - sizeof(*xsdt)) / 8;
    const riv_u64 *ptrs = (const riv_u64*)(xsdt + 1);
    for (riv_u32 i = 0; i < nentries; ++i) {
        const riv_acpi_sdt *t = (const riv_acpi_sdt*)(riv_uptr)ptrs[i];
        if (riv_memcmp(t->signature, sig, 4) == 0) return t;
    }
    return (const riv_acpi_sdt*)0;
}

/* Byte-checksum verification: sum of all bytes must be 0 mod 256. */
RIV_ALWAYS int riv_acpi_checksum_ok(const void *data, riv_size n) {
    const riv_u8 *p = (const riv_u8*)data;
    riv_u8 sum = 0;
    for (riv_size i = 0; i < n; ++i) sum += p[i];
    return sum == 0;
}

#endif /* RIVET_ACPI_H */
