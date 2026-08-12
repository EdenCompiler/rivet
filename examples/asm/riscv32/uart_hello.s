# RIVET RV32I demo: print a message to the QEMU virt UART.
#
# The virt machine maps a 16550-compatible UART at 0x10000000. Byte
# register stride. Standard register set: THR at +0, LSR at +5 with
# bit 5 = transmit-holding-register empty.
#
# Build + run:
#   build/rivet-as -m riscv32 examples/asm/riscv32/uart_hello.s \
#                  -o bin/uart_hello.bin -b 0x80000000
#   qemu-system-riscv32 -machine virt -bios none -kernel bin/uart_hello.bin \
#                       -nographic -no-reboot

.equ HALT_CODE, 93

.text
_start:
    # Load addresses without using `la` (the assembler does not expand it).
    # Kernel is loaded at 0x80000000; the message starts at 0x80000100.
    lui     s0, 0x80000           # s0 = 0x80000000
    addi    s0, s0, 0x100         # s0 = address of msg
    lui     s1, 0x10000           # s1 = UART base 0x10000000

print_loop:
    lb      a0, 0(s0)
    beq     a0, zero, done
wait_tx:
    lb      t0, 5(s1)             # LSR
    andi    t0, t0, 0x20          # THRE
    beq     t0, zero, wait_tx
    sb      a0, 0(s1)             # THR <- byte
    addi    s0, s0, 1
    j       print_loop

done:
    li      a7, HALT_CODE
    ecall
spin:
    j       spin

# Place message at a fixed absolute address so the lui+addi above can
# reach it. .org is interpreted as an absolute target (current base
# subtracted internally).
.org 0x80000100
msg:
    .asciz "Hello from RIVET on qemu-system-riscv32!\n"
