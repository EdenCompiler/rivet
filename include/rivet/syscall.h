/* RIVET — syscall.h : syscall number table + dispatch.
 *
 * On x86_64 the SYSCALL instruction enters at a single entry stub which
 * saves registers, then calls riv_syscall_dispatch(nr, a0..a5).
 *
 * Arguments follow the Linux x86_64 ABI: nr in rax, args in rdi rsi rdx
 * r10 r8 r9, return in rax. Stub must shuffle rcx<->r10 if using the
 * Linux convention (rcx holds saved-rip after SYSCALL).
 *
 * Syscall numbers are intentionally compatible with a Linux-style subset
 * so user code can be portable in spirit. */
#ifndef RIVET_SYSCALL_H
#define RIVET_SYSCALL_H

#include "core.h"

/* Canonical syscall numbers — extend as needed. */
enum {
    RIV_SYS_READ      = 0,
    RIV_SYS_WRITE     = 1,
    RIV_SYS_OPEN      = 2,
    RIV_SYS_CLOSE     = 3,
    RIV_SYS_STAT      = 4,
    RIV_SYS_LSEEK     = 8,
    RIV_SYS_MMAP      = 9,
    RIV_SYS_BRK       = 12,
    RIV_SYS_GETPID    = 39,
    RIV_SYS_FORK      = 57,
    RIV_SYS_EXECVE    = 59,
    RIV_SYS_EXIT      = 60,
    RIV_SYS_YIELD     = 24,
    RIV_SYS_MAX       = 256,
};

typedef riv_i64 (*riv_syscall_fn)(riv_i64 a, riv_i64 b, riv_i64 c,
                                  riv_i64 d, riv_i64 e, riv_i64 f);

extern riv_syscall_fn riv_syscall_table[RIV_SYS_MAX];

#define RIV_SYSCALL_DECLARE() \
    riv_syscall_fn riv_syscall_table[RIV_SYS_MAX] = { (riv_syscall_fn)0 }

RIV_ALWAYS void riv_syscall_register(unsigned nr, riv_syscall_fn fn) {
    if (nr < RIV_SYS_MAX) riv_syscall_table[nr] = fn;
}

/* Dispatch — called by the arch entry stub. Returns -1 on unknown nr. */
RIV_ALWAYS riv_i64 riv_syscall_dispatch(riv_i64 nr,
                                        riv_i64 a, riv_i64 b, riv_i64 c,
                                        riv_i64 d, riv_i64 e, riv_i64 f) {
    if (nr < 0 || nr >= RIV_SYS_MAX) return -1;
    riv_syscall_fn h = riv_syscall_table[nr];
    if (!h) return -1;
    return h(a, b, c, d, e, f);
}

/* User-space-style inline wrapper (Linux x86_64 ABI). */
#if RIVET_ARCH_X86_64
RIV_ALWAYS riv_i64 riv_syscall(riv_i64 nr,
                               riv_i64 a, riv_i64 b, riv_i64 c,
                               riv_i64 d, riv_i64 e, riv_i64 f) {
    riv_i64 ret;
    register riv_i64 r10 __asm__("r10") = d;
    register riv_i64 r8  __asm__("r8")  = e;
    register riv_i64 r9  __asm__("r9")  = f;
    __asm__ __volatile__("syscall"
        : "=a"(ret)
        : "a"(nr), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory");
    return ret;
}
#endif

#endif /* RIVET_SYSCALL_H */
