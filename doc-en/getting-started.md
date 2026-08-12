# Getting Started

## Requirements

- GCC (or Clang) with C99 support
- GNU make
- A cross-toolchain for the target arch when running on real hardware
- QEMU for emulator-based testing (optional)

RIVET is built with the host compiler. Cross-compiling for the target
happens later when consuming the runtime headers from your kernel or
firmware code.

## Build All Shipped Artefacts

From the project root:

```bash
make
```

This builds the toolchain into `build/`, assembles every shipped `.s`
example into `bin/`, and compile-checks every shipped `.c` example.

## Build Only the Tools

```bash
make tools
```

Produces `build/rivet-as`, `build/rivet-disas`, `build/rivet-ld`,
`build/rivet-objcopy`, and `build/rivet-img`.

## Build a Single Source File

You can assemble one file directly:

```bash
build/rivet-as -m riscv32 examples/asm/riscv32/boot.s \
                -o bin/boot.bin -b 0x80000000
```

For x86_64:

```bash
build/rivet-as -m x86_64 examples/asm/x86_64/hello.s \
                -o bin/hello.bin -b 0x400000
```

If no architecture is given, the assembler defaults to `riscv32`.

## Disassemble a Binary

```bash
build/rivet-disas -m x86_64 bin/hello.bin -b 0x400000
```

## Build an Image from Multiple Binaries

```bash
build/rivet-ld examples/linker/sample.ld.txt
```

The layout script declares memory regions and `place` directives that
position assembled binaries inside the final image.

## Use the Runtime in Your Own Code

Add `-Iinclude` to your compile command, then:

```c
#include "rivet.h"

kernel_entry void kmain(void) {
    riv_log_info("kernel", "RIVET v%d.%d up",
                 RIVET_VERSION_MAJOR, RIVET_VERSION_MINOR);
    forever { riv_cpu_halt(); }
}
```

Compile freestanding:

```bash
gcc -std=c99 -ffreestanding -nostdlib -Iinclude -c my_kernel.c
```

## Minimal RIVET Firmware

```c
#include "rivet.h"

RIV_UART_DECLARE();
RIV_TIME_DECLARE(1000);

static riv_uart_16550 uart;

kernel_entry void firmware_main(void) {
    riv_uart_16550_init(&uart, 0x10000000, 0);
    riv_uart_set_default(&uart.ops);

    riv_uart_puts("Hello from RIVET firmware\n");
    forever {
        riv_busy_wait_us(500000, 24000000);
        riv_uart_puts(".");
    }
}
```

## Practical Advice

- Start from `examples/firmware/blink.c` or `examples/kernel/kernel.c`.
- Use `examples/kernel/kernel_full.c` and `examples/kernel/x86_init.c`
  as references for a larger multi-subsystem kernel layout.
- Prefer building and testing through QEMU first.
- Treat the current target as RV32 or x86_64 — not "all architectures".
