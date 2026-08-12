/* RIVET demo: 32-bit multiboot1 boot stub that switches to long mode.
 *
 * Used by every bootable x86_64 kernel example. After this stub runs,
 * the CPU is in 64-bit long mode with the first 512 MiB identity-mapped
 * using 2 MiB pages, IRQs disabled, and a usable stack. It then calls
 * `kmain64(magic, mb_info_phys_addr)`.
 *
 * Page table layout:
 *   PML4[0] -> PDPT
 *   PDPT[0] -> PD
 *   PD[0..255] -> 2 MiB identity-mapped pages */

.set MB_MAGIC,    0x1BADB002
.set MB_FLAGS,    0x00000003
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

.section .multiboot, "a"
.align 4
.long MB_MAGIC
.long MB_FLAGS
.long MB_CHECKSUM

/* Reserve page-aligned tables in BSS. */
.section .bss, "aw", @nobits
.align 4096
pml4:    .skip 4096
pdpt:    .skip 4096
pd:      .skip 4096
.align 16
stack_bottom: .skip 16384
stack_top:
saved_magic: .skip 8
saved_info:  .skip 8

/* 64-bit GDT: null / code64 / data. */
.section .rodata
.align 16
.global gdt64
gdt64:
    .quad 0                              /* null descriptor */
    .quad 0x00AF9A000000FFFF             /* code64: P|DPL0|S|X|L */
    .quad 0x00CF92000000FFFF             /* data: P|DPL0|S|W */
gdt64_end:

gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .long gdt64

.section .text
.code32
.global _start
.type   _start, @function
_start:
    cli
    movl  $stack_top, %esp

    /* Save multiboot magic + info pointer in BSS. */
    movl  %eax, saved_magic
    movl  %ebx, saved_info

    /* Zero PML4 / PDPT / PD (3 * 4096 / 4 = 3072 dwords). */
    cld
    xorl  %eax, %eax
    movl  $pml4, %edi
    movl  $3072, %ecx
    rep   stosl

    /* PML4[0] -> PDPT, present + writable. */
    movl  $pdpt, %eax
    orl   $0x3, %eax
    movl  %eax, pml4

    /* PDPT[0] -> PD. */
    movl  $pd, %eax
    orl   $0x3, %eax
    movl  %eax, pdpt

    /* Fill 256 PD entries with 2 MiB identity mappings, P|W|PS. */
    movl  $pd, %edi
    movl  $0x83, %eax
    movl  $256, %ecx
1:
    movl  %eax, (%edi)
    movl  $0,   4(%edi)
    addl  $0x200000, %eax
    addl  $8, %edi
    decl  %ecx
    jnz   1b

    /* Enable PAE in CR4. */
    movl  %cr4, %eax
    orl   $0x20, %eax
    movl  %eax, %cr4

    /* Load PML4 base into CR3. */
    movl  $pml4, %eax
    movl  %eax, %cr3

    /* Set LME in IA32_EFER (MSR 0xC0000080). */
    movl  $0xC0000080, %ecx
    rdmsr
    orl   $0x100, %eax
    wrmsr

    /* Enable paging + protected mode in CR0. */
    movl  %cr0, %eax
    orl   $0x80000001, %eax
    movl  %eax, %cr0

    /* Activate 64-bit GDT and jump into long mode. */
    lgdt  gdt64_ptr
    ljmp  $0x08, $long_mode_entry

.code64
long_mode_entry:
    movw  $0x10, %ax
    movw  %ax, %ds
    movw  %ax, %es
    movw  %ax, %fs
    movw  %ax, %gs
    movw  %ax, %ss

    /* SysV AMD64 ABI: rdi = magic, rsi = info ptr. movl zero-extends. */
    movl  saved_magic, %edi
    movl  saved_info,  %esi
    call  kmain64

    /* If kmain64 returns, halt forever. */
1:  cli
    hlt
    jmp   1b
