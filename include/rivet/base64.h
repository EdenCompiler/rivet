/* RIVET — base64.h : base64 encode + decode.
 *
 * Standard RFC-4648 alphabet (A-Z a-z 0-9 + /), `=` padding. Helpers
 * are buffer-safe and return the number of bytes produced. */
#ifndef RIVET_BASE64_H
#define RIVET_BASE64_H

#include "core.h"

static const char riv_b64_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

RIV_ALWAYS riv_size riv_base64_encoded_size(riv_size n) {
    return ((n + 2) / 3) * 4;
}
RIV_ALWAYS riv_size riv_base64_decoded_max(riv_size n) {
    return (n / 4) * 3;
}

RIV_ALWAYS riv_size riv_base64_encode(const void *in_, riv_size n,
                                       char *out, riv_size cap) {
    const riv_u8 *in = (const riv_u8*)in_;
    riv_size needed = riv_base64_encoded_size(n);
    if (cap < needed + 1) return 0;
    riv_size o = 0;
    for (riv_size i = 0; i < n; i += 3) {
        riv_u32 t = (riv_u32)in[i] << 16;
        if (i + 1 < n) t |= (riv_u32)in[i + 1] << 8;
        if (i + 2 < n) t |=  (riv_u32)in[i + 2];
        out[o++] = riv_b64_alphabet[(t >> 18) & 0x3F];
        out[o++] = riv_b64_alphabet[(t >> 12) & 0x3F];
        out[o++] = (i + 1 < n) ? riv_b64_alphabet[(t >> 6) & 0x3F] : '=';
        out[o++] = (i + 2 < n) ? riv_b64_alphabet[ t       & 0x3F] : '=';
    }
    out[o] = 0;
    return o;
}

RIV_ALWAYS int _riv_b64_val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

RIV_ALWAYS riv_size riv_base64_decode(const char *in, riv_size n,
                                       void *out_, riv_size cap) {
    riv_u8 *out = (riv_u8*)out_;
    riv_size o = 0;
    for (riv_size i = 0; i + 3 < n; i += 4) {
        int v0 = _riv_b64_val(in[i]);
        int v1 = _riv_b64_val(in[i + 1]);
        int v2 = (in[i + 2] == '=') ? 0 : _riv_b64_val(in[i + 2]);
        int v3 = (in[i + 3] == '=') ? 0 : _riv_b64_val(in[i + 3]);
        if (v0 < 0 || v1 < 0) return 0;
        if (o >= cap) return o;
        out[o++] = (riv_u8)((v0 << 2) | (v1 >> 4));
        if (in[i + 2] != '=' && v2 >= 0) {
            if (o >= cap) return o;
            out[o++] = (riv_u8)((v1 << 4) | (v2 >> 2));
        }
        if (in[i + 3] != '=' && v3 >= 0) {
            if (o >= cap) return o;
            out[o++] = (riv_u8)((v2 << 6) | v3);
        }
    }
    return o;
}

#endif /* RIVET_BASE64_H */
