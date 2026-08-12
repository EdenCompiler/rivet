/* rivet-objcopy : convert flat binary <-> Intel HEX / Motorola SREC.
 *
 * Usage:
 *   rivet-objcopy -O ihex   in.bin out.hex [-b base_addr]
 *   rivet-objcopy -O srec   in.bin out.s19 [-b base_addr]
 *   rivet-objcopy -I ihex   in.hex out.bin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char  u8;
typedef unsigned int   u32;

static int hex_val(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* ---------- bin -> Intel HEX ---------- */
static int bin_to_ihex(const char *in, const char *out, u32 base) {
    FILE *fi = fopen(in, "rb"); if (!fi) { perror(in); return 1; }
    FILE *fo = fopen(out, "w"); if (!fo) { perror(out); fclose(fi); return 1; }
    u32 addr = base;
    u32 last_upper = (u32)-1;
    u8 buf[16];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fi)) > 0) {
        u32 upper = addr >> 16;
        if (upper != last_upper) {
            u8 b1 = (u8)(upper >> 8), b2 = (u8)upper;
            u8 sum = (u8)(2 + 0 + 0 + 4 + b1 + b2);
            sum = (u8)(0x100 - sum);
            fprintf(fo, ":02000004%02X%02X%02X\n", b1, b2, sum);
            last_upper = upper;
        }
        u8 sum = (u8)n + (u8)(addr >> 8) + (u8)addr + 0x00;
        fprintf(fo, ":%02X%04X00", (unsigned)n, addr & 0xFFFF);
        for (size_t i = 0; i < n; ++i) { fprintf(fo, "%02X", buf[i]); sum = (u8)(sum + buf[i]); }
        sum = (u8)(0x100 - sum);
        fprintf(fo, "%02X\n", sum);
        addr += (u32)n;
    }
    fprintf(fo, ":00000001FF\n");
    fclose(fi); fclose(fo);
    return 0;
}

/* ---------- bin -> SREC (S0 header, S3 data, S7 end) ---------- */
static int bin_to_srec(const char *in, const char *out, u32 base) {
    FILE *fi = fopen(in, "rb"); if (!fi) { perror(in); return 1; }
    FILE *fo = fopen(out, "w"); if (!fo) { perror(out); fclose(fi); return 1; }

    /* S0: header "HDR" */
    {
        const char *hdr = "HDR";
        u8 cnt = 3 + 2 + 1;
        u8 sum = (u8)(cnt + 0 + 0);
        for (int i = 0; i < 3; ++i) sum = (u8)(sum + hdr[i]);
        fprintf(fo, "S0%02X0000", cnt);
        for (int i = 0; i < 3; ++i) fprintf(fo, "%02X", (u8)hdr[i]);
        fprintf(fo, "%02X\n", (u8)~sum);
    }

    u32 addr = base;
    u8 buf[16];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fi)) > 0) {
        u8 cnt = (u8)(n + 4 + 1);
        u8 sum = cnt;
        sum = (u8)(sum + (addr >> 24) + (addr >> 16) + (addr >> 8) + addr);
        fprintf(fo, "S3%02X%08X", cnt, addr);
        for (size_t i = 0; i < n; ++i) { fprintf(fo, "%02X", buf[i]); sum = (u8)(sum + buf[i]); }
        fprintf(fo, "%02X\n", (u8)~sum);
        addr += (u32)n;
    }

    /* S7 end record */
    {
        u8 cnt = 5; u8 sum = cnt;
        sum = (u8)(sum + (base >> 24) + (base >> 16) + (base >> 8) + base);
        fprintf(fo, "S7%02X%08X%02X\n", cnt, base, (u8)~sum);
    }
    fclose(fi); fclose(fo);
    return 0;
}

/* ---------- Intel HEX -> bin ---------- */
static int ihex_to_bin(const char *in, const char *out) {
    FILE *fi = fopen(in, "r"); if (!fi) { perror(in); return 1; }
    /* two-pass: find min/max addr to size output */
    char line[1024];
    u32 minaddr = 0xFFFFFFFFu, maxaddr = 0;
    u32 upper = 0;
    while (fgets(line, sizeof(line), fi)) {
        if (line[0] != ':') continue;
        int len = hex_val(line[1]) * 16 + hex_val(line[2]);
        u32 addr = (u32)(hex_val(line[3]) * 4096 + hex_val(line[4]) * 256
                      + hex_val(line[5]) * 16 + hex_val(line[6]));
        int type = hex_val(line[7]) * 16 + hex_val(line[8]);
        if (type == 0x04) {
            upper = (u32)(hex_val(line[9]) * 4096 + hex_val(line[10]) * 256
                       + hex_val(line[11]) * 16 + hex_val(line[12]));
            upper <<= 16;
        } else if (type == 0x00) {
            u32 a = upper | addr;
            if (a < minaddr) minaddr = a;
            if (a + (u32)len > maxaddr) maxaddr = a + (u32)len;
        }
    }
    if (minaddr == 0xFFFFFFFFu) { fprintf(stderr, "no data records\n"); fclose(fi); return 1; }

    u32 sz = maxaddr - minaddr;
    u8 *image = (u8*)calloc(sz ? sz : 1, 1);
    rewind(fi);
    upper = 0;
    while (fgets(line, sizeof(line), fi)) {
        if (line[0] != ':') continue;
        int len = hex_val(line[1]) * 16 + hex_val(line[2]);
        u32 addr = (u32)(hex_val(line[3]) * 4096 + hex_val(line[4]) * 256
                      + hex_val(line[5]) * 16 + hex_val(line[6]));
        int type = hex_val(line[7]) * 16 + hex_val(line[8]);
        if (type == 0x04) {
            upper = (u32)(hex_val(line[9]) * 4096 + hex_val(line[10]) * 256
                       + hex_val(line[11]) * 16 + hex_val(line[12]));
            upper <<= 16;
        } else if (type == 0x00) {
            for (int i = 0; i < len; ++i) {
                int hi = hex_val(line[9 + i * 2]);
                int lo = hex_val(line[10 + i * 2]);
                image[(upper | addr) - minaddr + (u32)i] = (u8)((hi << 4) | lo);
            }
        }
    }
    fclose(fi);
    FILE *fo = fopen(out, "wb"); if (!fo) { perror(out); free(image); return 1; }
    fwrite(image, 1, sz, fo);
    fclose(fo);
    free(image);
    fprintf(stderr, "rivet-objcopy: wrote %u bytes (base=0x%08x) to %s\n",
            sz, minaddr, out);
    return 0;
}

int main(int argc, char **argv) {
    const char *mode = NULL;
    int input_mode = 0;
    u32 base = 0;
    const char *in = NULL, *out = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-O") == 0 && i + 1 < argc) { mode = argv[++i]; input_mode = 0; }
        else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) { mode = argv[++i]; input_mode = 1; }
        else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) base = (u32)strtoul(argv[++i], NULL, 0);
        else if (argv[i][0] == '-') {
            fprintf(stderr, "unknown flag: %s\n", argv[i]); return 1;
        } else if (!in) in = argv[i];
        else out = argv[i];
    }
    if (!mode || !in || !out) {
        fprintf(stderr,
            "usage:\n"
            "  rivet-objcopy -O ihex in.bin out.hex [-b base]\n"
            "  rivet-objcopy -O srec in.bin out.s19 [-b base]\n"
            "  rivet-objcopy -I ihex in.hex out.bin\n");
        return 1;
    }
    if (!input_mode && strcmp(mode, "ihex") == 0) return bin_to_ihex(in, out, base);
    if (!input_mode && strcmp(mode, "srec") == 0) return bin_to_srec(in, out, base);
    if ( input_mode && strcmp(mode, "ihex") == 0) return ihex_to_bin(in, out);
    fprintf(stderr, "unsupported mode: %s\n", mode);
    return 1;
}
