/* RIVET — rtc.h : real-time clock abstraction.
 *
 * Provides absolute wall-clock time as a broken-down struct plus a
 * 64-bit Unix epoch (seconds since 1970-01-01 UTC). Drivers fill in
 * `get` / `set`; helpers below convert between the two views. */
#ifndef RIVET_RTC_H
#define RIVET_RTC_H

#include "core.h"

typedef struct {
    riv_u16 year;        /* full year, e.g. 2026 */
    riv_u8  month;       /* 1..12 */
    riv_u8  day;         /* 1..31 */
    riv_u8  hour;        /* 0..23 */
    riv_u8  minute;      /* 0..59 */
    riv_u8  second;      /* 0..59 */
    riv_u8  weekday;     /* 0=Sunday..6=Saturday, driver-defined */
} riv_rtc_time;

typedef struct riv_rtc_ops {
    const char *name;
    int  (*get)(struct riv_rtc_ops *self, riv_rtc_time *out);
    int  (*set)(struct riv_rtc_ops *self, const riv_rtc_time *in);
} riv_rtc_ops;

RIV_ALWAYS int riv_rtc_get(riv_rtc_ops *r, riv_rtc_time *t) {
    return r && r->get ? r->get(r, t) : -1;
}

/* Convert broken-down time to Unix epoch seconds. */
RIV_ALWAYS riv_i64 riv_rtc_to_epoch(const riv_rtc_time *t) {
    static const int days_per_month[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    riv_i64 years = (riv_i64)t->year - 1970;
    riv_i64 days  = years * 365 + (years + 1) / 4 - (years + 69) / 100
                  + (years + 369) / 400;
    for (int m = 0; m < t->month - 1; ++m) days += days_per_month[m];
    int y = t->year;
    if (t->month > 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) days++;
    days += t->day - 1;
    return ((days * 24 + t->hour) * 60 + t->minute) * 60 + t->second;
}

#endif /* RIVET_RTC_H */
