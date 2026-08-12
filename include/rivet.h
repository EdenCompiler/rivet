/* RIVET — bare-metal kernel & firmware DSL for C99
 * Umbrella header. Include this or pick individual modules.
 *
 *   rivet/arch.h    arch detection macros
 *   rivet/core.h    types, attributes, DSL keywords
 *   rivet/mem.h     memcpy/memset, barriers, cpu_relax
 *   rivet/mmio.h    memory-mapped I/O accessors, field macros
 *   rivet/io.h      x86 port I/O (not declared on non-x86)
 *   rivet/irq.h     IRQ enable/disable, critical_section, halt
 *   rivet/atomic.h  C11 atomic primitives
 *   rivet/spin.h    spinlock (with IRQ-save variant + scoped block)
 *   rivet/list.h    intrusive circular doubly-linked list
 *   rivet/ring.h    SPSC byte ring buffer
 *   rivet/bitmap.h  fixed-width bit array
 *   rivet/heap.h    bump allocator + first-fit free-list
 *   rivet/str.h     freestanding strings + minimal vsnprintf
 *   rivet/log.h     leveled logging on a user sink
 *   rivet/time.h    tick counter + delays + deadlines
 *   rivet/crc.h     CRC-8 / CRC-16 / CRC-32
 *   rivet/uart.h    UART interface + 16550A driver + PL011 + debugcon
 *   rivet/boot.h    runtime init (BSS/.data/ctors)
 *   rivet/panic.h   panic / assert
 *   rivet/task.h    cooperative tick scheduler
 *   rivet/link.h    linker section helpers, BSS zero / data copy
 *   rivet/paging.h  physical memory manager + page tables (x86_64 / RV32)
 *   rivet/trap.h    interrupt/exception dispatch + x86_64 IDT
 *   rivet/proc.h    process control block + preemptive scheduler
 *   rivet/syscall.h syscall dispatch table + entry helpers
 *   rivet/vfs.h     virtual file system + per-process fd table
 *   rivet/ramfs.h   in-memory file system implementation
 *   rivet/waitq.h   wait queue (blocking primitive)
 *   rivet/mutex.h   sleeping mutex
 *   rivet/sem.h     counting semaphore
 *   rivet/pipe.h    blocking byte pipe IPC
 *   rivet/elf.h     ELF64 parsing + program-segment loader
 *   rivet/rand.h    xorshift / splitmix PRNGs
 *   rivet/hash.h    open-addressed string→uptr hash table
 *   rivet/fb.h      linear 32 bpp framebuffer + built-in 8x8 font
 *   rivet/gdt.h     x86_64 GDT + TSS descriptors
 *   rivet/pic.h     legacy 8259 PIC remap / mask / EOI
 *   rivet/pit.h     Intel 8253/8254 programmable interval timer
 *   rivet/apic.h    Local APIC + I/O APIC
 *   rivet/pci.h     legacy PCI configuration space access
 *   rivet/kbd.h     PS/2 keyboard (set 1) polling driver
 *   rivet/bare.h    bare-metal-flavored DSL macros (init/exit sections,
 *                   typed MMIO regs, kthread, device registry, defer)
 *   rivet/cpu.h     CPU identification (cpuid / midr / misa)
 *   rivet/signal.h  POSIX-style signal delivery
 *   rivet/slab.h    fixed-size-object slab allocator
 *   rivet/dt.h      flat device tree (FDT) reader
 *   rivet/disk.h    block-device abstraction
 *   rivet/fat.h     read-only FAT12/16/32 driver
 *   rivet/gpio.h    generic GPIO controller abstraction
 *   rivet/i2c.h     I2C master abstraction
 *   rivet/spi.h     SPI master abstraction
 *   rivet/dma.h     DMA channel abstraction
 *   rivet/wdt.h     watchdog timer abstraction
 *   rivet/flash.h   NOR/SPI flash erase + program + read
 *   rivet/net.h     Ethernet / IPv4 / UDP / ICMP header structures
 *   rivet/arp.h     ARP cache + request frame builder
 *   rivet/buf.h     block-cache layer over riv_disk_ops
 *   rivet/tty.h     line-discipline TTY (raw + cooked modes)
 *   rivet/acpi.h    minimal ACPI RSDP / RSDT / XSDT walker
 *   rivet/smp.h     per-CPU storage + CPU id query
 *   rivet/base64.h  base64 encode / decode
 *   rivet/sha256.h  SHA-256 streaming hash
 *   rivet/adc.h     analog-to-digital converter abstraction
 *   rivet/pwm.h     pulse-width-modulation channel abstraction
 *   rivet/rtc.h     real-time clock abstraction + epoch conversion
 *   rivet/can.h     CAN bus abstraction (classic + FD frames)
 */
#ifndef RIVET_H
#define RIVET_H

#include "rivet/arch.h"
#include "rivet/core.h"
#include "rivet/mem.h"
#include "rivet/mmio.h"
#include "rivet/io.h"
#include "rivet/irq.h"
#include "rivet/atomic.h"
#include "rivet/spin.h"
#include "rivet/list.h"
#include "rivet/ring.h"
#include "rivet/bitmap.h"
#include "rivet/heap.h"
#include "rivet/str.h"
#include "rivet/log.h"
#include "rivet/time.h"
#include "rivet/crc.h"
#include "rivet/uart.h"
#include "rivet/boot.h"
#include "rivet/panic.h"
#include "rivet/task.h"
#include "rivet/link.h"
#include "rivet/paging.h"
#include "rivet/trap.h"
#include "rivet/vfs.h"
#include "rivet/ramfs.h"
#include "rivet/proc.h"
#include "rivet/syscall.h"
#include "rivet/waitq.h"
#include "rivet/mutex.h"
#include "rivet/sem.h"
#include "rivet/pipe.h"
#include "rivet/elf.h"
#include "rivet/rand.h"
#include "rivet/hash.h"
#include "rivet/fb.h"
#include "rivet/gdt.h"
#include "rivet/pic.h"
#include "rivet/pit.h"
#include "rivet/apic.h"
#include "rivet/pci.h"
#include "rivet/kbd.h"
#include "rivet/bare.h"
#include "rivet/cpu.h"
#include "rivet/signal.h"
#include "rivet/slab.h"
#include "rivet/dt.h"
#include "rivet/disk.h"
#include "rivet/fat.h"
#include "rivet/gpio.h"
#include "rivet/i2c.h"
#include "rivet/spi.h"
#include "rivet/dma.h"
#include "rivet/wdt.h"
#include "rivet/flash.h"
#include "rivet/net.h"
#include "rivet/arp.h"
#include "rivet/buf.h"
#include "rivet/tty.h"
#include "rivet/acpi.h"
#include "rivet/smp.h"
#include "rivet/base64.h"
#include "rivet/sha256.h"
#include "rivet/adc.h"
#include "rivet/pwm.h"
#include "rivet/rtc.h"
#include "rivet/can.h"

#define RIVET_VERSION_MAJOR 0
#define RIVET_VERSION_MINOR 6
#define RIVET_VERSION_PATCH 0

#endif /* RIVET_H */
