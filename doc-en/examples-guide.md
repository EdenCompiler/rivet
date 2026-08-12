# Examples Guide

The `examples/` tree ships compile-ready demonstrations for every
major part of the framework. They are organized by kind:

```
examples/
  asm/
    riscv32/     RV32 assembly examples
    x86_64/      x86_64 assembly examples
  firmware/      firmware-style C demos
  kernel/        kernel-style C demos
  linker/        layout scripts for rivet-ld
```

Each subsection below summarizes what each file teaches and what to
read next.

## Assembly

### `asm/riscv32/boot.s`

A minimal RV32I bootrom. Computes 1+2+…+10 in `a0`, halts with
`ecall`. Exercises:

- `.equ` constant declaration
- `.text` section + labels
- `li`, `bge`, `add`, `addi`, `j`, `mv`, `ecall` instructions

Good starting point for understanding the assembler's two-pass label
resolution.

### `asm/riscv32/features.s`

Tests the RV32M multiply / divide extension plus `fence` and the
string directives. Exercises:

- `mul`, `div`, `rem`
- `fence rw, rw`, `fence.i`
- `.align`, `.asciz "RIVET\n"`, `.zero 8, 0xAA`

### `asm/x86_64/hello.s`

Linux x86_64 syscall ABI demo: writes "hello from x86_64" to fd 1 via
SYS_write, then exits via SYS_exit. Exercises:

- `mov reg, imm32`
- `syscall` instruction
- `.align`, `.asciz`, `.equ`

### `asm/x86_64/branch.s`

Loop / call / push-pop / inc-dec demo. Exercises:

- `cmp reg, imm32` + `jne rel32`
- `call rel32` to forward-declared label
- `push reg` / `pop reg`
- `inc` / `dec`

### `asm/x86_64/memops.s`

Comprehensive memory-operand exercise. Tests every ModR/M + SIB
addressing form the assembler supports:

- `[rbx]`, `[rbx+8]`, `[rbx+0x1000]`
- `[rsp]` (SIB-forced base)
- `[rbp]` (disp8=0 forced)
- `[r13+16]` (REX.B high-reg base)
- `[rbx+rcx*4]`, `[rbx+rcx*8+32]`
- `lea rax, [rbx+64]`
- `mov [mem], reg` and `xor reg, [mem]`

## Firmware

### `firmware/blink.c`

STM32-style GPIO blink firmware. Demonstrates:

- `mmio_bank` register-block declaration
- `riv_mmio_setbits32` / `riv_mmio_rmw32`
- `critical_section { }` scoped block
- The cooperative `riv_scheduler` with two periodic tasks
- `kernel_entry` and `isr` placement keywords

### `firmware/sensor.c`

Multi-driver firmware exercising the new bare-metal DSL. Demonstrates:

- `mmio_reg32` typed register declaration
- `init_func` / `exit_func` sections
- `module_init` / `module_exit` lifecycle table
- `device_register` device-enumeration table
- `kthread` thread declaration
- `defer` automatic cleanup
- `irq_safe` block + `riv_wfe()` + `panic_on`
- I2C master interaction via the generic `riv_i2c_ops` vtable

## Kernel

### `kernel/kernel.c`

Tiny x86_64 kernel that writes to the VGA text framebuffer at 0xB8000
and overrides `riv_panic_handler`. Demonstrates:

- `kernel_entry` + `RIV_NORETURN`
- Linker-symbol BSS clearing via `riv_zero_range`
- Custom panic handler
- `riv_assert` runtime check

### `kernel/panic_test.c`

Exercises the default panic handler when `RIVET_PANIC_IMPL` is
defined. Demonstrates:

- `RIV_UART_DECLARE`, `RIV_LOG_DECLARE`, `RIV_TIME_DECLARE`
- x86 debugcon UART backend (`riv_uart_debugcon_init`)
- `riv_log_set_sink` + leveled logging
- Hardware cycle counter sanity check (`riv_cycle_count`)
- Triggering the default panic handler

### `kernel/kernel_full.c`

Compile-only smoke test of every major OS subsystem cohering at once.
Demonstrates:

- PMM (`riv_pmm_init`)
- Ramfs (`riv_ramfs_init`) mounted on `/`
- Syscall table registration (read, write, open, getpid, exit, yield)
- Trap handlers for page fault and timer
- Process creation (`riv_proc_alloc`, `riv_proc_ready`)
- Per-process fd table
- Stub context-switch hook

### `kernel/x86_init.c`

x86_64 early init exercising the legacy + modern infrastructure.
Demonstrates:

- GDT + TSS setup with `riv_gdt_set` / `riv_gdt_set_tss`
- 8259 PIC remap + mask-all
- 8253 PIT at 100 Hz
- PCI bus enumeration with callback
- Mutex / semaphore / pipe / hash / rand init
- ELF program loader with placement callback

## Linker

### `linker/sample.ld.txt`

Two-region layout: 4 KiB ROM at `0x80000000` plus 16 KiB RAM at
`0x80100000`. Places `boot.bin` into ROM, declares an entry symbol,
and produces a single image binary.

## Recommended Reading Order

1. `firmware/blink.c` — DSL basics + scheduler
2. `asm/riscv32/boot.s` — assembler basics
3. `asm/x86_64/hello.s` — x86_64 register/immediate forms
4. `asm/x86_64/memops.s` — full ModR/M + SIB encoding
5. `kernel/kernel.c` — first real kernel entry
6. `kernel/panic_test.c` — runtime services (uart, log, time)
7. `firmware/sensor.c` — full bare-metal DSL
8. `kernel/kernel_full.c` — OS subsystems wired together
9. `kernel/x86_init.c` — x86_64 boot infrastructure
10. `linker/sample.ld.txt` + `make` — full build pipeline
