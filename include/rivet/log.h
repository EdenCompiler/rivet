/* RIVET — log.h : leveled logging on a user-supplied character sink. */
#ifndef RIVET_LOG_H
#define RIVET_LOG_H

#include "core.h"
#include "str.h"

typedef enum { RIV_LOG_ERROR=0, RIV_LOG_WARN, RIV_LOG_INFO, RIV_LOG_DEBUG } riv_log_level;

typedef void (*riv_log_sink_fn)(const char *buf, riv_size len);

extern riv_log_sink_fn riv_log_sink;
extern riv_log_level   riv_log_min;

#define RIV_LOG_DECLARE() \
    riv_log_sink_fn riv_log_sink = (riv_log_sink_fn)0; \
    riv_log_level   riv_log_min  = RIV_LOG_INFO

RIV_ALWAYS void riv_log_set_sink (riv_log_sink_fn f) { riv_log_sink = f; }
RIV_ALWAYS void riv_log_set_level(riv_log_level l)   { riv_log_min  = l; }

static inline RIV_UNUSED void riv_logv(riv_log_level lv, const char *tag, const char *fmt, ...) {
    if (!riv_log_sink || lv > riv_log_min) return;
    static const char *tags[] = { "[ERR ] ", "[WARN] ", "[INFO] ", "[DBG ] " };
    char buf[256];
    int n = riv_snprintf(buf, sizeof(buf), "%s%s: ", tags[lv], tag ? tag : "");
    if (n < 0 || (riv_size)n >= sizeof(buf)) n = (int)sizeof(buf) - 1;
    va_list ap; va_start(ap, fmt);
    int m = riv_vsnprintf(buf + n, sizeof(buf) - (riv_size)n, fmt, ap);
    va_end(ap);
    if (m < 0) m = 0;
    riv_size total = (riv_size)n + (riv_size)m;
    if (total >= sizeof(buf) - 1) total = sizeof(buf) - 2;
    buf[total++] = '\n';
    riv_log_sink(buf, total);
}

#define riv_log_err(tag, ...)  riv_logv(RIV_LOG_ERROR, tag, __VA_ARGS__)
#define riv_log_warn(tag, ...) riv_logv(RIV_LOG_WARN,  tag, __VA_ARGS__)
#define riv_log_info(tag, ...) riv_logv(RIV_LOG_INFO,  tag, __VA_ARGS__)
#define riv_log_dbg(tag, ...)  riv_logv(RIV_LOG_DEBUG, tag, __VA_ARGS__)

#endif /* RIVET_LOG_H */
