/* RIVET — adc.h : analog-to-digital converter abstraction. */
#ifndef RIVET_ADC_H
#define RIVET_ADC_H

#include "core.h"

typedef struct riv_adc_ops {
    const char *name;
    riv_u8      resolution_bits;       /* 8, 10, 12, 16, ... */
    riv_u32     ref_mv;                /* reference voltage in millivolts */
    int  (*setup)  (struct riv_adc_ops *self);
    int  (*sample) (struct riv_adc_ops *self, unsigned channel,
                    riv_u32 *out_raw);
} riv_adc_ops;

RIV_ALWAYS int riv_adc_sample(riv_adc_ops *a, unsigned ch, riv_u32 *out) {
    return a && a->sample ? a->sample(a, ch, out) : -1;
}

/* Convert a raw count into millivolts using the device's reference. */
RIV_ALWAYS riv_u32 riv_adc_to_mv(const riv_adc_ops *a, riv_u32 raw) {
    if (!a || !a->resolution_bits) return 0;
    riv_u32 max_count = (1u << a->resolution_bits) - 1u;
    return (raw * a->ref_mv) / max_count;
}

#endif /* RIVET_ADC_H */
