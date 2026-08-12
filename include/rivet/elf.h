/* RIVET — elf.h : ELF64 header types + minimal program loader.
 *
 * Implements just enough to validate, walk program headers, and copy
 * PT_LOAD segments into the target address space. Suitable for loading
 * a flat-ELF user program into a freshly-allocated page table. */
#ifndef RIVET_ELF_H
#define RIVET_ELF_H

#include "core.h"
#include "mem.h"

#define RIV_ELF_MAG0        0x7F
#define RIV_ELF_MAG1        'E'
#define RIV_ELF_MAG2        'L'
#define RIV_ELF_MAG3        'F'

#define RIV_ELFCLASS32      1
#define RIV_ELFCLASS64      2
#define RIV_ELFDATA2LSB     1

#define RIV_ET_EXEC         2
#define RIV_ET_DYN          3
#define RIV_PT_LOAD         1
#define RIV_PT_DYNAMIC      2
#define RIV_PT_INTERP       3
#define RIV_PT_PHDR         6

#define RIV_PF_X            1
#define RIV_PF_W            2
#define RIV_PF_R            4

typedef struct RIV_PACKED {
    riv_u8  ident[16];
    riv_u16 type;
    riv_u16 machine;
    riv_u32 version;
    riv_u64 entry;
    riv_u64 phoff;
    riv_u64 shoff;
    riv_u32 flags;
    riv_u16 ehsize;
    riv_u16 phentsize;
    riv_u16 phnum;
    riv_u16 shentsize;
    riv_u16 shnum;
    riv_u16 shstrndx;
} riv_elf64_ehdr;

typedef struct RIV_PACKED {
    riv_u32 type;
    riv_u32 flags;
    riv_u64 offset;
    riv_u64 vaddr;
    riv_u64 paddr;
    riv_u64 filesz;
    riv_u64 memsz;
    riv_u64 align;
} riv_elf64_phdr;

RIV_ALWAYS int riv_elf_is_valid(const void *img, riv_size n) {
    if (n < sizeof(riv_elf64_ehdr)) return 0;
    const riv_u8 *p = (const riv_u8*)img;
    return p[0] == RIV_ELF_MAG0
        && p[1] == RIV_ELF_MAG1
        && p[2] == RIV_ELF_MAG2
        && p[3] == RIV_ELF_MAG3;
}

RIV_ALWAYS const riv_elf64_phdr *riv_elf_phdr(const void *img, int i) {
    const riv_elf64_ehdr *e = (const riv_elf64_ehdr*)img;
    if (i >= e->phnum) return (const riv_elf64_phdr*)0;
    return (const riv_elf64_phdr*)((const char*)img + e->phoff
                                    + (riv_size)i * e->phentsize);
}

RIV_ALWAYS riv_u64 riv_elf_entry(const void *img) {
    return ((const riv_elf64_ehdr*)img)->entry;
}

/* Walks PT_LOAD segments and calls `place(dst_vaddr, src_data,
 * filesz, memsz, flags, ctx)` for each. `place` is responsible for
 * mapping pages and copying bytes (memsz > filesz ⇒ zero-fill).
 * Returns 0 on success, -1 on malformed input. */
typedef int (*riv_elf_place_fn)(riv_u64 vaddr, const void *data,
                                 riv_u64 filesz, riv_u64 memsz,
                                 riv_u32 flags, void *ctx);

RIV_ALWAYS int riv_elf_load(const void *img, riv_size n,
                             riv_elf_place_fn place, void *ctx) {
    if (!riv_elf_is_valid(img, n)) return -1;
    const riv_elf64_ehdr *e = (const riv_elf64_ehdr*)img;
    if (e->ident[4] != RIV_ELFCLASS64) return -1;
    if (e->type != RIV_ET_EXEC && e->type != RIV_ET_DYN) return -1;

    for (int i = 0; i < e->phnum; ++i) {
        const riv_elf64_phdr *p = riv_elf_phdr(img, i);
        if (p->type != RIV_PT_LOAD) continue;
        if (p->offset + p->filesz > n) return -1;
        const void *data = (const char*)img + p->offset;
        int r = place(p->vaddr, data, p->filesz, p->memsz, p->flags, ctx);
        if (r) return r;
    }
    return 0;
}

#endif /* RIVET_ELF_H */
