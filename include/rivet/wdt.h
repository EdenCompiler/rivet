/* RIVET — wdt.h : watchdog timer.
 *
 * Three-call lifecycle: enable, kick (periodically), and optional
 * disable. Driver supplies the timeout-in-milliseconds rounding. */
#ifndef RIVET_WDT_H
#define RIVET_WDT_H

#include "core.h"

typedef struct riv_wdt_ops {
    const char *name;
    int  (*enable) (struct riv_wdt_ops *self, riv_u32 timeout_ms);
    void (*kick)   (struct riv_wdt_ops *self);
    int  (*disable)(struct riv_wdt_ops *self);
    int  (*last_reset_was_wdt)(struct riv_wdt_ops *self);
} riv_wdt_ops;

RIV_ALWAYS int  riv_wdt_enable (riv_wdt_ops *w, riv_u32 ms) {
    return w && w->enable ? w->enable(w, ms) : -1;
}
RIV_ALWAYS void riv_wdt_kick   (riv_wdt_ops *w) {
    if (w && w->kick) w->kick(w);
}
RIV_ALWAYS int  riv_wdt_disable(riv_wdt_ops *w) {
    return w && w->disable ? w->disable(w) : -1;
}

#endif /* RIVET_WDT_H */
