/* RIVET — paging.h : physical memory manager + page-table primitives.
 *
 *   riv_pmm        Bitmap-based 4 KiB physical frame allocator.
 *   riv_pgmap      Arch-specific page table (x86_64 4-level, RV32 Sv32).
 *
 * Caller supplies the PMM bitmap and the region of physical RAM it
 * manages. Page tables are themselves allocated from the PMM.
 *
 * IMPORTANT: All addresses below are physical until paging is enabled.
 * After enabling paging, the caller's identity map must cover the
 * region the kernel is running from. */
#ifndef RIVET_PAGING_H
#define RIVET_PAGING_H

#include "core.h"
#include "mem.h"

#define RIV_PAGE_SIZE   4096u
#define RIV_PAGE_SHIFT  12
#define RIV_PAGE_MASK   (RIV_PAGE_SIZE - 1)

/* Generic page-flag bits (mapped to arch-specific PTE bits internally). */
#define RIV_PG_PRESENT  (1u << 0)
#define RIV_PG_WRITE    (1u << 1)
#define RIV_PG_USER     (1u << 2)
#define RIV_PG_NX       (1u << 3)
#define RIV_PG_GLOBAL   (1u << 4)

/* ----------------------- physical memory manager --------------------- */
typedef struct {
    riv_u32 *bitmap;    /* 1 bit per frame; 1 = free */
    riv_uptr base;      /* physical base of region */
    riv_u32  nframes;
    riv_u32  free_count;
} riv_pmm;

RIV_ALWAYS void riv_pmm_init(riv_pmm *m, riv_u32 *bitmap,
                             riv_uptr base, riv_u32 nframes) {
    m->bitmap = bitmap;
    m->base = base;
    m->nframes = nframes;
    m->free_count = nframes;
    for (riv_u32 i = 0; i < (nframes + 31) / 32; ++i) bitmap[i] = 0xFFFFFFFFu;
}

RIV_ALWAYS riv_uptr riv_pmm_alloc(riv_pmm *m) {
    riv_u32 words = (m->nframes + 31) / 32;
    for (riv_u32 w = 0; w < words; ++w) {
        if (m->bitmap[w] == 0) continue;
        riv_u32 b = (riv_u32)__builtin_ctz(m->bitmap[w]);
        riv_u32 idx = w * 32 + b;
        if (idx >= m->nframes) return 0;
        m->bitmap[w] &= ~(1u << b);
        m->free_count--;
        riv_uptr p = m->base + (riv_uptr)idx * RIV_PAGE_SIZE;
        /* zero new frame to avoid leaking previous data */
        riv_u8 *q = (riv_u8*)p;
        for (riv_u32 i = 0; i < RIV_PAGE_SIZE; ++i) q[i] = 0;
        return p;
    }
    return 0;
}

RIV_ALWAYS void riv_pmm_free(riv_pmm *m, riv_uptr frame) {
    if (frame < m->base) return;
    riv_u32 idx = (riv_u32)((frame - m->base) >> RIV_PAGE_SHIFT);
    if (idx >= m->nframes) return;
    m->bitmap[idx >> 5] |= (1u << (idx & 31));
    m->free_count++;
}

/* ----------------------- x86_64 4-level paging ----------------------- */
#if RIVET_ARCH_X86_64

typedef riv_u64 riv_pte;

#define X86_PTE_P    (1ull << 0)
#define X86_PTE_RW   (1ull << 1)
#define X86_PTE_US   (1ull << 2)
#define X86_PTE_G    (1ull << 8)
#define X86_PTE_NX   (1ull << 63)

typedef struct riv_pgmap { riv_pte *pml4; } riv_pgmap;

RIV_ALWAYS riv_u64 _riv_pg_flags_to_x86(riv_u32 f) {
    riv_u64 v = X86_PTE_P;
    if (f & RIV_PG_WRITE)  v |= X86_PTE_RW;
    if (f & RIV_PG_USER)   v |= X86_PTE_US;
    if (f & RIV_PG_GLOBAL) v |= X86_PTE_G;
    if (f & RIV_PG_NX)     v |= X86_PTE_NX;
    return v;
}

RIV_ALWAYS int riv_pgmap_init(riv_pgmap *p, riv_pmm *pmm) {
    riv_uptr f = riv_pmm_alloc(pmm);
    if (!f) return -1;
    p->pml4 = (riv_pte*)f;
    return 0;
}

RIV_ALWAYS int riv_pgmap_map(riv_pgmap *p, riv_uptr vaddr, riv_uptr paddr,
                              riv_u32 flags, riv_pmm *pmm) {
    riv_pte *t = p->pml4;
    int idx[4] = {
        (int)((vaddr >> 39) & 0x1FF),
        (int)((vaddr >> 30) & 0x1FF),
        (int)((vaddr >> 21) & 0x1FF),
        (int)((vaddr >> 12) & 0x1FF),
    };
    for (int lvl = 0; lvl < 3; ++lvl) {
        riv_pte e = t[idx[lvl]];
        if (!(e & X86_PTE_P)) {
            riv_uptr f = riv_pmm_alloc(pmm);
            if (!f) return -1;
            t[idx[lvl]] = (riv_pte)f | X86_PTE_P | X86_PTE_RW
                        | ((flags & RIV_PG_USER) ? X86_PTE_US : 0);
            t = (riv_pte*)f;
        } else {
            t = (riv_pte*)(e & 0x000FFFFFFFFFF000ull);
        }
    }
    t[idx[3]] = (paddr & ~(riv_uptr)RIV_PAGE_MASK) | _riv_pg_flags_to_x86(flags);
    return 0;
}

RIV_ALWAYS riv_uptr riv_pgmap_walk(riv_pgmap *p, riv_uptr vaddr) {
    riv_pte *t = p->pml4;
    int idx[4] = {
        (int)((vaddr >> 39) & 0x1FF), (int)((vaddr >> 30) & 0x1FF),
        (int)((vaddr >> 21) & 0x1FF), (int)((vaddr >> 12) & 0x1FF)
    };
    for (int lvl = 0; lvl < 3; ++lvl) {
        riv_pte e = t[idx[lvl]];
        if (!(e & X86_PTE_P)) return 0;
        t = (riv_pte*)(e & 0x000FFFFFFFFFF000ull);
    }
    riv_pte leaf = t[idx[3]];
    if (!(leaf & X86_PTE_P)) return 0;
    return (riv_uptr)(leaf & 0x000FFFFFFFFFF000ull) | (vaddr & RIV_PAGE_MASK);
}

RIV_ALWAYS int riv_pgmap_unmap(riv_pgmap *p, riv_uptr vaddr) {
    riv_pte *t = p->pml4;
    int idx[4] = {
        (int)((vaddr >> 39) & 0x1FF), (int)((vaddr >> 30) & 0x1FF),
        (int)((vaddr >> 21) & 0x1FF), (int)((vaddr >> 12) & 0x1FF)
    };
    for (int lvl = 0; lvl < 3; ++lvl) {
        riv_pte e = t[idx[lvl]];
        if (!(e & X86_PTE_P)) return -1;
        t = (riv_pte*)(e & 0x000FFFFFFFFFF000ull);
    }
    t[idx[3]] = 0;
    return 0;
}

RIV_ALWAYS void riv_pgmap_activate(riv_pgmap *p) {
    __asm__ __volatile__("mov %0, %%cr3" :: "r"((riv_uptr)p->pml4) : "memory");
}
RIV_ALWAYS void riv_tlb_flush(riv_uptr vaddr) {
    __asm__ __volatile__("invlpg (%0)" :: "r"(vaddr) : "memory");
}
RIV_ALWAYS void riv_tlb_flush_all(void) {
    riv_uptr cr3;
    __asm__ __volatile__("mov %%cr3, %0; mov %0, %%cr3"
                         : "=r"(cr3) :: "memory");
}

#elif RIVET_ARCH_RISCV
/* ----------------------- RISC-V Sv32 paging -------------------------- */

typedef riv_u32 riv_pte;

#define RV_PTE_V    (1u << 0)
#define RV_PTE_R    (1u << 1)
#define RV_PTE_W    (1u << 2)
#define RV_PTE_X    (1u << 3)
#define RV_PTE_U    (1u << 4)
#define RV_PTE_G    (1u << 5)

typedef struct riv_pgmap { riv_pte *root; } riv_pgmap;

RIV_ALWAYS riv_u32 _riv_pg_flags_to_rv(riv_u32 f) {
    riv_u32 v = RV_PTE_V | RV_PTE_R;
    if (f & RIV_PG_WRITE)  v |= RV_PTE_W;
    if (f & RIV_PG_USER)   v |= RV_PTE_U;
    if (f & RIV_PG_GLOBAL) v |= RV_PTE_G;
    return v;
}

RIV_ALWAYS int riv_pgmap_init(riv_pgmap *p, riv_pmm *pmm) {
    riv_uptr f = riv_pmm_alloc(pmm);
    if (!f) return -1;
    p->root = (riv_pte*)f;
    return 0;
}

RIV_ALWAYS int riv_pgmap_map(riv_pgmap *p, riv_uptr vaddr, riv_uptr paddr,
                              riv_u32 flags, riv_pmm *pmm) {
    riv_u32 vpn1 = (vaddr >> 22) & 0x3FF;
    riv_u32 vpn0 = (vaddr >> 12) & 0x3FF;
    riv_pte pde = p->root[vpn1];
    riv_pte *pt;
    if (!(pde & RV_PTE_V)) {
        riv_uptr f = riv_pmm_alloc(pmm);
        if (!f) return -1;
        p->root[vpn1] = ((riv_pte)(f >> 12) << 10) | RV_PTE_V;
        pt = (riv_pte*)f;
    } else {
        pt = (riv_pte*)((pde >> 10) << 12);
    }
    pt[vpn0] = ((riv_pte)(paddr >> 12) << 10) | _riv_pg_flags_to_rv(flags);
    return 0;
}

RIV_ALWAYS riv_uptr riv_pgmap_walk(riv_pgmap *p, riv_uptr vaddr) {
    riv_u32 vpn1 = (vaddr >> 22) & 0x3FF;
    riv_u32 vpn0 = (vaddr >> 12) & 0x3FF;
    riv_pte pde = p->root[vpn1];
    if (!(pde & RV_PTE_V)) return 0;
    riv_pte *pt = (riv_pte*)((pde >> 10) << 12);
    riv_pte leaf = pt[vpn0];
    if (!(leaf & RV_PTE_V)) return 0;
    return ((riv_uptr)(leaf >> 10) << 12) | (vaddr & RIV_PAGE_MASK);
}

RIV_ALWAYS int riv_pgmap_unmap(riv_pgmap *p, riv_uptr vaddr) {
    riv_u32 vpn1 = (vaddr >> 22) & 0x3FF;
    riv_u32 vpn0 = (vaddr >> 12) & 0x3FF;
    riv_pte pde = p->root[vpn1];
    if (!(pde & RV_PTE_V)) return -1;
    riv_pte *pt = (riv_pte*)((pde >> 10) << 12);
    pt[vpn0] = 0;
    return 0;
}

RIV_ALWAYS void riv_pgmap_activate(riv_pgmap *p) {
    riv_u32 satp = (1u << 31) | ((riv_uptr)p->root >> 12);
    __asm__ __volatile__("csrw satp, %0; sfence.vma" :: "r"(satp) : "memory");
}
RIV_ALWAYS void riv_tlb_flush(riv_uptr vaddr) {
    __asm__ __volatile__("sfence.vma %0" :: "r"(vaddr) : "memory");
}
RIV_ALWAYS void riv_tlb_flush_all(void) {
    __asm__ __volatile__("sfence.vma" ::: "memory");
}

#else
/* Architecture without a paging backend: provide opaque type stubs so
 * the OS subsystem headers (proc.h, vfs.h) still compile. Functional
 * calls into the page-table API are not available. */
typedef void     riv_pte;
typedef struct riv_pgmap { void *root; } riv_pgmap;

#endif

#endif /* RIVET_PAGING_H */
