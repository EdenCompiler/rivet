/* RIVET — sem.h : counting semaphore built on the wait queue.
 *
 * `wait` blocks while count is zero; `post` increments and wakes one
 * waiter. Binary semaphores fall out by initializing count to 0 or 1. */
#ifndef RIVET_SEM_H
#define RIVET_SEM_H

#include "spin.h"
#include "waitq.h"

typedef struct {
    riv_spin  guard;
    riv_waitq waiters;
    int       count;
} riv_sem;

#define RIV_SEM_INIT(name, n) \
    { RIV_SPIN_INIT, RIV_WAITQ_INIT((name).waiters), (n) }

RIV_ALWAYS void riv_sem_init(riv_sem *s, int initial) {
    riv_spin_init(&s->guard);
    riv_waitq_init(&s->waiters);
    s->count = initial;
}

RIV_ALWAYS void riv_sem_wait(riv_sem *s) {
    for (;;) {
        riv_spin_lock(&s->guard);
        if (s->count > 0) { s->count--; riv_spin_unlock(&s->guard); return; }
        riv_waitq_sleep(&s->waiters, &s->guard);
    }
}

RIV_ALWAYS int riv_sem_trywait(riv_sem *s) {
    int got = 0;
    spin_locked(&s->guard) {
        if (s->count > 0) { s->count--; got = 1; }
    }
    return got;
}

RIV_ALWAYS void riv_sem_post(riv_sem *s) {
    int wake = 0;
    spin_locked(&s->guard) {
        s->count++;
        wake = 1;
    }
    if (wake) riv_waitq_wake_one(&s->waiters);
}

#endif /* RIVET_SEM_H */
