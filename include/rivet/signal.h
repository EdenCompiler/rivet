/* RIVET — signal.h : POSIX-flavored signal delivery for riv_proc.
 *
 * Each process carries a bitmap of pending signals and a table of
 * handlers. Signals fire at well-defined points (syscall return, scheduler
 * tick) by calling riv_signal_dispatch_pending() on the current proc. */
#ifndef RIVET_SIGNAL_H
#define RIVET_SIGNAL_H

#include "core.h"
#include "proc.h"

#define RIV_SIGHUP   1
#define RIV_SIGINT   2
#define RIV_SIGQUIT  3
#define RIV_SIGILL   4
#define RIV_SIGTRAP  5
#define RIV_SIGABRT  6
#define RIV_SIGBUS   7
#define RIV_SIGFPE   8
#define RIV_SIGKILL  9
#define RIV_SIGUSR1  10
#define RIV_SIGSEGV  11
#define RIV_SIGUSR2  12
#define RIV_SIGPIPE  13
#define RIV_SIGALRM  14
#define RIV_SIGTERM  15
#define RIV_SIGCHLD  17
#define RIV_NSIG     32

typedef void (*riv_sig_handler)(int sig);

#define RIV_SIG_DFL  ((riv_sig_handler)0)
#define RIV_SIG_IGN  ((riv_sig_handler)1)

typedef struct {
    riv_sig_handler handlers[RIV_NSIG];
    riv_u32         pending;
    riv_u32         mask;
} riv_sig_state;

/* Each process needs a riv_sig_state pointer. Caller wires it as
 * needed; RIVET keeps the runtime mechanism out of riv_proc itself
 * to avoid forcing the dependency. */
RIV_ALWAYS void riv_sig_init(riv_sig_state *s) {
    for (int i = 0; i < RIV_NSIG; ++i) s->handlers[i] = RIV_SIG_DFL;
    s->pending = 0;
    s->mask    = 0;
}

RIV_ALWAYS void riv_sig_set(riv_sig_state *s, int sig, riv_sig_handler h) {
    if (sig > 0 && sig < RIV_NSIG && sig != RIV_SIGKILL) {
        s->handlers[sig] = h;
    }
}

RIV_ALWAYS void riv_sig_raise(riv_sig_state *s, int sig) {
    if (sig > 0 && sig < RIV_NSIG) s->pending |= (1u << sig);
}

/* Returns 1 if any handler ran. SIGKILL force-exits via riv_proc_exit. */
RIV_ALWAYS int riv_signal_dispatch_pending(riv_sig_state *s) {
    riv_u32 deliverable = s->pending & ~s->mask;
    if (!deliverable) return 0;
    int ran = 0;
    for (int i = 1; i < RIV_NSIG; ++i) {
        if (!(deliverable & (1u << i))) continue;
        s->pending &= ~(1u << i);
        if (i == RIV_SIGKILL) riv_proc_exit(128 + i);
        riv_sig_handler h = s->handlers[i];
        if (h == RIV_SIG_DFL) {
            if (i == RIV_SIGSEGV || i == RIV_SIGABRT || i == RIV_SIGBUS) {
                riv_proc_exit(128 + i);
            }
        } else if (h != RIV_SIG_IGN) {
            h(i);
        }
        ran = 1;
    }
    return ran;
}

#endif /* RIVET_SIGNAL_H */
