/* RIVET demo: linker script for the lm64_launch wrapper.
 *
 * Places the multiboot header + 32-bit boot stub at 1 MiB, then drops
 * the embedded 64-bit payload at 2 MiB. Payload contents are linked
 * in via a `.payload` input section provided by an accompanying
 * `.incbin` wrapper assembly file. */

ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot ALIGN(4) : { *(.multiboot) }
    .text     ALIGN(16) : { *(.text*) }
    .rodata   ALIGN(16) : { *(.rodata*) }
    .data     ALIGN(16) : { *(.data*) }

    . = 2M;
    .payload ALIGN(16) :
    {
        *(.payload)
    }

    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss*)
    }
}
