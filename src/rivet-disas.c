/* rivet-disas : architecture-pluggable disassembler frontend.
 *
 * Reads a flat binary, dispatches each instruction to the chosen
 * architecture's decoder (located in src/arch/<arch>.c).
 *
 * Usage:
 *   rivet-disas [-m riscv32|x86_64] in.bin [-b base_addr]
 */

#include "arch/arch.h"

static const riv_arch_disas *pick_arch(const char *name) {
    if (!name || !strcmp(name, "riscv32") || !strcmp(name, "rv32"))
        return &riv_disas_riscv32;
    if (!strcmp(name, "x86_64") || !strcmp(name, "amd64"))
        return &riv_disas_x86_64;
    return NULL;
}

int main(int argc, char **argv) {
    const char *in = NULL, *march = NULL;
    riv_u32 base = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-b") && i + 1 < argc)
            base = (riv_u32)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "-m") && i + 1 < argc)
            march = argv[++i];
        else if (argv[i][0] == '-') {
            fprintf(stderr,
                "usage: rivet-disas [-m riscv32|x86_64] input.bin [-b base]\n");
            return 1;
        } else in = argv[i];
    }
    if (!in) {
        fprintf(stderr,
            "usage: rivet-disas [-m riscv32|x86_64] input.bin [-b base]\n");
        return 1;
    }
    const riv_arch_disas *arch = pick_arch(march);
    if (march && !arch) {
        fprintf(stderr, "unknown arch: %s\n", march);
        return 1;
    }
    if (!arch) arch = &riv_disas_riscv32;

    FILE *f = fopen(in, "rb");
    if (!f) { perror(in); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    riv_u8 *buf = (riv_u8*)malloc(sz ? (size_t)sz : 1);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        perror(in); fclose(f); free(buf); return 1;
    }
    fclose(f);

    riv_u32 pc = base;
    long i = 0;
    while (i < sz) {
        char line[128];
        int n = arch->decode(pc, buf + i, (int)(sz - i), line, sizeof(line));
        if (n <= 0) {
            /* fall back: print a raw byte and advance one */
            printf("%08x:  %02x%*s   .byte  0x%02x\n",
                   pc, buf[i], 9, "", buf[i]);
            ++i; ++pc;
            continue;
        }
        printf("%08x:  ", pc);
        /* print hex of decoded bytes — up to 6 columns */
        int hex_cols = n < 6 ? n : 6;
        for (int k = 0; k < hex_cols; ++k) printf("%02x", buf[i + k]);
        for (int k = hex_cols; k < 6; ++k) printf("  ");
        printf("   %s\n", line);
        i  += n;
        pc += (riv_u32)n;
    }
    free(buf);
    return 0;
}
