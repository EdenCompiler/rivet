# DSL Reference

This is a practical reference to the RIVET C-macro DSL. It focuses on
meaning and usage, not a formal grammar.

## Reading the DSL Correctly

Inside a RIVET source file, most macros are not ordinary C expressions.
They are constructs that:

- emit one or more inline functions
- mark functions for placement in named linker sections
- declare typed MMIO registers
- register devices or modules for enumeration
- influence section layout

If a macro is not recognized as a RIVET DSL form, it falls through to
the C preprocessor as a normal identifier.

## Top-Level Source Forms

### `kernel_entry`

Annotation that marks a function as the kernel/firmware entry point.

```c
kernel_entry void kmain(void) {
    ...
}
```

Expands to `RIV_USED RIV_SECTION(".text.entry")`, so the linker keeps
the function and places it in the entry section.

### `init_func` / `exit_func` / `early_func`

Section annotations for boot-time and shutdown-time functions.

```c
init_func void setup_clocks(void) {
    ...
}
```

Each places the function in `.init.text`, `.exit.text`, or
`.text.early` so a boot stub can walk and run them in order.

### `module_init` / `module_exit`

Linux-style module lifecycle: place a function pointer in `.init.fns`
or `.exit.fns`.

```c
init_func void my_init(void) { ... }
module_init(my_init);
```

### `device_register`

Place a `riv_device_ops *` in the `.dev.tbl` section so a generic
enumerator can probe every device at boot.

```c
static riv_device_ops my_dev = { "my_dev", probe, remove, NULL };
device_register(my_dev_entry, &my_dev);
```

The `tag` argument is an arbitrary identifier used to name the
placement symbol; it just has to be unique in the translation unit.

## Core Structural Forms

### `forever`

Infinite loop.

```c
forever {
    riv_cpu_halt();
}
```

### `naked`

Function attribute for boot stubs and ISRs that must not have a C
prologue or epilogue.

```c
naked void _start(void) { ... }
```

### `packed_struct`

Equivalent to `struct __attribute__((packed))`.

```c
packed_struct mbr_part {
    riv_u8 status;
    riv_u8 chs_start[3];
    ...
};
```

### `unreachable()`

Compiler hint that the call cannot be reached.

```c
forever { riv_cpu_halt(); }
unreachable();
```

## Scoped Blocks

### `irq_safe { ... }` / `critical_section { ... }`

Disable interrupts for the duration of the block, restoring prior state
on exit.

```c
irq_safe {
    shared_counter++;
}
```

### `spin_locked(&lock) { ... }`

Acquire a spinlock (with IRQs saved) for the duration of the block.

```c
spin_locked(&my_lock) {
    push_to_list(&my_list, node);
}
```

### `defer(fn)`

GCC `__attribute__((cleanup))` wrapper. The named cleanup function is
called when the annotated variable leaves scope.

```c
static void release_lock(riv_spin **p) { riv_spin_unlock(*p); }

riv_spin *l defer(release_lock) = &my_lock;
riv_spin_lock(l);
/* automatic release at end of block */
```

## Data and Layout Forms

### `mmio_reg32(name, addr)` / `mmio_reg16` / `mmio_reg8`

Declares a typed MMIO register and emits inline `name_read()` /
`name_write(v)` helpers.

```c
mmio_reg32(CLK_CTRL, 0x40020000)

CLK_CTRL_write(CLK_CTRL_read() | 0x1);
```

### `mmio_bank(typename, fields...)`

Declares a packed `volatile struct` describing a register bank, with a
compile-time size-alignment assertion.

```c
mmio_bank(stm32_gpio_t,
    riv_u32 MODER;
    riv_u32 OTYPER;
    riv_u32 OSPEEDR;
);

#define GPIOA (*(stm32_gpio_t*)0x40020000)
```

### `place_at(addr)` / `aligned_as(n)`

Linker placement helpers.

```c
place_at(0x10000) static const riv_u32 fixed_table[] = { ... };
aligned_as(64) static riv_u8 buffer[256];
```

### `persistent_data` / `no_init_data`

Section annotations for non-zeroed BSS or persistent storage.

## Threads and Tasks

### `kthread(name) { body }`

Declare a kernel thread function. Receives an unused context argument
in the signature for compatibility with the task scheduler.

```c
kthread(sensor_task) {
    riv_u8 v;
    if (riv_i2c_read_reg(&bus, 0x68, 0x75, &v) == 0) {
        riv_log_info("sensor", "WHO_AM_I=0x%x", v);
    }
}
```

### `register_kthread(fn, period)`

Macro that produces an initialized `riv_task` table entry for the
cooperative scheduler.

```c
static riv_task tasks[] = {
    register_kthread(sensor_task, 500),
    register_kthread(blink_task,  250),
};
```

## Interrupt-Service-Routine Forms

### `isr`

Annotate a function as an ISR. Expands to `RIV_USED` only — the
`interrupt` attribute is intentionally opt-in via `isr_attr` because
required signatures differ per arch.

```c
isr void SysTick_Handler(void) {
    riv_tick();
}
```

### `isr_vector(name)`

Declare a vector table.

```c
isr_vector(vectors) = {
    reset_handler,
    nmi_handler,
    hardfault_handler,
    ...
};
```

## Instruction Intrinsics

Inline-asm wrappers exposed under short, arch-neutral names:

- `riv_pause()` — `pause` / `yield` / `nop` per arch
- `riv_wfe()` — wait-for-event (or halt on non-ARM)
- `riv_sev()` — send-event (ARM only; no-op elsewhere)
- `riv_dmb()` — data memory barrier
- `riv_dsb()` — data sync barrier
- `riv_isb()` — instruction sync barrier
- `riv_irq_enable()` / `riv_irq_disable()` / `riv_cpu_halt()`
- `riv_irq_save()` / `riv_irq_restore(state)`
- `riv_compiler_barrier()` — no instruction, just a memory clobber

## Diagnostics

### `panic_on(cond, msg)`

```c
panic_on(!ptr, "null pointer in init");
```

### `riv_assert(cond)`

```c
riv_assert(sizeof(riv_uptr) == sizeof(void*));
```

### `static_assert_size(type, n)`

```c
static_assert_size(riv_idt_entry, 16);
```

### `riv_panic(msg)`

Calls the registered panic handler. Default handler (when
`RIVET_PANIC_IMPL` is defined) formats the message and writes it to the
registered UART plus x86 debugcon port 0xE9, then halts forever.

## DSL Decoration Attributes

- `RIV_MUST_CHECK` — warn if the return value is ignored
- `RIV_PURE` / `RIV_CONST` — pureness annotations
- `RIV_HOT` / `RIV_COLD` — placement hints
- `RIV_PRINTF_LIKE(fmt, va)` — printf format checking

## Assembler Interop

For arch-specific bare-metal work that the runtime does not yet abstract,
use GCC extended inline assembly:

```c
__asm__ __volatile__("invlpg (%0)" :: "r"(vaddr) : "memory");
```

### Best Practice

- use DSL macros when they clearly describe the action you want
- use inline assembly when you need exact instruction selection or
  special-case behavior
- keep mode switches, stack setup, and machine assumptions explicit

## A Small Real Example

```c
#include "rivet.h"

RIV_UART_DECLARE();
RIV_TIME_DECLARE(1000);

static riv_uart_16550 uart;

mmio_reg32(LED_CTRL, 0x40010000)

kthread(blink) {
    LED_CTRL_write(LED_CTRL_read() ^ 1);
}

init_func void board_init(void) {
    riv_uart_16550_init(&uart, 0x10000000, 0);
    riv_uart_set_default(&uart.ops);
}
module_init(board_init);

kernel_entry void firmware_main(void) {
    riv_uart_puts("RIVET firmware booted\n");
    forever {
        irq_safe { blink(NULL); }
        riv_wfe();
    }
}
```

This shows the three main layers working together:

- module-level entry and lifecycle (`kernel_entry`, `module_init`)
- per-thread bodies (`kthread`)
- direct hardware access (`mmio_reg32` + intrinsics)
