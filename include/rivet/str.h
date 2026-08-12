/* RIVET — str.h : freestanding string ops + integer formatting +
 * minimal vsnprintf (supports %d %u %x %X %p %s %c %% and width/0-pad). */
#ifndef RIVET_STR_H
#define RIVET_STR_H

#include "core.h"

RIV_ALWAYS riv_size riv_strlen(const char *s) {
    const char *p = s; while (*p) ++p; return (riv_size)(p - s);
}
RIV_ALWAYS int riv_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
RIV_ALWAYS int riv_strncmp(const char *a, const char *b, riv_size n) {
    while (n && *a && *a == *b) { ++a; ++b; --n; }
    if (!n) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
RIV_ALWAYS char *riv_strcpy(char *d, const char *s) {
    char *r = d; while ((*d++ = *s++)) { } return r;
}
RIV_ALWAYS riv_size riv_strlcpy(char *d, const char *s, riv_size cap) {
    riv_size n = 0;
    if (cap == 0) return riv_strlen(s);
    while (--cap && (*d++ = *s)) { ++s; ++n; }
    *d = 0;
    while (*s++) ++n;
    return n;
}

/* unsigned to string in base 2..16; returns length written (no NUL) */
RIV_ALWAYS riv_size riv_u64_to_str(riv_u64 v, unsigned base, int upper, char *out) {
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[24]; riv_size n = 0;
    if (base < 2 || base > 16) return 0;
    do { tmp[n++] = dig[v % base]; v /= base; } while (v);
    for (riv_size i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    return n;
}

/* signed decimal */
RIV_ALWAYS riv_size riv_i64_to_str(riv_i64 v, char *out) {
    if (v < 0) {
        *out++ = '-';
        riv_u64 u = (riv_u64)(-(v + 1)) + 1;   /* avoid INT_MIN overflow */
        return 1 + riv_u64_to_str(u, 10, 0, out);
    }
    return riv_u64_to_str((riv_u64)v, 10, 0, out);
}

/* tiny vsnprintf — returns number of bytes that would have been written.
 * always NUL-terminates if cap > 0. */
#include <stdarg.h>
static inline int riv_vsnprintf(char *buf, riv_size cap, const char *fmt, va_list ap) {
    riv_size pos = 0;
    #define PUT(c) do { if (pos + 1 < cap) buf[pos] = (char)(c); ++pos; } while (0)
    while (*fmt) {
        if (*fmt != '%') { PUT(*fmt++); continue; }
        ++fmt;
        int zero_pad = 0, width = 0;
        if (*fmt == '0') { zero_pad = 1; ++fmt; }
        while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt - '0'); ++fmt; }
        char tmp[32]; riv_size n = 0; const char *src = tmp;
        switch (*fmt) {
        case 'd': case 'i': n = riv_i64_to_str((riv_i64)va_arg(ap, int), tmp); break;
        case 'u': n = riv_u64_to_str((riv_u64)(unsigned)va_arg(ap, unsigned), 10, 0, tmp); break;
        case 'x': n = riv_u64_to_str((riv_u64)(unsigned)va_arg(ap, unsigned), 16, 0, tmp); break;
        case 'X': n = riv_u64_to_str((riv_u64)(unsigned)va_arg(ap, unsigned), 16, 1, tmp); break;
        case 'p': {
            tmp[0] = '0'; tmp[1] = 'x';
            n = 2 + riv_u64_to_str((riv_u64)(riv_uptr)va_arg(ap, void*), 16, 0, tmp + 2);
        } break;
        case 's': {
            const char *s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            n = riv_strlen(s); src = s;
        } break;
        case 'c': tmp[0] = (char)va_arg(ap, int); n = 1; break;
        case '%': tmp[0] = '%'; n = 1; break;
        default:  tmp[0] = '%'; tmp[1] = *fmt; n = 2; break;
        }
        while (n < (riv_size)width) { PUT(zero_pad ? '0' : ' '); --width; }
        for (riv_size i = 0; i < n; ++i) PUT(src[i]);
        ++fmt;
    }
    if (cap > 0) buf[pos < cap ? pos : cap - 1] = 0;
    #undef PUT
    return (int)pos;
}

static inline RIV_UNUSED int riv_snprintf(char *buf, riv_size cap, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = riv_vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return r;
}

#endif /* RIVET_STR_H */
