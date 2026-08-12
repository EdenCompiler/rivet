/* RIVET — can.h : Controller Area Network bus abstraction.
 *
 * Supports classic CAN frames (standard + extended IDs) plus a basic
 * remote-frame flag. CAN-FD payloads up to 64 bytes are accommodated
 * by the data array but signaling parameters are driver-defined. */
#ifndef RIVET_CAN_H
#define RIVET_CAN_H

#include "core.h"

#define RIV_CAN_MAX_DLC  64       /* CAN-FD upper bound */

#define RIV_CAN_FLAG_EXT   0x01   /* 29-bit extended ID */
#define RIV_CAN_FLAG_RTR   0x02   /* remote transmission request */
#define RIV_CAN_FLAG_FD    0x04   /* CAN-FD frame */
#define RIV_CAN_FLAG_BRS   0x08   /* bit-rate switch (CAN-FD) */

typedef struct {
    riv_u32 id;
    riv_u8  flags;
    riv_u8  dlc;
    riv_u8  data[RIV_CAN_MAX_DLC];
} riv_can_frame;

typedef struct riv_can_ops {
    const char *name;
    int  (*setup)(struct riv_can_ops *self, riv_u32 bitrate_bps);
    int  (*xmit) (struct riv_can_ops *self, const riv_can_frame *f);
    int  (*recv) (struct riv_can_ops *self, riv_can_frame *out);
    int  (*tx_ready)(struct riv_can_ops *self);
    int  (*rx_ready)(struct riv_can_ops *self);
} riv_can_ops;

RIV_ALWAYS int riv_can_send(riv_can_ops *c, const riv_can_frame *f) {
    return c && c->xmit ? c->xmit(c, f) : -1;
}
RIV_ALWAYS int riv_can_recv(riv_can_ops *c, riv_can_frame *out) {
    return c && c->recv ? c->recv(c, out) : -1;
}

#endif /* RIVET_CAN_H */
