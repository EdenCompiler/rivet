# RIVET Documentation

RIVET is a C99 framework for constructing bare-metal operating systems
and firmware. The documentation describes how to build images, target
specific architectures, and extend the runtime with new drivers and
subsystems.

## Key Capabilities

The framework currently specializes in RISC-V 32-bit and x86_64
architectures, with modules and demonstrations covering boot stubs,
paging, interrupts, processes, syscalls, VFS, ramfs, FAT, ELF, PCI,
ACPI, SMP, framebuffer, serial UART, PS/2 keyboard, Ethernet/IPv4/UDP
header structures, an ARP cache, block-cache, TTY line discipline,
SHA-256, base64, and a wide selection of CPU and peripheral
primitives (GPIO, I²C, SPI, DMA, WDT, flash, ADC, PWM, RTC, CAN).

## Documentation Structure

The guide provides six main sections starting with Getting Started,
followed by Core Concepts and the DSL reference. Tools, modules, and an
example tour round out the learning path, designed to be followed
sequentially for optimal comprehension.

- [Getting Started](getting-started.md)
- [Core Concepts](core-concepts.md)
- [DSL Reference](dsl-reference.md)
- [Modules Reference](modules-reference.md)
- [Tools Reference](tools-reference.md)
- [Examples Guide](examples-guide.md)

## Current Limitations

As noted in the root README, "RIVET is not yet a general-purpose
multi-architecture production OS platform." The assembler emits flat
binaries only, the supported architectures are RV32 and x86_64, and the
runtime is header-only. Adding new architectures requires writing two C
backend files in `src/arch/` and updating the umbrella header.
