/* RIVET demo: linker script for a multiboot1 ELF kernel.
 *
 * Loaded at 1 MiB (Multiboot1 convention). The header must be in the
 * first 8 KiB of the file, so .multiboot is placed first. */

ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot ALIGN(4) :
    {
        *(.multiboot)
    }

    .text ALIGN(4K) :
    {
        *(.text*)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata*)
    }

    .data ALIGN(4K) :
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
