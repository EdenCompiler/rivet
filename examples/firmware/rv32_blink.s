# RIVET firmware demo for QEMU's RV32 virt machine.
#
# Mimics the structure of a blinky firmware: a "tick" routine fires
# periodically, printing a single character to the UART each iteration.
# The body cycles through "RIVET\n" forever, like a heartbeat blink.
#
# The QEMU virt machine wires the standard 16550 UART at 0x10000000.
#
# Build + run:
#   build/rivet-as -m riscv32 examples/firmware/rv32_blink.s \
#                  -o bin/rv32_blink.bin -b 0x80000000
#   qemu-system-riscv32 -machine virt -bios none -kernel bin/rv32_blink.bin \
#                       -nographic -no-reboot

.equ BANNER_ADDR, 0x80000200
.equ ITER_COUNT,  3

.text
_start:
    # s1 = UART base 0x10000000
    lui     s1, 0x10000

    # s2 = iteration counter
    li      s2, ITER_COUNT

outer:
    # s0 = pointer to message at fixed address 0x80000200
    lui     s0, 0x80000
    addi    s0, s0, 0x200

print_loop:
    lb      a0, 0(s0)
    beq     a0, zero, next_iter
wait_tx:
    lb      t0, 5(s1)               # read LSR
    andi    t0, t0, 0x20            # THRE bit
    beq     t0, zero, wait_tx
    sb      a0, 0(s1)               # THR <- byte
    addi    s0, s0, 1
    j       print_loop

next_iter:
    addi    s2, s2, -1
    bne     s2, zero, outer

done:
    li      a7, 93                  # exit syscall
    ecall
spin:
    j       spin

.org BANNER_ADDR
banner:
    .asciz "[RV32] heartbeat tick\n"
