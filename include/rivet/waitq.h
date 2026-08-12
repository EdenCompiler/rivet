/* RIVET — waitq.h : wait queue for blocking primitives.
 *
 * A wait queue holds processes that are sleeping on some condition.
 * Waiters call `riv_waitq_sleep`, which moves the current process out
 * of the run queue and yields. Other code calls `riv_waitq_wake_one`
 * or `riv_waitq_wake_all` when the condition becomes true. */
#ifndef RIVET_WAITQ_H
#define RIVET_WAITQ_H

#include "list.h"
#include "spin.h"
#include "proc.h"

typedef struct {
    riv_node head;
    riv_spin lock;
} riv_waitq;

#define RIV_WAITQ_INIT(name) { { &(name).head, &(name).head }, RIV_SPIN_INIT }

RIV_ALWAYS void riv_waitq_init(riv_waitq *q) {
    riv_list_init(&q->head);
    riv_spin_init(&q->lock);
}

/* Suspend the current process on q. Caller's `cond_lock` is the spin
 * lock protecting the wait condition; it is released across the sleep
 * to avoid lost wake-ups, then reacquired on return. */
RIV_ALWAYS void riv_waitq_sleep(riv_waitq *q, riv_spin *cond_lock) {
    riv_proc *cur = riv_proc_current();
    if (!cur) return;
    spin_locked(&q->lock) {
        cur->state = RIV_PROC_SLEEPING;
        riv_list_del(&cur->link);
        riv_list_add_tail(&q->head, &cur->link);
    }
    if (cond_lock) riv_spin_unlock(cond_lock);
    riv_proc_yield();
    if (cond_lock) riv_spin_lock(cond_lock);
}

/* Move one waiter (FIFO) back to the run queue, if any. */
RIV_ALWAYS int riv_waitq_wake_one(riv_waitq *q) {
    int woke = 0;
    spin_locked(&q->lock) {
        if (!riv_list_empty(&q->head)) {
            riv_node *n = riv_list_pop(&q->head);
            riv_proc *p = riv_list_entry(n, riv_proc, link);
            riv_proc_ready(p);
            woke = 1;
        }
    }
    return woke;
}

/* Wake every waiter. Useful for broadcast (e.g. close on a pipe). */
RIV_ALWAYS int riv_waitq_wake_all(riv_waitq *q) {
    int n = 0;
    while (riv_waitq_wake_one(q)) ++n;
    return n;
}

#endif /* RIVET_WAITQ_H */
