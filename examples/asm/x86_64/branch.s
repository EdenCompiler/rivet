# RIVET x86_64 demo: counting loop + branches, printing the result via
# QEMU's debugcon (port 0xE9), then exiting via isa-debug-exit (port
# 0xF4). Loaded at 0x200000 by examples/qemu/lm64_launch.S after the
# 32->64 mode transition.

.text
_start:
    # Compute 1 + 2 + ... + 10 into rax, then print the banner.
    mov     rax, 0
    mov     rbx, 1
    mov     rcx, 11
sum_loop:
    cmp     rbx, rcx
    jge     do_print
    add     rax, rbx
    inc     rbx
    jmp     sum_loop

do_print:
    mov     rsi, 0x200080
    mov     rcx, 24                 # length of banner (incl. newline + NUL pad)
print_loop:
    cmp     rcx, 0
    je      shutdown
    mov     rax, [rsi]
    out     0xE9, al
    inc     rsi
    dec     rcx
    jmp     print_loop

shutdown:
    xor     rax, rax
    out     0xF4, al
    hlt

.org 0x200080
banner:
    .asciz "[x86_64] branch demo OK\n"
