/* RIVET — proc.h : process control block + preemptive scheduler.
 *
 * Lightweight process model:
 *   - Fixed-size process table (no malloc dependency).
 *   - Each riv_proc owns a kernel stack, a page map, and a saved CPU
 *     context that can be restored by the arch context-switch primitive.
 *   - Round-robin scheduler walks the runnable list; each timer ISR
 *     calls riv_sched_tick_irq() to (optionally) trigger a switch.
 *
 * The context-switch helper riv_ctx_switch() must be provided by the
 * arch backend (an asm stub — declared here, implemented by user). */
#ifndef RIVET_PROC_H
#define RIVET_PROC_H

#include "core.h"
#include "list.h"
#include "spin.h"
#include "paging.h"

#define RIV_PROC_MAX        64
#define RIV_PROC_NAME_LEN   16
#define RIV_PROC_KSTACK     8192

typedef enum {
    RIV_PROC_UNUSED = 0,
    RIV_PROC_READY,
    RIV_PROC_RUNNING,
    RIV_PROC_SLEEPING,
    RIV_PROC_ZOMBIE,
} riv_proc_state;

/* Saved CPU context. Layout is opaque to portable code; the asm stub
 * defines the field order. We reserve enough room for any supported
 * arch. */
typedef struct {
    riv_uptr regs[16];
    riv_uptr pc;
    riv_uptr sp;
} riv_context;

struct riv_file_table;   /* fwd decl, defined in vfs.h */

typedef struct riv_proc {
    int             pid;
    char            name[RIV_PROC_NAME_LEN];
    riv_proc_state  state;
    riv_context     ctx;
    riv_uptr        kstack_base;
    riv_pgmap      *pgmap;
    int             exit_code;
    struct riv_proc *parent;
    struct riv_file_table *files;
    riv_node        link;        /* run queue / sleep queue node */
} riv_proc;

typedef struct {
    riv_proc  table[RIV_PROC_MAX];
    riv_node  runq;
    riv_proc *current;
    int       next_pid;
    riv_spin  lock;
} riv_scheduler_state;

extern riv_scheduler_state riv_sched;

#define RIV_PROC_DECLARE() \
    riv_scheduler_state riv_sched = { {{0}}, { &riv_sched.runq, &riv_sched.runq }, \
                                       (riv_proc*)0, 1, RIV_SPIN_INIT }

/* Allocate next free PCB slot. Returns NULL if table full. */
RIV_ALWAYS riv_proc *riv_proc_alloc(void) {
    riv_proc *r = (riv_proc*)0;
    spin_locked(&riv_sched.lock) {
        for (int i = 0; i < RIV_PROC_MAX; ++i) {
            if (riv_sched.table[i].state == RIV_PROC_UNUSED) {
                r = &riv_sched.table[i];
                r->state = RIV_PROC_READY;
                r->pid   = riv_sched.next_pid++;
                r->parent = riv_sched.current;
                r->exit_code = 0;
                r->files = (struct riv_file_table*)0;
                break;
            }
        }
    }
    return r;
}

RIV_ALWAYS void riv_proc_free(riv_proc *p) {
    spin_locked(&riv_sched.lock) {
        riv_list_del(&p->link);
        p->state = RIV_PROC_UNUSED;
    }
}

/* Arch-provided context switch — implemented as an asm stub. */
void riv_ctx_switch(riv_context *save, riv_context *load);

/* Add to runnable queue. */
RIV_ALWAYS void riv_proc_ready(riv_proc *p) {
    spin_locked(&riv_sched.lock) {
        p->state = RIV_PROC_READY;
        riv_list_add_tail(&riv_sched.runq, &p->link);
    }
}

/* Voluntary yield from current task. */
RIV_ALWAYS void riv_proc_yield(void) {
    riv_proc *cur, *next;
    spin_locked(&riv_sched.lock) {
        cur = riv_sched.current;
        if (riv_list_empty(&riv_sched.runq)) { next = cur; break; }
        if (cur && cur->state == RIV_PROC_RUNNING) {
            cur->state = RIV_PROC_READY;
            riv_list_add_tail(&riv_sched.runq, &cur->link);
        }
        riv_node *n = riv_list_pop(&riv_sched.runq);
        next = riv_list_entry(n, riv_proc, link);
        next->state = RIV_PROC_RUNNING;
        riv_sched.current = next;
    }
    if (cur != next) {
        riv_ctx_switch(cur ? &cur->ctx : (riv_context*)0, &next->ctx);
    }
}

/* Exit the current process. */
RIV_NORETURN RIV_ALWAYS void riv_proc_exit(int code) {
    riv_proc *cur = riv_sched.current;
    if (cur) {
        cur->exit_code = code;
        cur->state = RIV_PROC_ZOMBIE;
    }
    /* hand off to scheduler; never returns */
    forever { riv_proc_yield(); }
}

RIV_ALWAYS riv_proc *riv_proc_current(void) { return riv_sched.current; }

/* Call from timer ISR. Forces a yield if quantum expired (here: every tick). */
RIV_ALWAYS void riv_sched_tick_irq(void) { riv_proc_yield(); }

#endif /* RIVET_PROC_H */
