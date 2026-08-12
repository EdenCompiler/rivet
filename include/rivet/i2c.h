/* RIVET — i2c.h : I2C master controller abstraction.
 *
 * Caller drives a vtable for any I2C peripheral (STM32, RP2040, etc).
 * Supports start/stop framing, 7-bit and 10-bit addressing, repeated
 * start (combined transfers), and simple polled transactions. */
#ifndef RIVET_I2C_H
#define RIVET_I2C_H

#include "core.h"

#define RIV_I2C_RD     0x0001
#define RIV_I2C_TENBIT 0x0002   /* 10-bit address */
#define RIV_I2C_NOSTOP 0x0004   /* repeated start, no STOP */

typedef struct {
    riv_u16 addr;         /* 7-bit or 10-bit slave */
    riv_u16 flags;        /* OR of RIV_I2C_* */
    riv_u32 len;
    riv_u8 *buf;
} riv_i2c_msg;

typedef struct riv_i2c_ops {
    const char *name;
    int  (*setup)   (struct riv_i2c_ops *self, riv_u32 hz);
    int  (*xfer)    (struct riv_i2c_ops *self, riv_i2c_msg *msgs, int n);
} riv_i2c_ops;

/* Simple single-byte register read/write helpers built on xfer. */
RIV_ALWAYS int riv_i2c_write_reg(riv_i2c_ops *bus, riv_u16 dev,
                                  riv_u8 reg, riv_u8 val) {
    riv_u8 tx[2] = { reg, val };
    riv_i2c_msg m = { dev, 0, 2, tx };
    return bus && bus->xfer ? bus->xfer(bus, &m, 1) : -1;
}

RIV_ALWAYS int riv_i2c_read_reg(riv_i2c_ops *bus, riv_u16 dev,
                                 riv_u8 reg, riv_u8 *out) {
    riv_i2c_msg tx = { dev, RIV_I2C_NOSTOP, 1, &reg };
    riv_i2c_msg rx = { dev, RIV_I2C_RD,     1, out };
    riv_i2c_msg msgs[2] = { tx, rx };
    return bus && bus->xfer ? bus->xfer(bus, msgs, 2) : -1;
}

RIV_ALWAYS int riv_i2c_read_block(riv_i2c_ops *bus, riv_u16 dev,
                                   riv_u8 reg, void *out, riv_u32 n) {
    riv_i2c_msg tx = { dev, RIV_I2C_NOSTOP, 1, &reg };
    riv_i2c_msg rx = { dev, RIV_I2C_RD,     n, (riv_u8*)out };
    riv_i2c_msg msgs[2] = { tx, rx };
    return bus && bus->xfer ? bus->xfer(bus, msgs, 2) : -1;
}

#endif /* RIVET_I2C_H */
