# Modules Reference

This is a tour of every header in `include/rivet/`. Each section gives
the module's purpose, dependencies, and a one-line usage hint. For full
signatures consult the header itself.

## Foundation

### `arch.h`

Architecture detection macros. Defines `RIVET_ARCH_X86_64`,
`RIVET_ARCH_RISCV`, `RIVET_ARCH_AARCH64`, `RIVET_ARCH_ARM`,
`RIVET_WORD_BITS`, and the friendly `RIVET_ARCH_NAME` string.

### `core.h`

Fixed-width types (`riv_u8` .. `riv_u64`, `riv_uptr`, `riv_size`),
compiler attribute macros (`RIV_USED`, `RIV_ALWAYS`, `RIV_PACKED`,
`RIV_ALIGN`, `RIV_SECTION`, `RIV_NORETURN`, `RIV_WEAK`,
`RIV_INTERRUPT`, `RIV_LIKELY` / `RIV_UNLIKELY`), and DSL keyword
macros (`kernel_entry`, `isr`, `naked`, `forever`).

### `mem.h`

Freestanding `riv_memset` / `riv_memcpy` / `riv_memcmp`, memory
barriers (`riv_mb`, `riv_rmb`, `riv_wmb`), and `riv_cpu_relax()`.

### `mmio.h`

Memory-mapped I/O accessors at 8/16/32/64 bits, RMW helpers, register
bank declarators (`mmio_bank`, `mmio_block`), and bit-field helpers
(`riv_field`, `riv_field_set`).

### `io.h`

x86 port I/O (`riv_inb` / `riv_outb` / etc). Not declared on non-x86 so
misuse produces a compile-time error.

### `irq.h`

IRQ enable/disable, halt, `riv_irq_save` / `riv_irq_restore` per arch,
`critical_section { ... }` scoped block, `isr_vector` macro.

### `link.h`

Linker section helpers, `riv_zero_range` and `riv_copy_range` for BSS
clear and .data copy.

## Concurrency

### `atomic.h`

C11 atomics on top of GCC/Clang `__atomic_*` builtins: load, store,
add, sub, or, and, xchg, CAS.

### `spin.h`

Test-and-set spinlock built on `riv_atomic_cas`. Includes IRQ-save
variant and `spin_locked(&lock) { ... }` scoped block.

### `waitq.h`

Wait queue. Suspend the current process on a condition; wake one or all
waiters.

### `mutex.h`

Sleeping mutex on top of waitq. Owner-tracked.

### `sem.h`

Counting semaphore with `wait` / `trywait` / `post`.

## Data Structures

### `list.h`

Intrusive circular doubly-linked list (Linux-style). Includes
iteration macros, `riv_list_entry(ptr, type, member)`.

### `ring.h`

SPSC byte ring buffer. Lockless, power-of-two capacity.

### `bitmap.h`

Fixed-width bit array: set / clear / test / flip / ffs / ffz /
popcount.

### `hash.h`

Open-addressed string→uptr hash table. FNV-1a 64; no tombstones.

### `slab.h`

Fixed-size-object slab allocator over a caller-supplied arena.

### `heap.h`

Bump allocator plus first-fit free-list with adjacent coalescing.

## Strings, I/O, and Time

### `str.h`

Freestanding string ops (`riv_strlen`, `riv_strcmp`, `riv_strlcpy`),
integer→string formatting, and a minimal `riv_vsnprintf` /
`riv_snprintf` supporting `%d %u %x %X %p %s %c`, width and zero-pad.

### `log.h`

Leveled logging on a user-supplied character sink. Levels: ERROR,
WARN, INFO, DEBUG.

### `time.h`

Tick counter (driven by user from a SysTick ISR), monotonic ms/us,
hardware cycle counter per arch (`riv_cycle_count`), `riv_busy_wait_*`,
and `riv_deadline_*`.

### `crc.h`

CRC-32 (IEEE 802.3 / zlib), CRC-16 (CCITT-FALSE), CRC-8 (1-Wire).
Bit-serial, no tables.

### `rand.h`

xorshift64* PRNG seeded by splitmix64. Includes Lemire-style range
helper.

### `base64.h`

RFC-4648 base64 encode and decode. Standard alphabet, `=` padding,
buffer-safe.

### `sha256.h`

SHA-256 streaming hash (FIPS 180-4). `init` / `update` / `final`.
Suitable for image integrity; not constant-time.

## Drivers

### `uart.h`

UART vtable plus two backend drivers:

- 16550A (configurable register stride for 8-bit or 32-bit access)
- ARM PL011 (QEMU virt aarch64, RPi mini-UART)

Plus x86 debugcon backend writing to port 0xE9.

### `gpio.h`

Generic GPIO vtable: configure (direction, pull), set alternate
function, read, write, toggle, IRQ-set.

### `i2c.h`

I2C master vtable: setup at a given clock, multi-message xfer with
repeated-start, plus convenience `read_reg` / `write_reg` /
`read_block`.

### `spi.h`

SPI master vtable: setup (mode 0..3, bit order), xfer, manual CS
control.

### `dma.h`

DMA channel vtable: start, stop, busy, remaining. Configurable
direction, burst, circular, source/destination increment.

### `wdt.h`

Watchdog vtable: enable with timeout, kick, disable, last-reset-was-wdt
query.

### `flash.h`

NOR/SPI flash vtable: info, read, program, erase, busy poll.

### `adc.h`

Analog-to-digital converter vtable plus raw→millivolt conversion using
the device's reference voltage.

### `pwm.h`

Pulse-width-modulation channel vtable: setup at a given frequency, set
duty in Q16, enable/disable, plus percent-duty helper.

### `rtc.h`

Real-time clock vtable: get/set broken-down time, plus
broken-down-time → Unix epoch seconds conversion.

### `can.h`

CAN bus vtable: classic + CAN-FD frames, standard + extended IDs,
RTR/BRS flags. Payload up to 64 bytes.

### `fb.h`

Linear 32 bpp ARGB framebuffer: plot, clear, fill, glyph, puts. Ships
with an 8×8 ASCII font subset.

### `kbd.h`

PS/2 keyboard polling driver (scancode set 1). Tracks shift / ctrl /
alt / capslock; produces ASCII characters.

### `tty.h`

Line-discipline TTY wrapping a `riv_uart_ops`. Two modes:

- raw — bytes pass through unmodified, no echo, no editing
- cooked — line buffering, backspace, echo, EOL translation

## CPU and Bus

### `cpu.h`

CPU identification per arch: x86 CPUID (vendor, feature bits), ARM
MIDR/MPIDR, RISC-V misa/mvendorid/marchid.

### `gdt.h` (x86_64)

GDT entry, TSS descriptor, 64-bit TSS layout, plus `lgdt` and `ltr`
helpers.

### `pic.h` (x86)

Legacy 8259A PIC remap / mask / EOI. Includes `pic_mask_all` for APIC
takeover.

### `pit.h` (x86)

8253/8254 channel 0 mode 2 rate-generator setup.

### `apic.h` (x86_64)

Local APIC enable, EOI, periodic timer. I/O APIC redirect-table
routing.

### `pci.h`

Type-1 PCI configuration space access via I/O ports 0xCF8 / 0xCFC,
plus full bus enumeration with multi-function device support.

### `acpi.h`

Minimal ACPI table walker: RSDP signature search, RSDT/XSDT lookup by
4-byte signature, byte-checksum verification. Locates MADT/FADT/HPET/
MCFG and friends; parsing those is left to the caller.

### `smp.h`

Per-CPU storage (`RIV_PERCPU(type, name)` plus `riv_percpu(name)`),
declared core count (`riv_smp_set_count` / `riv_smp_count`), and an
arch-specific `riv_cpu_id()` reading APIC ID / MPIDR / mhartid.

## OS Subsystems

### `paging.h`

Bitmap physical memory manager (`riv_pmm_alloc` / `riv_pmm_free`) plus
arch-specific page tables: x86_64 4-level (PML4 → PDPT → PD → PT) and
RISC-V Sv32. Map / unmap / walk / activate / TLB-flush.

### `trap.h`

Trap frame, dispatch table, `riv_trap_set(vec, handler)`,
`riv_trap_dispatch`. x86_64 IDT entry struct and `lidt` loader; RISC-V
mtvec setter.

### `proc.h`

Process control block (`riv_proc`): pid, state, saved context, kernel
stack, page map, file table. Run queue, round-robin scheduler,
`riv_proc_alloc` / `_ready` / `_yield` / `_exit` / `_current` /
`riv_sched_tick_irq`.

### `syscall.h`

Syscall number constants (Linux x86_64 layout), dispatch table,
`riv_syscall_register`, `riv_syscall_dispatch`, and an inline
`riv_syscall(nr, ...)` user-side wrapper on x86_64.

### `signal.h`

POSIX-style signals: handler table, raise, mask, dispatch on syscall
return.

### `pipe.h`

Blocking byte-pipe IPC backed by a ring buffer plus two wait queues.

### `vfs.h`

Virtual file system: inode struct, file ops vtable, mount table,
per-process fd table, `open` / `close` / `read` / `write` / `lseek`.

### `ramfs.h`

In-RAM filesystem with a fixed inode pool of 32 entries × 4 KiB each.
Plugs into VFS.

### `elf.h`

ELF64 header types + program-loader callback. Walks PT_LOAD segments
and calls user-supplied `place(vaddr, data, filesz, memsz, flags, ctx)`.

### `disk.h`

Block device vtable: read LBA, write LBA, flush.

### `fat.h`

Read-only FAT12 / FAT16 / FAT32 driver. Auto-detects type from BPB.

### `dt.h`

Flat device tree (FDT) reader. Magic validation, BE32/64 helpers,
node walk callback.

### `buf.h`

Block-cache layer over `riv_disk_ops`. Caches 32 × 512-byte blocks
with clock-sweep eviction and dirty write-back via `riv_buf_sync`.

### `net.h`

Ethernet, IPv4, UDP, and ICMP packed header structs. Byte-order
helpers (`riv_htons` / `riv_htonl`), one's-complement IPv4 checksum,
dotted-quad builder, generic `riv_netif` vtable.

### `arp.h`

ARP packet header struct, 16-entry ARP cache with LRU eviction, and a
helper that builds a complete 42-byte broadcast request frame.

## Runtime

### `boot.h`

Linker-symbol helpers: `riv_boot_clear_bss`, `riv_boot_copy_data`,
`riv_boot_run_ctors`, plus the umbrella `riv_boot_runtime_init`.

### `panic.h`

Panic / assert / halt. User-overridable handler; default impl (under
`RIVET_PANIC_IMPL`) routes a formatted message to the default UART
plus x86 debug port 0xE9, then halts.

### `task.h`

Cooperative tick-driven task scheduler: `riv_task` entries with
period, `riv_sched_run` from a super-loop, `riv_sched_tick` from a
SysTick ISR.

### `bare.h`

Bare-metal-flavored DSL extensions: attributes (`RIV_MUST_CHECK`,
`RIV_HOT`, `RIV_COLD`, etc.), section keywords (`init_func`,
`exit_func`, `early_func`, `place_at`), scoped blocks (`irq_safe`,
`defer`), instruction intrinsics (`riv_wfe`, `riv_dmb`, etc.),
compile-time helpers (`panic_on`, `static_assert_size`),
typed-register macros (`mmio_reg32` / 16 / 8), and the kernel-thread
/ module / device registration macros.
