# RIVET example RV32I bootrom.
# Computes 1+2+...+10 into a0, then halts via ecall.

.equ HALT_CODE, 93

.text
_start:
    li   t0, 0           # accumulator
    li   t1, 1           # i
    li   t2, 11          # limit (exclusive)
loop:
    bge  t1, t2, done
    add  t0, t0, t1
    addi t1, t1, 1
    j    loop
done:
    mv   a0, t0
    li   a7, HALT_CODE
    ecall
spin:
    j    spin
