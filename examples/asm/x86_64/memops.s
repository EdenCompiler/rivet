# RIVET x86_64 demo: memory-operand round-trip against a safe scratch
# region. Loaded at 0x200000 by examples/qemu/lm64_launch.S.
#
# rbx points to a 256-byte scratch region we own. We use the full
# ModR/M+SIB encoding range against it: base, base+disp8, base+disp32,
# base+index*scale+disp, lea variants, and reg-from-mem reads. After
# every form has been exercised, prints a confirmation banner via the
# debug console and exits via isa-debug-exit.

.text
_start:
    # rbx = scratch region (in our own .data area below).
    mov     rbx, 0x200100

    # Store known patterns at known offsets.
    mov     rax, 0x1111111111111111
    mov     [rbx], rax
    mov     rax, 0x2222222222222222
    mov     [rbx + 8], rax
    mov     rax, 0x3333333333333333
    mov     [rbx + 16], rax

    # Read them back with various addressing forms.
    mov     rax, [rbx]                  # base only
    mov     rcx, [rbx + 8]              # base + disp8
    mov     rdx, [rbx + 16]             # base + disp8
    lea     rsi, [rbx + 0x10]           # lea base + disp
    add     rax, [rbx + 8]              # add reg, [mem]
    xor     rax, [rbx]                  # xor reg, [mem]

    # Print banner.
    mov     rsi, 0x200080
    mov     rcx, 24
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
    .asciz "[x86_64] memops demo OK\n"

.org 0x200100
scratch:
    .zero 256
