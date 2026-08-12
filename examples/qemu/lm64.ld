/* RIVET demo: linker script for a long-mode multiboot1 kernel.
 *
 * Loaded at 1 MiB. Multiboot header in the first 8 KiB of the image.
 * Mixed 32-bit (.text in lm64_boot.o) and 64-bit code share the same
 * physical layout; relocations are resolved at static link time. */

ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot ALIGN(4) :
    {
        *(.multiboot)
    }

    .text ALIGN(16) :
    {
        *(.text*)
    }

    .rodata ALIGN(16) :
    {
        *(.rodata*)
    }

    .data ALIGN(16) :
    {
        *(.data*)
    }

    .bss ALIGN(4K) :
    {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }
}
