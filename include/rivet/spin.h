/* RIVET — spin.h : test-and-set spinlock with IRQ-save variant. */
#ifndef RIVET_SPIN_H
#define RIVET_SPIN_H

#include "atomic.h"
#include "irq.h"
#include "mem.h"

typedef struct { riv_atomic32 lock; } riv_spin;

#define RIV_SPIN_INIT  { RIV_ATOMIC_INIT(0) }

RIV_ALWAYS void riv_spin_init(riv_spin *s) { riv_atomic_store(&s->lock, 0); }

RIV_ALWAYS void riv_spin_lock(riv_spin *s) {
    for (;;) {
        riv_u32 zero = 0;
        if (riv_atomic_cas(&s->lock, &zero, 1)) return;
        while (riv_atomic_load(&s->lock)) riv_cpu_relax();
    }
}

RIV_ALWAYS int riv_spin_trylock(riv_spin *s) {
    riv_u32 zero = 0;
    return riv_atomic_cas(&s->lock, &zero, 1);
}

RIV_ALWAYS void riv_spin_unlock(riv_spin *s) { riv_atomic_store(&s->lock, 0); }

/* IRQ-save variant: returns saved state to be passed back to unlock_irq. */
RIV_ALWAYS riv_irq_state riv_spin_lock_irqsave(riv_spin *s) {
    riv_irq_state st = riv_irq_save();
    riv_spin_lock(s);
    return st;
}
RIV_ALWAYS void riv_spin_unlock_irqrestore(riv_spin *s, riv_irq_state st) {
    riv_spin_unlock(s);
    riv_irq_restore(st);
}

/* scoped block: `spin_locked(&s) { ... }` */
#define spin_locked(s) \
    for (riv_irq_state _riv_st = riv_spin_lock_irqsave(s), _riv_k = 0; \
         !_riv_k; _riv_k = 1, riv_spin_unlock_irqrestore(s, _riv_st))

#endif /* RIVET_SPIN_H */
