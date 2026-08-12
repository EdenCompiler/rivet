/* RIVET — pipe.h : blocking byte pipe IPC.
 *
 * A pipe is a fixed-capacity ring buffer plus two wait queues: readers
 * block when empty, writers block when full. Capacity must be a power
 * of two. The pipe is process-agnostic — it's the user's choice to put
 * one end into a fd table for syscall-based IPC. */
#ifndef RIVET_PIPE_H
#define RIVET_PIPE_H

#include "ring.h"
#include "spin.h"
#include "waitq.h"

#define RIV_PIPE_CAP 256

typedef struct {
    riv_u8     buf[RIV_PIPE_CAP];
    riv_ring   ring;
    riv_spin   guard;
    riv_waitq  readers;
    riv_waitq  writers;
    int        closed;
} riv_pipe;

RIV_ALWAYS void riv_pipe_init(riv_pipe *p) {
    riv_ring_init(&p->ring, p->buf, RIV_PIPE_CAP);
    riv_spin_init(&p->guard);
    riv_waitq_init(&p->readers);
    riv_waitq_init(&p->writers);
    p->closed = 0;
}

/* Blocking write: returns number of bytes written, or -1 on closed pipe. */
RIV_ALWAYS int riv_pipe_write(riv_pipe *p, const void *src, riv_u32 n) {
    const riv_u8 *s = (const riv_u8*)src;
    riv_u32 done = 0;
    while (done < n) {
        riv_spin_lock(&p->guard);
        if (p->closed) { riv_spin_unlock(&p->guard); return -1; }
        riv_u32 w = riv_ring_write(&p->ring, s + done, n - done);
        riv_spin_unlock(&p->guard);
        done += w;
        if (w > 0) riv_waitq_wake_one(&p->readers);
        if (done < n) riv_waitq_sleep(&p->writers, (riv_spin*)0);
    }
    return (int)done;
}

/* Blocking read: returns bytes read; 0 indicates EOF on closed-and-empty. */
RIV_ALWAYS int riv_pipe_read(riv_pipe *p, void *dst, riv_u32 n) {
    riv_u8 *d = (riv_u8*)dst;
    for (;;) {
        riv_spin_lock(&p->guard);
        riv_u32 r = riv_ring_read(&p->ring, d, n);
        int closed = p->closed;
        riv_spin_unlock(&p->guard);
        if (r > 0) {
            riv_waitq_wake_one(&p->writers);
            return (int)r;
        }
        if (closed) return 0;
        riv_waitq_sleep(&p->readers, (riv_spin*)0);
    }
}

RIV_ALWAYS void riv_pipe_close(riv_pipe *p) {
    spin_locked(&p->guard) { p->closed = 1; }
    riv_waitq_wake_all(&p->readers);
    riv_waitq_wake_all(&p->writers);
}

#endif /* RIVET_PIPE_H */
