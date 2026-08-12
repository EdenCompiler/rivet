/* RIVET — mutex.h : sleeping mutex.
 *
 * Like a spinlock, but blocks the caller on a wait queue rather than
 * busy-waiting. Use this whenever the critical section may be long or
 * may itself block. Owner-tracked: must be unlocked by the same task. */
#ifndef RIVET_MUTEX_H
#define RIVET_MUTEX_H

#include "spin.h"
#include "waitq.h"
#include "proc.h"

typedef struct {
    riv_spin   guard;     /* protects owner+waiters */
    riv_waitq  waiters;
    riv_proc  *owner;     /* NULL when unlocked */
} riv_mutex;

#define RIV_MUTEX_INIT(name) \
    { RIV_SPIN_INIT, RIV_WAITQ_INIT((name).waiters), (riv_proc*)0 }

RIV_ALWAYS void riv_mutex_init(riv_mutex *m) {
    riv_spin_init(&m->guard);
    riv_waitq_init(&m->waiters);
    m->owner = (riv_proc*)0;
}

RIV_ALWAYS void riv_mutex_lock(riv_mutex *m) {
    riv_proc *me = riv_proc_current();
    for (;;) {
        riv_spin_lock(&m->guard);
        if (!m->owner) { m->owner = me; riv_spin_unlock(&m->guard); return; }
        riv_waitq_sleep(&m->waiters, &m->guard);
    }
}

RIV_ALWAYS int riv_mutex_trylock(riv_mutex *m) {
    int got = 0;
    spin_locked(&m->guard) {
        if (!m->owner) { m->owner = riv_proc_current(); got = 1; }
    }
    return got;
}

RIV_ALWAYS void riv_mutex_unlock(riv_mutex *m) {
    spin_locked(&m->guard) {
        m->owner = (riv_proc*)0;
    }
    riv_waitq_wake_one(&m->waiters);
}

#endif /* RIVET_MUTEX_H */
