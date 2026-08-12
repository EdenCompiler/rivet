/* RIVET — kbd.h : PS/2 keyboard driver (scancode set 1).
 *
 * Poll-mode reader for the standard PC keyboard controller (port 0x60
 * data, port 0x64 status/command). Maintains modifier state and
 * translates scancodes to ASCII via two small lookup tables. */
#ifndef RIVET_KBD_H
#define RIVET_KBD_H

#include "core.h"
#include "io.h"

#if RIVET_ARCH_X86 || RIVET_ARCH_X86_64

#define RIV_KBD_DATA   0x60
#define RIV_KBD_STAT   0x64

#define RIV_KBD_OBF    0x01     /* output buffer full → byte available */

/* Compact set-1 → ASCII tables. Entries marked 0 are non-printables
 * (function keys, modifiers, special keys). */
static const char riv_kbd_map_lo[128] = {
    0,    27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0,    '*', 0,   ' '
};
static const char riv_kbd_map_hi[128] = {
    0,    27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    0,    '*', 0,   ' '
};

typedef struct {
    unsigned shift   : 1;
    unsigned ctrl    : 1;
    unsigned alt     : 1;
    unsigned capslock: 1;
    unsigned ext     : 1;     /* 0xE0 prefix seen */
} riv_kbd_state;

/* Returns 1 if a byte is waiting in the controller. */
RIV_ALWAYS int riv_kbd_has_byte(void) {
    return riv_inb(RIV_KBD_STAT) & RIV_KBD_OBF;
}

/* Non-blocking poll: returns a decoded ASCII character, or 0 if no
 * complete key event is available. Updates internal modifier state. */
RIV_ALWAYS int riv_kbd_poll(riv_kbd_state *st) {
    if (!riv_kbd_has_byte()) return 0;
    riv_u8 sc = riv_inb(RIV_KBD_DATA);

    if (sc == 0xE0) { st->ext = 1; return 0; }

    int release = sc & 0x80;
    riv_u8 code = sc & 0x7F;

    switch (code) {
    case 0x2A: case 0x36: st->shift = !release; return 0;
    case 0x1D:            st->ctrl  = !release; return 0;
    case 0x38:            st->alt   = !release; return 0;
    case 0x3A:            if (!release) st->capslock ^= 1; return 0;
    }
    st->ext = 0;
    if (release) return 0;
    if (code >= 128) return 0;

    int upper = st->shift ^ st->capslock;
    int c = upper ? riv_kbd_map_hi[code] : riv_kbd_map_lo[code];
    if (c == 0) return 0;
    if (st->ctrl && c >= 'a' && c <= 'z') c -= 0x60;     /* Ctrl-letter */
    return c;
}

#endif /* x86 */

#endif /* RIVET_KBD_H */
