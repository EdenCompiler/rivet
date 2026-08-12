/* RIVET demo: 32-bit multiboot1 ELF that switches to long mode and
 * jumps to a 64-bit payload pre-placed at LOAD_ADDR_64.
 *
 * The 64-bit payload is provided by a sibling assembly file (typically
 * compiled with rivet-as for x86_64 and embedded into the .payload
 * section by the per-demo linker script).
 *
 * Identity-maps the first 512 MiB with 2 MiB pages, enables PAE and
 * long mode, then `ljmp` into 64-bit code that does
 *     mov   $LOAD_ADDR_64, %rax
 *     jmp   *%rax
 * to hand control to the payload. */

.set MB_MAGIC,    0x1BADB002
.set MB_FLAGS,    0x00000003
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

.set LOAD_ADDR_64, 0x200000

.section .multiboot, "a"
.align 4
.long MB_MAGIC
.long MB_FLAGS
.long MB_CHECKSUM

.section .bss, "aw", @nobits
.align 4096
pml4:    .skip 4096
pdpt:    .skip 4096
pd:      .skip 4096
.align 16
stack_bottom: .skip 16384
stack_top:

.section .rodata
.align 16
.global gdt64
gdt64:
    .quad 0
    .quad 0x00AF9A000000FFFF
    .quad 0x00CF92000000FFFF
gdt64_end:
gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .long gdt64

.section .text
.code32
.global _start
.type _start, @function
_start:
    cli
    movl  $stack_top, %esp

    cld
    xorl  %eax, %eax
    movl  $pml4, %edi
    movl  $3072, %ecx
    rep   stosl

    movl  $pdpt, %eax
    orl   $0x3, %eax
    movl  %eax, pml4

    movl  $pd, %eax
    orl   $0x3, %eax
    movl  %eax, pdpt

    movl  $pd, %edi
    movl  $0x83, %eax
    movl  $256, %ecx
1:  movl  %eax, (%edi)
    movl  $0,   4(%edi)
    addl  $0x200000, %eax
    addl  $8, %edi
    decl  %ecx
    jnz   1b

    movl  %cr4, %eax
    orl   $0x20, %eax
    movl  %eax, %cr4

    movl  $pml4, %eax
    movl  %eax, %cr3

    movl  $0xC0000080, %ecx
    rdmsr
    orl   $0x100, %eax
    wrmsr

    movl  %cr0, %eax
    orl   $0x80000001, %eax
    movl  %eax, %cr0

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

    movq  $LOAD_ADDR_64, %rax
    jmp   *%rax
