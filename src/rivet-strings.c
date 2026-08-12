/* rivet-strings : extract printable ASCII runs from a binary.
 *
 * Walks the file byte-by-byte; emits any run of `min_len` or more
 * printable characters (0x20..0x7E plus tab) terminated by a non-
 * printable byte.
 *
 * Usage: rivet-strings input.bin [-n min_len] [-o output] */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_print(int c) {
    return (c >= 0x20 && c <= 0x7E) || c == '\t';
}

int main(int argc, char **argv) {
    const char *in = NULL, *out = NULL;
    int min_len = 4;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-n") && i + 1 < argc) min_len = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (argv[i][0] == '-') {
            fprintf(stderr, "usage: rivet-strings input.bin [-n min_len] [-o output]\n");
            return 1;
        } else in = argv[i];
    }
    if (!in) {
        fprintf(stderr, "usage: rivet-strings input.bin [-n min_len] [-o output]\n");
        return 1;
    }

    FILE *fi = fopen(in, "rb");
    if (!fi) { perror(in); return 1; }
    FILE *fo = out ? fopen(out, "w") : stdout;
    if (out && !fo) { perror(out); fclose(fi); return 1; }

    char buf[1024];
    int run_len = 0;
    long start_off = 0, off = 0;
    int c;
    while ((c = fgetc(fi)) != EOF) {
        if (is_print(c)) {
            if (run_len == 0) start_off = off;
            if (run_len < (int)sizeof(buf) - 1) buf[run_len++] = (char)c;
        } else {
            if (run_len >= min_len) {
                buf[run_len] = 0;
                fprintf(fo, "%08lx  %s\n", start_off, buf);
            }
            run_len = 0;
        }
        off++;
    }
    if (run_len >= min_len) {
        buf[run_len] = 0;
        fprintf(fo, "%08lx  %s\n", start_off, buf);
    }
    fclose(fi);
    if (out) fclose(fo);
    return 0;
}
