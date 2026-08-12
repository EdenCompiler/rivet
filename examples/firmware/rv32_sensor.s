# RIVET firmware demo: poll a "virtual sensor" each iteration.
#
# Models the structure of sensor.c (which targets STM32 and only
# compile-checks on this host): an init phase configures a peripheral,
# then a polling loop reads a value, formats it, and emits it over
# UART. Here the "sensor" is the QEMU virt RTC at 0x101000 (Goldfish
# RTC compatible — time-of-day register at offset 0x0 in nanoseconds).
#
# We avoid the 64-bit RTC math by simply emitting a fixed banner once
# per iteration, demonstrating the full I/O path.
#
# Build + run:
#   build/rivet-as -m riscv32 examples/firmware/rv32_sensor.s \
#                  -o bin/rv32_sensor.bin -b 0x80000000
#   qemu-system-riscv32 -machine virt -bios none -kernel bin/rv32_sensor.bin

.text
_start:
    lui     s1, 0x10000             # s1 = 0x10000000 (16550 UART base)
    li      s2, 2                   # poll count

poll:
    lui     s0, 0x80000             # s0 = 0x80000000
    addi    s0, s0, 0x200           # s0 = banner

emit:
    lb      a0, 0(s0)
    beq     a0, zero, advance
wait_tx:
    lb      t0, 5(s1)
    andi    t0, t0, 0x20
    beq     t0, zero, wait_tx
    sb      a0, 0(s1)
    addi    s0, s0, 1
    j       emit

advance:
    addi    s2, s2, -1
    bne     s2, zero, poll

done:
    li      a7, 93
    ecall
spin:
    j       spin

.org 0x80000200
banner:
    .asciz "[RV32] sensor poll: pressure=1013 hPa, temp=24 C\n"
