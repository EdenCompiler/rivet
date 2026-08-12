/* RIVET — gpio.h : generic GPIO controller abstraction.
 *
 * A `riv_gpio_ops` vtable lets the framework drive any GPIO peripheral
 * uniformly. Each controller registers a function table; user code
 * passes a controller pointer + a (bank, pin) pair. */
#ifndef RIVET_GPIO_H
#define RIVET_GPIO_H

#include "core.h"

typedef enum {
    RIV_GPIO_IN  = 0,
    RIV_GPIO_OUT = 1,
    RIV_GPIO_ALT = 2,           /* alternate function */
    RIV_GPIO_ANALOG = 3,
} riv_gpio_dir;

typedef enum {
    RIV_GPIO_PULL_NONE = 0,
    RIV_GPIO_PULL_UP   = 1,
    RIV_GPIO_PULL_DOWN = 2,
} riv_gpio_pull;

typedef enum {
    RIV_GPIO_EDGE_RISING  = 1,
    RIV_GPIO_EDGE_FALLING = 2,
    RIV_GPIO_EDGE_BOTH    = 3,
    RIV_GPIO_LEVEL_HIGH   = 4,
    RIV_GPIO_LEVEL_LOW    = 5,
} riv_gpio_trigger;

typedef struct riv_gpio_ops {
    int  (*configure)(struct riv_gpio_ops *self, unsigned bank, unsigned pin,
                      riv_gpio_dir dir, riv_gpio_pull pull);
    int  (*set_alt)  (struct riv_gpio_ops *self, unsigned bank, unsigned pin,
                      unsigned af);
    int  (*read)     (struct riv_gpio_ops *self, unsigned bank, unsigned pin);
    void (*write)    (struct riv_gpio_ops *self, unsigned bank, unsigned pin,
                      int value);
    void (*toggle)   (struct riv_gpio_ops *self, unsigned bank, unsigned pin);
    int  (*irq_set)  (struct riv_gpio_ops *self, unsigned bank, unsigned pin,
                      riv_gpio_trigger trig, void (*cb)(void *ctx), void *ctx);
} riv_gpio_ops;

/* Convenience wrappers that null-check the callback before invoking. */
RIV_ALWAYS int  riv_gpio_configure(riv_gpio_ops *g, unsigned b, unsigned p,
                                    riv_gpio_dir d, riv_gpio_pull pu) {
    return g && g->configure ? g->configure(g, b, p, d, pu) : -1;
}
RIV_ALWAYS int  riv_gpio_read (riv_gpio_ops *g, unsigned b, unsigned p) {
    return g && g->read ? g->read(g, b, p) : -1;
}
RIV_ALWAYS void riv_gpio_write(riv_gpio_ops *g, unsigned b, unsigned p, int v) {
    if (g && g->write) g->write(g, b, p, v);
}
RIV_ALWAYS void riv_gpio_toggle(riv_gpio_ops *g, unsigned b, unsigned p) {
    if (g && g->toggle) g->toggle(g, b, p);
}

#endif /* RIVET_GPIO_H */
