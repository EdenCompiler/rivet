/* rivet-ld : minimal locator/linker for the RIVET framework.
 *
 * Reads a layout script that describes memory regions and input binary
 * placements, produces a single flat image with proper padding, and can
 * also emit a GNU ld(1) script for use with cross-toolchains.
 *
 * Layout script syntax (line-based, # for comments):
 *
 *     entry  <symbol_or_addr>
 *     fill   <byte>                     # default fill (default 0x00)
 *     region <name> origin=<addr> length=<bytes> [fill=<byte>]
 *     place  <file.bin> in <region> [at <offset|+offset>]
 *     define <symbol> = <addr>
 *     output <path>                     # final image
 *
 * Numbers accept 0x / decimal / suffix K, M.
 *
 * Examples in tools/sample.ld.txt.
 *
 * Usage:
 *   rivet-ld script.ld                 # link → image as named in `output`
 *   rivet-ld -g script.ld -o out.ld    # emit GNU ld script instead
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef unsigned char  u8;
typedef unsigned int   u32;

#define MAX_REGIONS  16
#define MAX_PLACES   64
#define MAX_DEFS     64
#define MAX_LINE     512

typedef struct {
    char name[32];
    u32  origin;
    u32  length;
    u8   fill;
    int  has_fill;
} region_t;

typedef struct {
    char file[256];
    char region[32];
    u32  offset;          /* relative to region origin */
    int  has_offset;
} place_t;

typedef struct {
    char name[64];
    u32  value;
} def_t;

static region_t regs[MAX_REGIONS];   static int nreg = 0;
static place_t  places[MAX_PLACES];  static int nplc = 0;
static def_t    defs[MAX_DEFS];      static int ndef = 0;
static char     entry[64] = "_start";
static char     output[256] = "image.bin";
static u8       default_fill = 0x00;

static int line_no = 0;

static void die(const char *m, const char *x) {
    fprintf(stderr, "rivet-ld: line %d: %s%s%s\n",
            line_no, m, x ? " " : "", x ? x : "");
    exit(1);
}

static u32 parse_num(const char *s) {
    if (!s || !*s) die("expected number", NULL);
    char *end;
    unsigned long v = strtoul(s, &end, 0);
    if (*end == 'K' || *end == 'k') { v *= 1024; end++; }
    else if (*end == 'M' || *end == 'm') { v *= 1024UL * 1024UL; end++; }
    if (*end != 0 && !isspace((unsigned char)*end))
        die("bad number:", s);
    return (u32)v;
}

static region_t *find_region(const char *n) {
    for (int i = 0; i < nreg; ++i)
        if (strcmp(regs[i].name, n) == 0) return &regs[i];
    return NULL;
}

/* tokenize line on whitespace, '=', or ',' */
static int tokenize(char *line, char *tok[16]) {
    int n = 0; char *p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '=' || *p == ',') p++;
        if (!*p || *p == '#' || *p == '\n' || *p == '\r') break;
        if (n >= 16) die("too many tokens", NULL);
        tok[n++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '='
               && *p != ',' && *p != '\n' && *p != '\r') p++;
        if (*p) *p++ = 0;
    }
    return n;
}

/* parse "k=v" pairs after a leading directive */
static const char *kv_lookup(char **t, int nt, const char *k) {
    for (int i = 0; i + 1 < nt; ++i)
        if (strcmp(t[i], k) == 0) return t[i + 1];
    return NULL;
}

static void parse_line(char *line) {
    char *t[16];
    int n = tokenize(line, t);
    if (n == 0) return;

    if (strcmp(t[0], "entry") == 0) {
        if (n < 2) die("entry needs arg", NULL);
        strncpy(entry, t[1], sizeof(entry) - 1);
        return;
    }
    if (strcmp(t[0], "fill") == 0) {
        if (n < 2) die("fill needs arg", NULL);
        default_fill = (u8)parse_num(t[1]);
        return;
    }
    if (strcmp(t[0], "output") == 0) {
        if (n < 2) die("output needs path", NULL);
        strncpy(output, t[1], sizeof(output) - 1);
        return;
    }
    if (strcmp(t[0], "define") == 0) {
        if (n < 3) die("define needs name and value", NULL);
        if (ndef >= MAX_DEFS) die("too many defines", NULL);
        strncpy(defs[ndef].name, t[1], sizeof(defs[0].name) - 1);
        defs[ndef].value = parse_num(t[2]);
        ndef++;
        return;
    }
    if (strcmp(t[0], "region") == 0) {
        if (n < 2) die("region needs name", NULL);
        if (nreg >= MAX_REGIONS) die("too many regions", NULL);
        region_t *r = &regs[nreg++];
        strncpy(r->name, t[1], sizeof(r->name) - 1);
        const char *org = kv_lookup(t, n, "origin");
        const char *len = kv_lookup(t, n, "length");
        const char *fl  = kv_lookup(t, n, "fill");
        if (!org || !len) die("region needs origin and length", r->name);
        r->origin = parse_num(org);
        r->length = parse_num(len);
        r->fill = fl ? (u8)parse_num(fl) : default_fill;
        r->has_fill = fl != NULL;
        return;
    }
    if (strcmp(t[0], "place") == 0) {
        /* place <file> in <region> [at <offset>] */
        if (n < 4) die("place: bad syntax", NULL);
        if (nplc >= MAX_PLACES) die("too many places", NULL);
        place_t *p = &places[nplc++];
        strncpy(p->file, t[1], sizeof(p->file) - 1);
        if (strcmp(t[2], "in") != 0) die("expected 'in':", t[2]);
        strncpy(p->region, t[3], sizeof(p->region) - 1);
        p->has_offset = 0; p->offset = 0;
        if (n >= 6 && strcmp(t[4], "at") == 0) {
            p->offset = parse_num(t[5]);
            p->has_offset = 1;
        }
        return;
    }
    die("unknown directive:", t[0]);
}

static long file_size(FILE *f) {
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    return n;
}

static int link_image(void) {
    /* per-region cursor for sequential placements without explicit 'at' */
    u32 cursor[MAX_REGIONS] = {0};

    /* allocate image: total size = sum of region lengths,
     * regions packed in declaration order — final addresses derived
     * from each region's origin. Output is the concatenation of
     * region buffers in declaration order. */
    u32 total = 0;
    for (int i = 0; i < nreg; ++i) total += regs[i].length;

    u8 *image = (u8*)malloc(total ? total : 1);
    if (!image) { perror("malloc"); return 1; }

    /* fill each region with its fill byte */
    u32 off = 0;
    for (int i = 0; i < nreg; ++i) {
        memset(image + off, regs[i].fill, regs[i].length);
        off += regs[i].length;
    }

    /* place each input */
    for (int i = 0; i < nplc; ++i) {
        place_t *p = &places[i];
        region_t *r = find_region(p->region);
        if (!r) { fprintf(stderr, "unknown region: %s\n", p->region); return 1; }

        FILE *f = fopen(p->file, "rb");
        if (!f) { perror(p->file); return 1; }
        long sz = file_size(f);

        u32 ridx = (u32)(r - regs);
        u32 base_off = 0;
        for (u32 k = 0; k < ridx; ++k) base_off += regs[k].length;

        u32 reloff = p->has_offset ? p->offset : cursor[ridx];
        if (reloff + (u32)sz > r->length) {
            fprintf(stderr, "place %s overflows region %s (%u > %u)\n",
                    p->file, r->name, reloff + (u32)sz, r->length);
            fclose(f);
            return 1;
        }
        if (fread(image + base_off + reloff, 1, (size_t)sz, f) != (size_t)sz) {
            perror(p->file); fclose(f); return 1;
        }
        fclose(f);
        cursor[ridx] = reloff + (u32)sz;

        fprintf(stderr, "  placed %s -> %s @ 0x%08x (%ld bytes)\n",
                p->file, r->name, r->origin + reloff, sz);
    }

    FILE *fo = fopen(output, "wb");
    if (!fo) { perror(output); return 1; }
    if (fwrite(image, 1, total, fo) != total) { perror(output); fclose(fo); return 1; }
    fclose(fo);
    free(image);

    fprintf(stderr, "rivet-ld: wrote %u bytes to %s (entry=%s)\n",
            total, output, entry);
    return 0;
}

/* emit a GNU ld(1) compatible script */
static int emit_gnu_ld(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); return 1; }
    fprintf(f, "/* generated by rivet-ld */\n");
    fprintf(f, "ENTRY(%s)\n\n", entry);
    fprintf(f, "MEMORY {\n");
    for (int i = 0; i < nreg; ++i) {
        fprintf(f, "  %-8s (rwx) : ORIGIN = 0x%08x, LENGTH = 0x%x\n",
                regs[i].name, regs[i].origin, regs[i].length);
    }
    fprintf(f, "}\n\n");
    fprintf(f, "SECTIONS {\n");
    for (int i = 0; i < nreg; ++i) {
        fprintf(f, "  .%s : {\n", regs[i].name);
        fprintf(f, "    KEEP(*(.vectors))\n");
        fprintf(f, "    *(.text.entry) *(.text.boot) *(.text*)\n");
        fprintf(f, "    *(.rodata*) *(.data*)\n");
        fprintf(f, "  } > %s\n", regs[i].name);
    }
    fprintf(f, "  .bss (NOLOAD) : {\n");
    fprintf(f, "    __bss_start = .;\n");
    fprintf(f, "    *(.bss*) *(COMMON)\n");
    fprintf(f, "    __bss_end = .;\n");
    fprintf(f, "  } > %s\n", nreg > 0 ? regs[nreg - 1].name : "ram");
    fprintf(f, "}\n");
    for (int i = 0; i < ndef; ++i) {
        fprintf(f, "%s = 0x%08x;\n", defs[i].name, defs[i].value);
    }
    fclose(f);
    fprintf(stderr, "rivet-ld: wrote GNU ld script to %s\n", path);
    return 0;
}

static void parse_script(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); exit(1); }
    char line[MAX_LINE];
    line_no = 0;
    while (fgets(line, sizeof(line), f)) {
        line_no++;
        parse_line(line);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    int gen_gnu = 0;
    const char *script = NULL, *out_override = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-g") == 0) gen_gnu = 1;
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out_override = argv[++i];
        else if (argv[i][0] == '-') {
            fprintf(stderr, "unknown flag: %s\n", argv[i]);
            return 1;
        } else script = argv[i];
    }
    if (!script) {
        fprintf(stderr,
            "usage: rivet-ld [-g] script.ld [-o output]\n"
            "  default: produces flat image at 'output' path from script\n"
            "  -g     : emit GNU ld(1) script instead of linking\n");
        return 1;
    }
    parse_script(script);
    if (out_override) {
        strncpy(output, out_override, sizeof(output) - 1);
        output[sizeof(output) - 1] = 0;
    }
    return gen_gnu ? emit_gnu_ld(output) : link_image();
}
