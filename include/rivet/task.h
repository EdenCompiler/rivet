/* RIVET — task.h : cooperative tasks (super-loop scheduler) */
#ifndef RIVET_TASK_H
#define RIVET_TASK_H

#include "core.h"
#include "irq.h"

typedef void (*riv_task_fn)(void *ctx);

typedef struct riv_task {
    riv_task_fn  run;
    void       *ctx;
    riv_u32      period_ticks;
    riv_u32      next_tick;
    riv_bool     enabled;
} riv_task;

typedef struct riv_scheduler {
    riv_task   *tasks;
    riv_size    count;
    volatile riv_u32 ticks;
} riv_scheduler;

RIV_ALWAYS void riv_sched_init(riv_scheduler *s, riv_task *t, riv_size n) {
    s->tasks = t; s->count = n; s->ticks = 0;
    for (riv_size i = 0; i < n; ++i) {
        t[i].next_tick = t[i].period_ticks;
        t[i].enabled = RIV_TRUE;
    }
}

/* call from tick ISR */
RIV_ALWAYS void riv_sched_tick(riv_scheduler *s) { s->ticks++; }

/* call from main super-loop */
RIV_ALWAYS void riv_sched_run(riv_scheduler *s) {
    riv_u32 now = s->ticks;
    for (riv_size i = 0; i < s->count; ++i) {
        riv_task *t = &s->tasks[i];
        if (!t->enabled) continue;
        if ((riv_i32)(now - t->next_tick) >= 0) {
            t->next_tick = now + t->period_ticks;
            t->run(t->ctx);
        }
    }
}

/* DSL: define a periodic task entry */
#define task_entry(fn, ctxp, period) \
    { .run = (fn), .ctx = (ctxp), .period_ticks = (period), .next_tick = 0, .enabled = RIV_TRUE }

#endif /* RIVET_TASK_H */
