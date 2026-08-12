/* RIVET — pwm.h : pulse-width-modulation channel abstraction. */
#ifndef RIVET_PWM_H
#define RIVET_PWM_H

#include "core.h"

typedef struct riv_pwm_ops {
    const char *name;
    int  (*setup)    (struct riv_pwm_ops *self, unsigned channel,
                      riv_u32 freq_hz);
    int  (*set_duty) (struct riv_pwm_ops *self, unsigned channel,
                      riv_u32 duty_q16);   /* 0..0xFFFF = 0..100% */
    int  (*enable)   (struct riv_pwm_ops *self, unsigned channel);
    int  (*disable)  (struct riv_pwm_ops *self, unsigned channel);
} riv_pwm_ops;

/* Convenience: set duty as a percentage (0..100). */
RIV_ALWAYS int riv_pwm_set_percent(riv_pwm_ops *p, unsigned ch, riv_u8 pct) {
    if (!p || !p->set_duty) return -1;
    if (pct > 100) pct = 100;
    return p->set_duty(p, ch, (riv_u32)pct * 0xFFFF / 100);
}

#endif /* RIVET_PWM_H */
