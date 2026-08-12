/* RIVET demo: multiboot1 boot stub.
 *
 * Built with GCC's assembler (32-bit i386). The multiboot header tells
 * QEMU's -kernel option where to start; the entry point sets up a
 * stack and calls kmain() in C.
 *
 * Compile with `gcc -m32 -c multiboot.S` and link as ELF32. */

.set MB_MAGIC,    0x1BADB002
.set MB_FLAGS,    0x00000003          /* PAGE_ALIGN | MEMORY_INFO */
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

.section .multiboot, "a"
.align 4
.long MB_MAGIC
.long MB_FLAGS
.long MB_CHECKSUM

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.globl _start
.type  _start, @function
_start:
    cli
    movl $stack_top, %esp

    /* Pass multiboot magic + info pointer to kmain(unsigned, unsigned). */
    pushl %ebx
    pushl %eax
    call  kmain

    /* If kmain returns, fall through to a halt loop. */
1:  hlt
    jmp 1b
.size _start, . - _start
