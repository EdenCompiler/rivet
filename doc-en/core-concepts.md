# Core Concepts

## File Types

### `.h` (header)

RIVET runtime modules.

Each header is self-contained and lives in `include/rivet/`. The
umbrella header `include/rivet.h` pulls all of them in, but modules can
also be included individually.

### `.c` (source)

User code: firmware, kernels, drivers.

Examples live in `examples/firmware/` and `examples/kernel/`. They
include `rivet.h` (or a specific module) and consume the runtime as a
header-only library.

### `.s` (assembly)

Source files for the RIVET assembler, `rivet-as`.

Arch is selected via `-m riscv32` or `-m x86_64`. RIVET assembly is a
plain, comma-separated, GAS-like syntax with labels, directives, and
mnemonics. Pseudo-ops such as `nop`, `mv`, `li`, `j`, and `ret` are
expanded at parse time.

### `.ld.txt` (linker script)

Layout script consumed by `rivet-ld`.

Declares memory regions and `place` directives that position assembled
binaries inside the final image. Output is a single flat `.bin`.

## Image Model

A RIVET image is usually composed of:

- one boot binary (assembled from `.s`)
- one or more kernel or data binaries

Regions have:

- a name
- an origin (load address)
- a length
- an optional fill byte

The boot stub typically performs early init, loads later sections from
flash or disk, and transfers control to the kernel entry.

There is an important separation between:

- image-level structure, which decides what regions exist and where they
  live in memory (handled by `rivet-ld`)
- section-level structure, which decides what bytes get emitted inside
  each input file (handled by `rivet-as` for assembly and by the user's
  C toolchain for C code)

At the image level, `region`, `place`, `entry`, and `define` are the
main building blocks of the layout script. At the section level, forms
like `.text`, `.data`, `.word`, `.byte`, `.asciz`, `.align`, and raw
instructions define the actual contents.

## Execution Model

RIVET currently emphasizes two architectures:

- RISC-V 32-bit: RV32I integer base plus the M (multiply / divide)
  extension and the `fence` / `fence.i` memory-ordering ops
- x86_64: long mode with REX prefixes, full ModR/M and SIB addressing,
  immediates, REL32 branches, IDT-driven traps, PIC/APIC interrupts,
  and CR3-driven paging

The shipped runtime detects the current architecture in `arch.h` and
exposes per-arch primitives (irq save/restore, cycle counter, page
table operations) through a single C API.

The assembler therefore works across two encoding models at once:

- fixed-width 32-bit RV32 instructions
- variable-length x86_64 instructions with optional REX, ModR/M, SIB,
  and immediates

Forms such as `.text`, `.org`, and `.align` work identically on both.
Mnemonic and operand syntax are arch-specific and live in the
corresponding `src/arch/*_as.c` backend.

## DSL Philosophy

The runtime DSL is C syntax over low-level machine work. It tries to
provide:

- direct control over layout and instructions
- reusable arch-backed helpers
- explicit forms for things like GDT, IDT, paging, IRQs, MMIO, atomics,
  spinlocks, processes, syscalls, VFS, and devices

Where the DSL does not abstract something, you can still fall back to
inline assembly via `__asm__ __volatile__`.

In practice, RIVET behaves like a layered library:

- some macros are pure compile-time substitutions (`kernel_entry`,
  `irq_safe`, `forever`, `mmio_reg32`)
- some inline functions emit a single instruction (`riv_wfi`, `riv_dmb`,
  `riv_cpu_relax`, `riv_irq_save`)
- some functions implement small pieces of policy (allocators, schedulers,
  page-table walkers)
- some headers are vtables waiting for a driver (gpio, i2c, spi, dma,
  uart, disk, flash)

That last rule is what makes the framework practical for OS work: you
are not trapped when the high-level layer does not have a dedicated
driver yet.

## Build Pipeline

The normal path from source to image is:

1. `make` invokes the host compiler on tool sources in `src/`.
2. The assembler is built once with both arch backends linked in.
3. Each `.s` source is assembled to a flat `.bin` in `bin/`.
4. C sources in `examples/` are compile-checked against the runtime.
5. `rivet-ld` can be invoked to combine binaries into a final image.
6. `rivet-img` can pad and CRC-trail the final image.
7. `rivet-objcopy` can convert the final image to Intel HEX or SREC.

This is why order matters inside layout scripts, and why label-based
references work even when the label is defined later in the file.

## Compile-Time Versus Run-Time

Inside RIVET sources, ordinary C still exists. You can define helper
macros, constants, or include files at preprocessor time.

But macros from `bare.h` and friends should be read as image-building
forms:

- `const` (`#define`) is a compile-time substitution
- `mmio_reg32(name, addr)` emits typed read/write inline functions
- `kthread(name) { body }` declares a kernel thread function
- `init_func` / `exit_func` place a function in a named linker section
- `module_init(fn)` / `module_exit(fn)` place a pointer in a table
- `device_register(tag, ops)` enrolls a device for enumeration

If you need runtime state, store it in emitted data, not in macro
expansions.

## Structured Control

RIVET already provided ordinary C control flow. The DSL also exposes
structured forms for bare-metal idioms:

- `forever { ... }`
- `irq_safe { ... }`
- `critical_section { ... }`
- `spin_locked(&lock) { ... }`
- `defer(cleanup_fn)`

Their semantics are intentionally simple:

- `forever` emits an infinite loop
- `irq_safe` and `critical_section` disable interrupts for the duration
  of the block, restoring prior state on exit
- `spin_locked` acquires a spinlock for the duration of the block
- `defer` registers a cleanup function called when the variable goes
  out of scope

They are useful when you want the readability of structured flow without
giving up exact machine-oriented control.

## Current Hardware Scope

RIVET is not a universal hardware abstraction layer. Current modules
assume:

- RV32I + M for RISC-V targets
- x86_64 long mode with REX-prefixed encodings
- legacy 8259A PIC, 8253 PIT, 16550A UART, PL011 UART, and PS/2
  keyboard on x86 platforms
- standard PCI type-1 config-space access at I/O 0xCF8 / 0xCFC

That means the framework is well suited for emulator-driven OS
development, RV32 microcontroller firmware, and x86_64 hobby kernels.
