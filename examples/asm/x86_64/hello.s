# RIVET x86_64 demo: hello-world via QEMU debugcon.
# Loaded at 0x200000 by examples/qemu/lm64_launch.S. Linux's SYSCALL
# ABI is not available bare-metal, so we use `out al, 0xE9` to write
# bytes to QEMU's debug console, then `out al, 0xF4` to exit.

.text
_start:
    mov     rsi, 0x200080
    mov     rcx, 23
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
msg:
    .asciz "[x86_64] hello demo OK\n"
