/* RIVET — fb.h : linear framebuffer (32 bpp ARGB).
 *
 * Generic abstraction over any flat MMIO display surface. Handles
 * pixel plot, fill, blit, and an embedded 8x8 monospace font for
 * debug text. Callers supply the framebuffer base, pitch, and
 * dimensions — discovery (VESA/VBE/UEFI GOP/device-tree) is the
 * caller's responsibility. */
#ifndef RIVET_FB_H
#define RIVET_FB_H

#include "core.h"
#include "mem.h"

typedef struct {
    volatile riv_u32 *pixels;
    riv_u32 width;
    riv_u32 height;
    riv_u32 pitch_px;     /* number of u32 between rows */
} riv_fb;

#define RIV_RGB(r,g,b)   (((riv_u32)(r) << 16) | ((riv_u32)(g) << 8) | (riv_u32)(b))
#define RIV_ARGB(a,r,g,b) (((riv_u32)(a) << 24) | RIV_RGB(r,g,b))

RIV_ALWAYS void riv_fb_init(riv_fb *fb, void *base, riv_u32 w, riv_u32 h,
                             riv_u32 pitch_bytes) {
    fb->pixels   = (volatile riv_u32*)base;
    fb->width    = w;
    fb->height   = h;
    fb->pitch_px = pitch_bytes / 4;
}

RIV_ALWAYS void riv_fb_plot(riv_fb *fb, riv_u32 x, riv_u32 y, riv_u32 color) {
    if (x >= fb->width || y >= fb->height) return;
    fb->pixels[y * fb->pitch_px + x] = color;
}

RIV_ALWAYS void riv_fb_clear(riv_fb *fb, riv_u32 color) {
    for (riv_u32 y = 0; y < fb->height; ++y) {
        volatile riv_u32 *row = fb->pixels + y * fb->pitch_px;
        for (riv_u32 x = 0; x < fb->width; ++x) row[x] = color;
    }
}

RIV_ALWAYS void riv_fb_fill(riv_fb *fb, riv_u32 x, riv_u32 y,
                             riv_u32 w, riv_u32 h, riv_u32 color) {
    for (riv_u32 j = 0; j < h; ++j) {
        for (riv_u32 i = 0; i < w; ++i) {
            riv_fb_plot(fb, x + i, y + j, color);
        }
    }
}

/* Compact built-in 8x8 font for ASCII 0x20..0x7E. Each glyph is 8 bytes,
 * MSB-left. This table covers a subset that is enough for debug text;
 * extend at will. */
extern const riv_u8 riv_fb_font8x8[96][8];
#define RIV_FB_FONT8X8_DECLARE() \
    const riv_u8 riv_fb_font8x8[96][8] = {  /* space..~ */                 \
        [0x00]={0,0,0,0,0,0,0,0},               /* space */                \
        [0x21]={0x18,0x18,0x18,0x18,0x18,0,0x18,0}, /* ! */                \
        [0x2D]={0,0,0,0x7E,0,0,0,0},            /* - */                    \
        [0x2E]={0,0,0,0,0,0x18,0x18,0},         /* . */                    \
        [0x30]={0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0}, /* 0 */             \
        [0x31]={0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0}, /* 1 */             \
        [0x32]={0x3C,0x66,0x06,0x1C,0x30,0x66,0x7E,0}, /* 2 */             \
        [0x33]={0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0}, /* 3 */             \
        [0x34]={0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0}, /* 4 */             \
        [0x35]={0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0}, /* 5 */             \
        [0x36]={0x3C,0x60,0x60,0x7C,0x66,0x66,0x3C,0}, /* 6 */             \
        [0x37]={0x7E,0x06,0x0C,0x18,0x18,0x18,0x18,0}, /* 7 */             \
        [0x38]={0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0}, /* 8 */             \
        [0x39]={0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0}, /* 9 */             \
        [0x3A]={0,0x18,0x18,0,0,0x18,0x18,0},          /* : */             \
        [0x41]={0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0}, /* A */             \
        [0x42]={0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0}, /* B */             \
        [0x43]={0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0}, /* C */             \
        [0x44]={0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0}, /* D */             \
        [0x45]={0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0}, /* E */             \
        [0x46]={0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0}, /* F */             \
        [0x47]={0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0}, /* G */             \
        [0x48]={0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0}, /* H */             \
        [0x49]={0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0}, /* I */             \
        [0x4C]={0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0}, /* L */             \
        [0x4F]={0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0}, /* O */             \
        [0x50]={0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0}, /* P */             \
        [0x52]={0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0}, /* R */             \
        [0x53]={0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0}, /* S */             \
        [0x54]={0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0}, /* T */             \
        [0x56]={0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0}, /* V */             \
        [0x59]={0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0}, /* Y */             \
    }

RIV_ALWAYS void riv_fb_glyph(riv_fb *fb, riv_u32 x, riv_u32 y,
                              char c, riv_u32 fg, riv_u32 bg) {
    int idx = (c >= 0x20 && c < 0x7F) ? (c - 0x20) : 0;
    for (int row = 0; row < 8; ++row) {
        riv_u8 bits = riv_fb_font8x8[idx][row];
        for (int col = 0; col < 8; ++col) {
            riv_u32 px = (bits & (0x80 >> col)) ? fg : bg;
            riv_fb_plot(fb, x + (riv_u32)col, y + (riv_u32)row, px);
        }
    }
}

RIV_ALWAYS void riv_fb_puts(riv_fb *fb, riv_u32 x, riv_u32 y,
                             const char *s, riv_u32 fg, riv_u32 bg) {
    for (; *s; ++s) {
        riv_fb_glyph(fb, x, y, *s, fg, bg);
        x += 8;
    }
}

#endif /* RIVET_FB_H */
