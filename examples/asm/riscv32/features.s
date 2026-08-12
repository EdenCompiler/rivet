# Exercise new RIVET assembler features: M-ext, fence, string directives.

.text
_start:
    li     a0, 7
    li     a1, 6
    mul    a2, a0, a1          # 42
    div    a3, a2, a1          # 7
    rem    a4, a2, a0          # 0
    fence  rw, rw
    fence.i
    li     a7, 93
    ecall

.align  2
greeting:
    .asciz "RIVET\n"

filler:
    .zero  8, 0xAA
