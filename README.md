# RIVET

RIVET is a C99 framework for bare-metal operating system and firmware
development. It includes:

- a header-only runtime library (66 modules)
- a custom two-pass assembler with pluggable architecture backends
- an architecture-pluggable disassembler
- a small locator/linker plus an image post-processor and Intel-HEX/SREC converter
- inspection utilities (string extraction, size histogram)
- a bare-metal-flavored C macro DSL on top of the runtime

This root `README.md` is the documentation hub.

## Documentation

- English: [doc-en/README.md](doc-en/README.md)
- Português (Brasil): [doc-ptbr/README.md](doc-ptbr/README.md)

## Quick Links

- Umbrella header: [include/rivet.h](include/rivet.h)
- Runtime modules: [include/rivet/](include/rivet/)
- Tool sources: [src/](src/)
- Architecture backends: [src/arch/](src/arch/)
- Build entry point: [Makefile](Makefile)
- Example firmware: [examples/firmware/](examples/firmware/)
- Example kernel code: [examples/kernel/](examples/kernel/)
- Example assembly: [examples/asm/](examples/asm/)
- Sample linker script: [examples/linker/sample.ld.txt](examples/linker/sample.ld.txt)

## What RIVET Targets Today

RIVET is currently focused on two architectures:

- RISC-V 32-bit (RV32I + M extension)
- x86_64

It supports flat-binary assembly and disassembly for both, plus a
runtime suitable for firmware, RTOS, hobby microkernels, and small
teaching-grade UNIX-style kernels.

It is appropriate for embedded firmware projects, OS research, bootloader
development, networking experiments, and bare-metal experiments on
hardware or emulators such as QEMU.

## Build

```bash
make            # build tools + assemble examples + compile-check C examples
make tools      # only the host toolchain
make examples   # assemble every .s into bin/
make objects    # -ffreestanding compile-check every example .c
make clean      # remove build/ and bin/
```

Toolchain executables land in `build/`. Assembled `.bin` outputs land in
`bin/`. Object-file compile checks land in `build/obj/`.

## Current Limitations

RIVET is not yet a general-purpose multi-architecture production OS
platform. The framework remains focused on its two supported
architectures and on flat-binary output; it does not yet emit ELF
relocatable objects from the assembler, nor does it ship a full UEFI
or device-tree-driven boot path.
