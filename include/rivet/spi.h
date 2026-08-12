/* RIVET — spi.h : SPI master abstraction.
 *
 * Supports full-duplex and half-duplex (tx-only / rx-only) transfers
 * with explicit CS control. Backends configure mode, bit order, and
 * clock divider through `setup`. */
#ifndef RIVET_SPI_H
#define RIVET_SPI_H

#include "core.h"

#define RIV_SPI_MODE0   0
#define RIV_SPI_MODE1   1
#define RIV_SPI_MODE2   2
#define RIV_SPI_MODE3   3

#define RIV_SPI_MSB     0
#define RIV_SPI_LSB     1

typedef struct {
    const riv_u8 *tx;       /* may be NULL for rx-only */
    riv_u8       *rx;       /* may be NULL for tx-only */
    riv_u32       len;
    int           cs_keep;  /* 1 = leave CS asserted after transfer */
} riv_spi_xfer;

typedef struct riv_spi_ops {
    const char *name;
    int  (*setup) (struct riv_spi_ops *self, riv_u32 hz, int mode, int bit_order);
    int  (*xfer)  (struct riv_spi_ops *self, riv_spi_xfer *x);
    void (*cs_lo) (struct riv_spi_ops *self, unsigned slave);
    void (*cs_hi) (struct riv_spi_ops *self, unsigned slave);
} riv_spi_ops;

/* Half-duplex helpers. */
RIV_ALWAYS int riv_spi_send(riv_spi_ops *b, const void *p, riv_u32 n) {
    riv_spi_xfer x = { (const riv_u8*)p, (riv_u8*)0, n, 0 };
    return b && b->xfer ? b->xfer(b, &x) : -1;
}
RIV_ALWAYS int riv_spi_recv(riv_spi_ops *b, void *p, riv_u32 n) {
    riv_spi_xfer x = { (const riv_u8*)0, (riv_u8*)p, n, 0 };
    return b && b->xfer ? b->xfer(b, &x) : -1;
}

#endif /* RIVET_SPI_H */
