/* rivet-img : image post-processor.
 *
 * Pads a binary to a target size with a fill byte and optionally appends
 * a trailer containing a CRC-32 checksum (little-endian) for the
 * preceding region. Useful for bootloader-verified firmware images.
 *
 * Usage:
 *   rivet-img in.bin -o out.bin --pad SIZE [--fill 0xFF] [--crc32-trailer]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char  u8;
typedef unsigned int   u32;

static u32 crc32_buf(const u8 *p, size_t n) {
    u32 c = ~0u;
    while (n--) {
        c ^= *p++;
        for (int i = 0; i < 8; ++i)
            c = (c >> 1) ^ (0xEDB88320u & -(c & 1));
    }
    return ~c;
}

static u32 parse_size(const char *s) {
    char *e;
    unsigned long v = strtoul(s, &e, 0);
    if (*e == 'K' || *e == 'k') v *= 1024;
    else if (*e == 'M' || *e == 'm') v *= 1024UL * 1024UL;
    return (u32)v;
}

int main(int argc, char **argv) {
    const char *in = NULL, *out = "image.out";
    u32 pad = 0, fill = 0xFF;
    int add_crc = 0;
    int have_pad = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out = argv[++i];
        else if (strcmp(argv[i], "--pad") == 0 && i + 1 < argc) {
            pad = parse_size(argv[++i]); have_pad = 1;
        }
        else if (strcmp(argv[i], "--fill") == 0 && i + 1 < argc)
            fill = (u32)strtoul(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--crc32-trailer") == 0) add_crc = 1;
        else if (argv[i][0] == '-') {
            fprintf(stderr, "unknown flag: %s\n", argv[i]); return 1;
        } else in = argv[i];
    }
    if (!in) {
        fprintf(stderr,
            "usage: rivet-img in.bin -o out.bin --pad SIZE [--fill BYTE] [--crc32-trailer]\n");
        return 1;
    }

    FILE *fi = fopen(in, "rb"); if (!fi) { perror(in); return 1; }
    fseek(fi, 0, SEEK_END); long sz = ftell(fi); fseek(fi, 0, SEEK_SET);
    u32 final = have_pad ? pad : (u32)sz;
    if (add_crc && final < (u32)sz + 4) final = (u32)sz + 4;
    if (final < (u32)sz) {
        fprintf(stderr, "--pad %u smaller than input (%ld bytes)\n", pad, sz);
        fclose(fi); return 1;
    }

    u8 *image = (u8*)malloc(final);
    if (!image) { perror("malloc"); fclose(fi); return 1; }
    memset(image, (int)fill, final);
    if (fread(image, 1, (size_t)sz, fi) != (size_t)sz) {
        perror(in); fclose(fi); free(image); return 1;
    }
    fclose(fi);

    if (add_crc) {
        u32 crc = crc32_buf(image, final - 4);
        image[final - 4] = (u8)(crc      );
        image[final - 3] = (u8)(crc >>  8);
        image[final - 2] = (u8)(crc >> 16);
        image[final - 1] = (u8)(crc >> 24);
        fprintf(stderr, "rivet-img: CRC32 = 0x%08x (over %u bytes)\n",
                crc, final - 4);
    }

    FILE *fo = fopen(out, "wb"); if (!fo) { perror(out); free(image); return 1; }
    fwrite(image, 1, final, fo);
    fclose(fo);
    free(image);
    fprintf(stderr, "rivet-img: wrote %u bytes to %s\n", final, out);
    return 0;
}
