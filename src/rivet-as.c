/* rivet-as : two-pass flat-binary assembler frontend.
 *
 * Architecture-specific instruction encoding is delegated to backends
 * located in src/arch/<arch>.c (riscv32, x86_64). Select via -m <arch>.
 *
 * Usage:
 *   rivet-as [-m riscv32|x86_64] input.s -o output.bin [-b base_addr]
 */

#include "arch/arch.h"

#define MAX_LINE   512
#define MAX_SYM    2048
#define MAX_PROG   (1 << 20)

/* --------------------------- global state ---------------------------- */
static char     prog[MAX_PROG];
riv_u32         riv_as_pc       = 0;
static riv_u32  g_max_pc        = 0;
riv_u32         riv_as_base     = 0;
int             riv_as_pass     = 0;
const char     *riv_as_file     = "<?>";
int             riv_as_line     = 0;

typedef struct { char name[64]; riv_u32 addr; } sym_t;
static sym_t  symtab[MAX_SYM];
static int    nsym = 0;

void riv_as_die(const char *msg, const char *extra) {
    fprintf(stderr, "%s:%d: error: %s%s%s\n", riv_as_file, riv_as_line,
            msg, extra ? " " : "", extra ? extra : "");
    exit(1);
}

void riv_as_emit_u8(riv_u8 b) {
    if (riv_as_pc >= MAX_PROG) riv_as_die("program too large", NULL);
    prog[riv_as_pc++] = (char)b;
    if (riv_as_pc > g_max_pc) g_max_pc = riv_as_pc;
}
void riv_as_emit_u16(riv_u16 v) {
    riv_as_emit_u8((riv_u8)(v & 0xFF));
    riv_as_emit_u8((riv_u8)((v >> 8) & 0xFF));
}
void riv_as_emit_u32(riv_u32 v) {
    riv_as_emit_u8((riv_u8)(v & 0xFF));
    riv_as_emit_u8((riv_u8)((v >> 8)  & 0xFF));
    riv_as_emit_u8((riv_u8)((v >> 16) & 0xFF));
    riv_as_emit_u8((riv_u8)((v >> 24) & 0xFF));
}
void riv_as_emit_u64(riv_u64 v) {
    for (int i = 0; i < 8; ++i)
        riv_as_emit_u8((riv_u8)((v >> (8 * i)) & 0xFF));
}

int riv_as_sym_find(const char *n) {
    for (int i = 0; i < nsym; ++i)
        if (strcmp(symtab[i].name, n) == 0) return i;
    return -1;
}
void riv_as_sym_set(const char *n, riv_u32 v) {
    int i = riv_as_sym_find(n);
    if (i < 0) {
        if (nsym >= MAX_SYM) riv_as_die("symbol table full", NULL);
        i = nsym++;
        strncpy(symtab[i].name, n, sizeof(symtab[i].name) - 1);
        symtab[i].name[sizeof(symtab[i].name) - 1] = 0;
    } else if (riv_as_pass == 0) {
        riv_as_die("duplicate symbol:", n);
    }
    symtab[i].addr = v;
}
riv_u32 riv_as_sym_value(int idx) {
    return (idx >= 0 && idx < nsym) ? symtab[idx].addr : 0;
}

riv_i64 riv_as_parse_imm(const char *s) {
    if (!s || !*s) riv_as_die("missing immediate", NULL);
    if (s[0] == '-' || s[0] == '+' || isdigit((unsigned char)s[0])) {
        char *end;
        long long v = strtoll(s, &end, 0);
        if (*end != 0) riv_as_die("bad number:", s);
        return (riv_i64)v;
    }
    int i = riv_as_sym_find(s);
    if (i < 0) {
        if (riv_as_pass == 0) return 0;
        riv_as_die("unknown symbol:", s);
    }
    return (riv_i64)symtab[i].addr;
}

int riv_as_split_offset(char *tok, char **imm_out, char **reg_out) {
    char *lp = strchr(tok, '(');
    if (!lp) return 0;
    char *rp = strchr(lp, ')');
    if (!rp) riv_as_die("missing ')'", tok);
    *lp = 0; *rp = 0;
    *imm_out = tok;
    *reg_out = lp + 1;
    return 1;
}

/* --------------------------- tokenizer ------------------------------- */
static int tokenize(char *line, char *out[ARCH_MAX_TOKENS]) {
    int n = 0;
    char *p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (!*p || *p == '#' || *p == ';' || *p == '\n' || *p == '\r') break;
        if (n >= ARCH_MAX_TOKENS) riv_as_die("too many tokens", NULL);
        out[n++] = p;
        if (*p == '"') {
            ++p;
            while (*p && *p != '"') { if (*p == '\\' && p[1]) p += 2; else ++p; }
            if (*p == '"') ++p;
        } else if (*p == '[') {
            /* Bracketed memory operand — keep "[...]" as one token, ignoring
             * commas/whitespace inside. */
            int depth = 0;
            do {
                if (*p == '[') ++depth;
                else if (*p == ']') --depth;
                ++p;
            } while (*p && depth > 0);
        } else {
            while (*p && *p != ' ' && *p != '\t' && *p != ','
                   && *p != '#' && *p != ';' && *p != '\n' && *p != '\r') p++;
        }
        if (*p) *p++ = 0;
    }
    return n;
}

static int decode_string(char *tok, char *out, int cap) {
    int n = 0;
    size_t L = strlen(tok);
    if (L < 2 || tok[0] != '"' || tok[L-1] != '"')
        riv_as_die("expected quoted string:", tok);
    for (size_t i = 1; i + 1 < L; ++i) {
        if (n >= cap) riv_as_die("string too long", NULL);
        if (tok[i] != '\\') { out[n++] = tok[i]; continue; }
        ++i;
        if (i + 1 >= L) riv_as_die("trailing backslash", NULL);
        switch (tok[i]) {
        case 'n': out[n++] = '\n'; break;
        case 'r': out[n++] = '\r'; break;
        case 't': out[n++] = '\t'; break;
        case '0': out[n++] = '\0'; break;
        case '\\': out[n++] = '\\'; break;
        case '"': out[n++] = '"';  break;
        case 'x': {
            #define HV(c) ((c)>='0'&&(c)<='9'?(c)-'0': \
                           (c)>='a'&&(c)<='f'?(c)-'a'+10: \
                           (c)>='A'&&(c)<='F'?(c)-'A'+10:-1)
            int h = (i+2 < L) ? HV(tok[i+1]) : -1;
            int l = (i+3 < L) ? HV(tok[i+2]) : -1;
            #undef HV
            if (h < 0 || l < 0) riv_as_die("bad \\x escape", NULL);
            out[n++] = (char)((h << 4) | l);
            i += 2;
        } break;
        default: out[n++] = tok[i]; break;
        }
    }
    return n;
}

/* --------------------------- arch selection -------------------------- */
static const riv_arch_as *g_arch_as = NULL;

static const riv_arch_as *pick_arch(const char *name) {
    if (!name || !strcmp(name, "riscv32") || !strcmp(name, "rv32"))
        return &riv_arch_as_riscv32;
    if (!strcmp(name, "x86_64") || !strcmp(name, "amd64"))
        return &riv_arch_as_x86_64;
    return NULL;
}

/* --------------------------- line processor -------------------------- */
static void process_line(char *line) {
    char *tok[ARCH_MAX_TOKENS];
    int n = tokenize(line, tok);
    if (n == 0) return;

    /* labels */
    while (n > 0) {
        char *t0 = tok[0];
        size_t L = strlen(t0);
        if (L > 1 && t0[L-1] == ':') {
            t0[L-1] = 0;
            if (riv_as_pass == 0) riv_as_sym_set(t0, riv_as_pc + riv_as_base);
            for (int i = 1; i < n; ++i) tok[i-1] = tok[i];
            n--; continue;
        }
        break;
    }
    if (n == 0) return;

    /* directives */
    if (tok[0][0] == '.') {
        const char *d = tok[0];
        if (!strcmp(d, ".text") || !strcmp(d, ".data")) return;
        if (!strcmp(d, ".org")) {
            if (n < 2) riv_as_die(".org needs arg", NULL);
            riv_u32 newpc = (riv_u32)riv_as_parse_imm(tok[1]) - riv_as_base;
            while (riv_as_pc < newpc) riv_as_emit_u8(0);
            riv_as_pc = newpc;
            if (riv_as_pc > g_max_pc) g_max_pc = riv_as_pc;
            return;
        }
        if (!strcmp(d, ".word")) {
            for (int i = 1; i < n; ++i) riv_as_emit_u32((riv_u32)riv_as_parse_imm(tok[i]));
            return;
        }
        if (!strcmp(d, ".quad")) {
            for (int i = 1; i < n; ++i) riv_as_emit_u64((riv_u64)riv_as_parse_imm(tok[i]));
            return;
        }
        if (!strcmp(d, ".byte")) {
            for (int i = 1; i < n; ++i) riv_as_emit_u8((riv_u8)riv_as_parse_imm(tok[i]));
            return;
        }
        if (!strcmp(d, ".half") || !strcmp(d, ".short")) {
            for (int i = 1; i < n; ++i) riv_as_emit_u16((riv_u16)riv_as_parse_imm(tok[i]));
            return;
        }
        if (!strcmp(d, ".zero") || !strcmp(d, ".skip")) {
            if (n < 2) riv_as_die(".zero needs count", NULL);
            riv_u32 cnt = (riv_u32)riv_as_parse_imm(tok[1]);
            riv_u8 fill = (n >= 3) ? (riv_u8)riv_as_parse_imm(tok[2]) : 0;
            for (riv_u32 i = 0; i < cnt; ++i) riv_as_emit_u8(fill);
            return;
        }
        if (!strcmp(d, ".ascii") || !strcmp(d, ".asciz") || !strcmp(d, ".string")) {
            int term = (strcmp(d, ".ascii") != 0);
            if (n < 2) riv_as_die("string directive needs quoted arg", NULL);
            char buf[1024];
            int dn = decode_string(tok[1], buf, (int)sizeof(buf));
            for (int i = 0; i < dn; ++i) riv_as_emit_u8((riv_u8)buf[i]);
            if (term) riv_as_emit_u8(0);
            return;
        }
        if (!strcmp(d, ".align")) {
            if (n < 2) riv_as_die(".align needs arg", NULL);
            riv_u32 a = (riv_u32)riv_as_parse_imm(tok[1]);
            if (a < 1) a = 1;
            while (riv_as_pc & (a - 1)) riv_as_emit_u8(0);
            return;
        }
        if (!strcmp(d, ".p2align")) {
            if (n < 2) riv_as_die(".p2align needs arg", NULL);
            riv_u32 a = 1u << (riv_u32)riv_as_parse_imm(tok[1]);
            while (riv_as_pc & (a - 1)) riv_as_emit_u8(0);
            return;
        }
        if (!strcmp(d, ".global") || !strcmp(d, ".globl")) return;
        if (!strcmp(d, ".equ") || !strcmp(d, ".set")) {
            if (n < 3) riv_as_die("equ needs name,val", NULL);
            if (riv_as_pass == 0)
                riv_as_sym_set(tok[1], (riv_u32)riv_as_parse_imm(tok[2]));
            return;
        }
        if (!strcmp(d, ".arch")) {
            if (n < 2) riv_as_die(".arch needs name", NULL);
            const riv_arch_as *a = pick_arch(tok[1]);
            if (!a) riv_as_die("unknown arch:", tok[1]);
            g_arch_as = a;
            return;
        }
        riv_as_die("unknown directive:", d);
    }

    /* instruction → arch backend */
    if (!g_arch_as) riv_as_die("no arch selected (use -m or .arch)", NULL);
    if (!g_arch_as->encode_line(tok, n))
        riv_as_die("unknown mnemonic for arch:", tok[0]);
}

static void assemble_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); exit(1); }
    char line[MAX_LINE];
    riv_as_file = path;
    riv_as_line = 0;
    riv_as_pc = 0;
    while (fgets(line, sizeof(line), f)) {
        riv_as_line++;
        process_line(line);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    const char *in = NULL, *out = "a.bin";
    const char *march = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "-b") && i + 1 < argc)
            riv_as_base = (riv_u32)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "-m") && i + 1 < argc) march = argv[++i];
        else if (argv[i][0] == '-') {
            fprintf(stderr, "unknown flag: %s\n", argv[i]);
            return 1;
        } else in = argv[i];
    }
    if (!in) {
        fprintf(stderr,
            "usage: rivet-as [-m riscv32|x86_64] input.s -o output.bin [-b base]\n");
        return 1;
    }
    g_arch_as = pick_arch(march);
    if (march && !g_arch_as) {
        fprintf(stderr, "unknown arch: %s\n", march);
        return 1;
    }
    if (!g_arch_as) g_arch_as = &riv_arch_as_riscv32;   /* default */

    riv_as_pass = 0; assemble_file(in);
    riv_as_pass = 1; assemble_file(in);

    FILE *fo = fopen(out, "wb");
    if (!fo) { perror(out); return 1; }
    fwrite(prog, 1, g_max_pc, fo);
    fclose(fo);

    fprintf(stderr, "rivet-as[%s]: wrote %u bytes to %s (base=0x%08x, syms=%d)\n",
            g_arch_as->name, (unsigned)g_max_pc, out,
            (unsigned)riv_as_base, nsym);
    return 0;
}
