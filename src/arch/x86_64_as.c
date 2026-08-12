/* RIVET arch: x86_64 (subset).
 * Intel-syntax assembler + disassembler for a curated subset suitable for
 * bootloaders, microkernels, and small firmware blobs.
 *
 * Supported (assembler + disassembler):
 *   Data:        mov reg,reg | mov reg,imm
 *   Arithmetic:  add sub and or xor cmp test  (reg,reg and reg,imm32)
 *                inc dec neg not imul (reg,reg)
 *                shl shr sar (reg,imm8)
 *   Stack:       push pop  (reg)
 *   Control:     jmp call (rel32),
 *                jcc rel32 (je jne jg jge jl jle ja jae jb jbe js jns jo jno)
 *                ret
 *   System:      nop hlt int3 ud2 cli sti syscall  int imm8
 *   I/O:         in/out  (al,dx and al,imm8)
 *
 * All 16 GPRs (rax rcx rdx rbx rsp rbp rsi rdi r8-r15). 64-bit operand
 * size only (REX.W=1 where required). */

#include "arch.h"

/* --------------------------- registers ------------------------------- */
static const char *x86_regs[16] = {
    "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
    "r8", "r9", "r10","r11","r12","r13","r14","r15"
};

static int x86_reg_lookup(const char *s) {
    for (int i = 0; i < 16; ++i) if (strcmp(s, x86_regs[i]) == 0) return i;
    return -1;
}
static int x86_parse_reg(const char *s) {
    int r = x86_reg_lookup(s);
    if (r < 0) riv_as_die("bad x86_64 register:", s);
    return r;
}
static int x86_is_reg(const char *s) { return x86_reg_lookup(s) >= 0; }

/* --------------------------- emit helpers ---------------------------- */
static void x86_emit_imm32(riv_i32 v) {
    riv_as_emit_u8((riv_u8)( v        & 0xFF));
    riv_as_emit_u8((riv_u8)((v >>  8) & 0xFF));
    riv_as_emit_u8((riv_u8)((v >> 16) & 0xFF));
    riv_as_emit_u8((riv_u8)((v >> 24) & 0xFF));
}
static void x86_emit_imm64(riv_i64 v) {
    for (int i = 0; i < 8; ++i)
        riv_as_emit_u8((riv_u8)((v >> (8 * i)) & 0xFF));
}
static void x86_emit_rex(int w, int r, int x, int b) {
    if (w || r || x || b)
        riv_as_emit_u8((riv_u8)(0x40 | (w << 3) | (r << 2) | (x << 1) | b));
}
static void x86_emit_modrm(int mod, int reg_or_digit, int rm) {
    riv_as_emit_u8((riv_u8)((mod << 6) | ((reg_or_digit & 7) << 3) | (rm & 7)));
}

/* REX.W + opcode + ModR/M(11, src, dst) for two-register MR-form ops. */
static void x86_emit_rr_mr(int dst, int src, riv_u8 opcode) {
    x86_emit_rex(1, (src >> 3) & 1, 0, (dst >> 3) & 1);
    riv_as_emit_u8(opcode);
    x86_emit_modrm(3, src, dst);
}
/* REX.W + 0x81 /digit + imm32 — register-immediate arithmetic. */
static void x86_emit_ri32(int dst, int digit, riv_i32 imm) {
    x86_emit_rex(1, 0, 0, (dst >> 3) & 1);
    riv_as_emit_u8(0x81);
    x86_emit_modrm(3, digit, dst);
    x86_emit_imm32(imm);
}
/* REX.W + 0xC1 /digit + imm8 — register-immediate shift. */
static void x86_emit_shift(int dst, int digit, int imm8) {
    x86_emit_rex(1, 0, 0, (dst >> 3) & 1);
    riv_as_emit_u8(0xC1);
    x86_emit_modrm(3, digit, dst);
    riv_as_emit_u8((riv_u8)(imm8 & 0xFF));
}
/* REX.W + 0xF7 /digit — unary on r/m64. */
static void x86_emit_unary_f7(int dst, int digit) {
    x86_emit_rex(1, 0, 0, (dst >> 3) & 1);
    riv_as_emit_u8(0xF7);
    x86_emit_modrm(3, digit, dst);
}
/* REX.W + 0xFF /digit — unary r/m64 (inc/dec). */
static void x86_emit_unary_ff(int dst, int digit) {
    x86_emit_rex(1, 0, 0, (dst >> 3) & 1);
    riv_as_emit_u8(0xFF);
    x86_emit_modrm(3, digit, dst);
}

/* ----------------------- memory operand support ---------------------- */
typedef struct {
    int     base;        /* register index, or -1 */
    int     index;       /* register index, or -1 */
    int     scale;       /* 1, 2, 4, or 8 */
    int     has_disp;
    riv_i32 disp;
} x86_mem;

static int x86_is_mem(const char *s) { return s[0] == '['; }

/* Parse "[base [+/- index*scale] [+/- disp]]". Mutates a local copy. */
static x86_mem x86_parse_mem(const char *tok) {
    x86_mem m = { -1, -1, 1, 0, 0 };
    char buf[128];
    size_t L = strlen(tok);
    if (L < 2 || tok[0] != '[' || tok[L-1] != ']')
        riv_as_die("bad mem operand:", tok);
    if (L - 2 >= sizeof(buf)) riv_as_die("mem operand too long", NULL);
    memcpy(buf, tok + 1, L - 2);
    buf[L - 2] = 0;

    /* split into signed atoms */
    char *atoms[8]; int signs[8]; int n_atoms = 0;
    int sign = 1;
    char *p = buf;
    while (*p) {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) break;
        char *start = p;
        while (*p && *p != '+' && *p != '-') ++p;
        char saved = *p;
        char *end = p;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) --end;
        *end = 0;
        if (*start) {
            if (n_atoms >= 8) riv_as_die("too many mem atoms", NULL);
            atoms[n_atoms] = start;
            signs[n_atoms] = sign;
            ++n_atoms;
        }
        if (saved == 0) break;
        sign = (saved == '-') ? -1 : 1;
        *p++ = 0;
    }

    for (int i = 0; i < n_atoms; ++i) {
        char *a = atoms[i];
        char *star = strchr(a, '*');
        if (star) {
            *star = 0;
            char *l = a;       while (*l == ' ' || *l == '\t') ++l;
            char *r = star + 1; while (*r == ' ' || *r == '\t') ++r;
            int reg = x86_reg_lookup(l);
            int sc;
            if (reg < 0) {
                reg = x86_reg_lookup(r);
                if (reg < 0) riv_as_die("bad index reg:", a);
                sc = (int)riv_as_parse_imm(l);
            } else {
                sc = (int)riv_as_parse_imm(r);
            }
            if (sc != 1 && sc != 2 && sc != 4 && sc != 8)
                riv_as_die("bad scale (1/2/4/8):", a);
            m.index = reg;
            m.scale = sc;
        } else if (x86_is_reg(a)) {
            int reg = x86_reg_lookup(a);
            if (m.base < 0) m.base = reg;
            else { m.index = reg; m.scale = 1; }
        } else {
            riv_i64 v = riv_as_parse_imm(a);
            m.disp += (riv_i32)(v * signs[i]);
            m.has_disp = 1;
        }
    }
    return m;
}

/* Emit REX byte for an op whose memory operand may include high regs. */
static void x86_emit_rex_for_mem(int w, int reg_field, const x86_mem *m) {
    int r = (reg_field >> 3) & 1;
    int x = (m->index >= 0) ? ((m->index >> 3) & 1) : 0;
    int b = (m->base  >= 0) ? ((m->base  >> 3) & 1) : 0;
    x86_emit_rex(w, r, x, b);
}

/* Emit ModR/M (+ SIB + disp) for a memory operand encoded into r/m. */
static void x86_emit_mem(int reg_field, const x86_mem *m) {
    int base = m->base, index = m->index;
    int has_sib = (index >= 0) || (base >= 0 && (base & 7) == 4);
    int has_disp = m->has_disp;
    /* [rbp] / [r13] with no disp must be encoded as disp8=0 (rm=101 means
     * RIP-rel under mod=00). */
    int force_zero_disp = (base >= 0 && (base & 7) == 5 && !has_disp);
    if (force_zero_disp) has_disp = 1;

    int mod;
    if (!has_disp) mod = 0;
    else if (m->disp >= -128 && m->disp <= 127) mod = 1;
    else mod = 2;

    int rm = has_sib ? 4 : (base & 7);
    x86_emit_modrm(mod, reg_field, rm);

    if (has_sib) {
        int sc = m->scale == 1 ? 0 : m->scale == 2 ? 1 : m->scale == 4 ? 2 : 3;
        int sib_index = (index >= 0) ? (index & 7) : 4;     /* 4 == no index */
        int sib_base  = (base  >= 0) ? (base  & 7) : 5;
        riv_as_emit_u8((riv_u8)((sc << 6) | (sib_index << 3) | sib_base));
        /* SIB with no base + mod=00 demands disp32. */
        if (mod == 0 && base < 0) {
            x86_emit_imm32(m->disp);
            return;
        }
    }
    if (mod == 1) riv_as_emit_u8((riv_u8)(m->disp & 0xFF));
    else if (mod == 2) x86_emit_imm32(m->disp);
}

/* Generic encoder for `op  dst, src` where one side may be memory. */
static int x86_emit_rm_or_mr(const char *m_name, int opcode_mr, int opcode_rm,
                             char *dst_tok, char *src_tok) {
    int src_is_mem = x86_is_mem(src_tok);
    int dst_is_mem = x86_is_mem(dst_tok);
    if (src_is_mem && dst_is_mem) {
        riv_as_die("memory-to-memory not allowed:", m_name);
    }
    if (!src_is_mem && !dst_is_mem) {
        int dst = x86_parse_reg(dst_tok), src = x86_parse_reg(src_tok);
        x86_emit_rr_mr(dst, src, (riv_u8)opcode_mr);
        return 1;
    }
    if (src_is_mem) {
        /* RM form: reg = dst, mem = src */
        int dst = x86_parse_reg(dst_tok);
        x86_mem mem = x86_parse_mem(src_tok);
        x86_emit_rex_for_mem(1, dst, &mem);
        riv_as_emit_u8((riv_u8)opcode_rm);
        x86_emit_mem(dst, &mem);
        return 1;
    }
    /* dst is mem — MR form */
    int src = x86_parse_reg(src_tok);
    x86_mem mem = x86_parse_mem(dst_tok);
    x86_emit_rex_for_mem(1, src, &mem);
    riv_as_emit_u8((riv_u8)opcode_mr);
    x86_emit_mem(src, &mem);
    return 1;
}

/* --------------------------- jcc table ------------------------------- */
static int x86_jcc_opcode(const char *m) {
    /* the rel32 form uses opcode 0x0F 0x80+cc */
    if (!strcmp(m,"jo")  || !strcmp(m,"jno")) return !strcmp(m,"jo")  ? 0x80 : 0x81;
    if (!strcmp(m,"jb")  || !strcmp(m,"jc"))  return 0x82;
    if (!strcmp(m,"jae") || !strcmp(m,"jnc")) return 0x83;
    if (!strcmp(m,"je")  || !strcmp(m,"jz"))  return 0x84;
    if (!strcmp(m,"jne") || !strcmp(m,"jnz")) return 0x85;
    if (!strcmp(m,"jbe") || !strcmp(m,"jna")) return 0x86;
    if (!strcmp(m,"ja")  || !strcmp(m,"jnbe"))return 0x87;
    if (!strcmp(m,"js"))  return 0x88;
    if (!strcmp(m,"jns")) return 0x89;
    if (!strcmp(m,"jp"))  return 0x8A;
    if (!strcmp(m,"jnp")) return 0x8B;
    if (!strcmp(m,"jl"))  return 0x8C;
    if (!strcmp(m,"jge")) return 0x8D;
    if (!strcmp(m,"jle")) return 0x8E;
    if (!strcmp(m,"jg"))  return 0x8F;
    return -1;
}

/* Op-with-imm digit lookup for 0x81 family. */
static int x86_arith_digit(const char *m) {
    if (!strcmp(m, "add")) return 0;
    if (!strcmp(m, "or"))  return 1;
    if (!strcmp(m, "and")) return 4;
    if (!strcmp(m, "sub")) return 5;
    if (!strcmp(m, "xor")) return 6;
    if (!strcmp(m, "cmp")) return 7;
    return -1;
}
/* Op-with-reg primary opcode (MR form, /r). */
static int x86_arith_mr_opcode(const char *m) {
    if (!strcmp(m, "add")) return 0x01;
    if (!strcmp(m, "or"))  return 0x09;
    if (!strcmp(m, "and")) return 0x21;
    if (!strcmp(m, "sub")) return 0x29;
    if (!strcmp(m, "xor")) return 0x31;
    if (!strcmp(m, "cmp")) return 0x39;
    if (!strcmp(m, "test"))return 0x85;
    return -1;
}

/* --------------------------- encoder --------------------------------- */
static int x86_encode(char **tok, int nt) {
    const char *m = tok[0];

    /* zero-operand */
    if (!strcmp(m, "nop"))     { riv_as_emit_u8(0x90); return 1; }
    if (!strcmp(m, "hlt"))     { riv_as_emit_u8(0xF4); return 1; }
    if (!strcmp(m, "int3"))    { riv_as_emit_u8(0xCC); return 1; }
    if (!strcmp(m, "cli"))     { riv_as_emit_u8(0xFA); return 1; }
    if (!strcmp(m, "sti"))     { riv_as_emit_u8(0xFB); return 1; }
    if (!strcmp(m, "ret"))     { riv_as_emit_u8(0xC3); return 1; }
    if (!strcmp(m, "syscall")) { riv_as_emit_u8(0x0F); riv_as_emit_u8(0x05); return 1; }
    if (!strcmp(m, "sysret"))  { riv_as_emit_u8(0x48); riv_as_emit_u8(0x0F); riv_as_emit_u8(0x07); return 1; }
    if (!strcmp(m, "ud2"))     { riv_as_emit_u8(0x0F); riv_as_emit_u8(0x0B); return 1; }
    if (!strcmp(m, "leave"))   { riv_as_emit_u8(0xC9); return 1; }
    if (!strcmp(m, "iretq"))   { riv_as_emit_u8(0x48); riv_as_emit_u8(0xCF); return 1; }

    /* int imm8 */
    if (!strcmp(m, "int")) {
        if (nt < 2) riv_as_die("int needs imm8", NULL);
        riv_as_emit_u8(0xCD);
        riv_as_emit_u8((riv_u8)(riv_as_parse_imm(tok[1]) & 0xFF));
        return 1;
    }

    /* push reg / pop reg */
    if (!strcmp(m, "push") || !strcmp(m, "pop")) {
        if (nt < 2) riv_as_die("needs register", m);
        int r = x86_parse_reg(tok[1]);
        if (r >= 8) riv_as_emit_u8(0x41); /* REX.B */
        riv_as_emit_u8((riv_u8)(((m[1] == 'u') ? 0x50 : 0x58) + (r & 7)));
        return 1;
    }

    /* mov reg, (reg | imm | [mem])  /  mov [mem], (reg | imm32) */
    if (!strcmp(m, "mov")) {
        if (nt < 3) riv_as_die("mov needs 2 operands", NULL);
        /* memory on either side */
        if (x86_is_mem(tok[1]) || x86_is_mem(tok[2])) {
            /* mov [mem], imm32 */
            if (x86_is_mem(tok[1]) && !x86_is_reg(tok[2]) && !x86_is_mem(tok[2])) {
                x86_mem mem = x86_parse_mem(tok[1]);
                riv_i64 imm = riv_as_parse_imm(tok[2]);
                x86_emit_rex_for_mem(1, 0, &mem);
                riv_as_emit_u8(0xC7);
                x86_emit_mem(0, &mem);
                x86_emit_imm32((riv_i32)imm);
                return 1;
            }
            return x86_emit_rm_or_mr("mov", 0x89, 0x8B, tok[1], tok[2]);
        }
        int dst = x86_parse_reg(tok[1]);
        if (x86_is_reg(tok[2])) {
            int src = x86_parse_reg(tok[2]);
            x86_emit_rr_mr(dst, src, 0x89);
        } else {
            riv_i64 imm = riv_as_parse_imm(tok[2]);
            /* fits in signed 32? use C7 /0 + imm32 (7 bytes) */
            if (imm >= -0x80000000ll && imm <= 0x7FFFFFFFll) {
                x86_emit_rex(1, 0, 0, (dst >> 3) & 1);
                riv_as_emit_u8(0xC7);
                x86_emit_modrm(3, 0, dst);
                x86_emit_imm32((riv_i32)imm);
            } else {
                /* 10-byte REX.W + B8+rd + imm64 */
                x86_emit_rex(1, 0, 0, (dst >> 3) & 1);
                riv_as_emit_u8((riv_u8)(0xB8 + (dst & 7)));
                x86_emit_imm64(imm);
            }
        }
        return 1;
    }

    /* xchg reg, reg */
    if (!strcmp(m, "xchg")) {
        if (nt < 3) riv_as_die("xchg needs 2 regs", NULL);
        int dst = x86_parse_reg(tok[1]);
        int src = x86_parse_reg(tok[2]);
        x86_emit_rr_mr(dst, src, 0x87);
        return 1;
    }

    /* add/sub/and/or/xor/cmp/test  with reg/reg, reg/imm, or reg/mem, mem/reg */
    {
        int mr = x86_arith_mr_opcode(m);
        int dg = x86_arith_digit(m);
        if (mr >= 0) {
            if (nt < 3) riv_as_die("needs 2 operands", m);
            /* memory on either side */
            if (x86_is_mem(tok[1]) || x86_is_mem(tok[2])) {
                static const struct { const char *n; int mr, rm; } op_rm[] = {
                    {"add",0x01,0x03}, {"sub",0x29,0x2B}, {"and",0x21,0x23},
                    {"or", 0x09,0x0B}, {"xor",0x31,0x33}, {"cmp",0x39,0x3B},
                    {"test",0x85,0x85},
                };
                for (unsigned i = 0; i < sizeof(op_rm)/sizeof(op_rm[0]); ++i)
                    if (!strcmp(op_rm[i].n, m))
                        return x86_emit_rm_or_mr(m, op_rm[i].mr, op_rm[i].rm,
                                                  tok[1], tok[2]);
                riv_as_die("mem not supported for", m);
            }
            int dst = x86_parse_reg(tok[1]);
            if (x86_is_reg(tok[2])) {
                int src = x86_parse_reg(tok[2]);
                x86_emit_rr_mr(dst, src, (riv_u8)mr);
                return 1;
            }
            if (dg < 0) riv_as_die("only reg/reg supported for", m);
            x86_emit_ri32(dst, dg, (riv_i32)riv_as_parse_imm(tok[2]));
            return 1;
        }
    }

    /* lea reg, [mem]  (8D /r) */
    if (!strcmp(m, "lea")) {
        if (nt < 3) riv_as_die("lea reg, [mem]", NULL);
        int dst = x86_parse_reg(tok[1]);
        if (!x86_is_mem(tok[2])) riv_as_die("lea: second op must be [mem]", NULL);
        x86_mem mem = x86_parse_mem(tok[2]);
        x86_emit_rex_for_mem(1, dst, &mem);
        riv_as_emit_u8(0x8D);
        x86_emit_mem(dst, &mem);
        return 1;
    }

    /* imul reg, reg  (0F AF /r) */
    if (!strcmp(m, "imul")) {
        if (nt < 3) riv_as_die("imul reg, reg only", NULL);
        int dst = x86_parse_reg(tok[1]);
        int src = x86_parse_reg(tok[2]);
        x86_emit_rex(1, (dst >> 3) & 1, 0, (src >> 3) & 1);
        riv_as_emit_u8(0x0F);
        riv_as_emit_u8(0xAF);
        x86_emit_modrm(3, dst, src);
        return 1;
    }

    /* inc / dec / neg / not  reg */
    if (!strcmp(m, "inc") || !strcmp(m, "dec")) {
        if (nt < 2) riv_as_die("needs register", m);
        x86_emit_unary_ff(x86_parse_reg(tok[1]), m[0] == 'i' ? 0 : 1);
        return 1;
    }
    if (!strcmp(m, "neg") || !strcmp(m, "not")) {
        if (nt < 2) riv_as_die("needs register", m);
        x86_emit_unary_f7(x86_parse_reg(tok[1]), m[0] == 'n' ? (m[1] == 'e' ? 3 : 2) : 2);
        return 1;
    }
    if (!strcmp(m, "mul") || !strcmp(m, "div") || !strcmp(m, "idiv")) {
        if (nt < 2) riv_as_die("needs register", m);
        int digit = !strcmp(m, "mul") ? 4 : !strcmp(m, "div") ? 6 : 7;
        x86_emit_unary_f7(x86_parse_reg(tok[1]), digit);
        return 1;
    }

    /* shl / shr / sar reg, imm8 */
    if (!strcmp(m, "shl") || !strcmp(m, "shr") || !strcmp(m, "sar")) {
        if (nt < 3) riv_as_die("needs reg, imm8", m);
        int digit = !strcmp(m, "shl") ? 4 : !strcmp(m, "shr") ? 5 : 7;
        x86_emit_shift(x86_parse_reg(tok[1]), digit,
                       (int)riv_as_parse_imm(tok[2]));
        return 1;
    }

    /* jmp / call rel32 */
    if (!strcmp(m, "jmp") || !strcmp(m, "call")) {
        if (nt < 2) riv_as_die("needs target", m);
        riv_i64 tgt = riv_as_parse_imm(tok[1]);
        riv_i64 next_pc = (riv_i64)(riv_as_pc + riv_as_base + 5);
        riv_i32 off = (riv_i32)(tgt - next_pc);
        riv_as_emit_u8(!strcmp(m, "jmp") ? 0xE9 : 0xE8);
        x86_emit_imm32(off);
        return 1;
    }

    /* jcc rel32 */
    {
        int cc = x86_jcc_opcode(m);
        if (cc >= 0) {
            if (nt < 2) riv_as_die("jcc needs target", m);
            riv_i64 tgt = riv_as_parse_imm(tok[1]);
            riv_i64 next_pc = (riv_i64)(riv_as_pc + riv_as_base + 6);
            riv_i32 off = (riv_i32)(tgt - next_pc);
            riv_as_emit_u8(0x0F);
            riv_as_emit_u8((riv_u8)cc);
            x86_emit_imm32(off);
            return 1;
        }
    }

    /* in al, dx  /  in al, imm8  /  out dx, al  /  out imm8, al */
    if (!strcmp(m, "in")) {
        if (nt < 3 || strcmp(tok[1], "al") != 0) riv_as_die("in al, ...", NULL);
        if (!strcmp(tok[2], "dx")) { riv_as_emit_u8(0xEC); return 1; }
        riv_as_emit_u8(0xE4);
        riv_as_emit_u8((riv_u8)riv_as_parse_imm(tok[2]));
        return 1;
    }
    if (!strcmp(m, "out")) {
        if (nt < 3 || strcmp(tok[2], "al") != 0) riv_as_die("out ..., al", NULL);
        if (!strcmp(tok[1], "dx")) { riv_as_emit_u8(0xEE); return 1; }
        riv_as_emit_u8(0xE6);
        riv_as_emit_u8((riv_u8)riv_as_parse_imm(tok[1]));
        return 1;
    }

    return 0;   /* mnemonic not recognized */
}

const riv_arch_as riv_arch_as_x86_64 = { "x86_64", x86_encode };

