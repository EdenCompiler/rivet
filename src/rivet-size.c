/* rivet-size : report binary size with a quick byte-distribution
 * histogram.
 *
 * Output:
 *   total bytes
 *   non-zero bytes count + ratio
 *   first/last non-zero offset
 *   distribution of byte value classes (zero / printable / control / high)
 *
 * Useful for sanity-checking firmware images.
 *
 * Usage: rivet-size input.bin [input2.bin ...] */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void report(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return; }
    long zeros = 0, ff = 0, prn = 0, ctrl = 0, hi = 0, total = 0;
    long first_nz = -1, last_nz = -1;
    int c;
    long off = 0;
    while ((c = fgetc(f)) != EOF) {
        if (c == 0)        zeros++;
        else if (c == 0xFF) ff++;
        else if (c >= 0x20 && c <= 0x7E) prn++;
        else if (c < 0x20) ctrl++;
        else                hi++;
        if (c != 0) {
            if (first_nz < 0) first_nz = off;
            last_nz = off;
        }
        total++;
        off++;
    }
    fclose(f);

    long nz = total - zeros;
    printf("== %s\n", path);
    printf("  total            %ld\n", total);
    printf("  non-zero         %ld (%.1f%%)\n",
           nz, total ? 100.0 * nz / total : 0.0);
    printf("  zeros            %ld\n", zeros);
    printf("  0xFF             %ld\n", ff);
    printf("  printable ASCII  %ld\n", prn);
    printf("  other control    %ld\n", ctrl);
    printf("  high bytes       %ld\n", hi);
    if (first_nz >= 0)
        printf("  first non-zero   0x%lx\n  last  non-zero   0x%lx\n",
               first_nz, last_nz);
    printf("\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: rivet-size input.bin [input2.bin ...]\n");
        return 1;
    }
    for (int i = 1; i < argc; ++i) report(argv[i]);
    return 0;
}
