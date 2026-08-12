/* RIVET arch: RISC-V 32-bit (RV32I + M extension).
 * Assembler encoder + flat-binary disassembler. */

#include "arch.h"

/* --------------------------- registers ------------------------------- */
static const char *rv_abi[32] = {
    "zero","ra","sp","gp","tp","t0","t1","t2","s0","s1",
    "a0","a1","a2","a3","a4","a5","a6","a7",
    "s2","s3","s4","s5","s6","s7","s8","s9","s10","s11",
    "t3","t4","t5","t6"
};

static int rv_reg_lookup(const char *s) {
    if ((s[0] == 'x' || s[0] == 'X') && isdigit((unsigned char)s[1])) {
        int n = atoi(s + 1);
        if (n >= 0 && n < 32) return n;
    }
    if (strcmp(s, "fp") == 0) return 8;
    for (int i = 0; i < 32; ++i) if (strcmp(s, rv_abi[i]) == 0) return i;
    return -1;
}
static int rv_parse_reg(const char *s) {
    int r = rv_reg_lookup(s);
    if (r < 0) riv_as_die("bad RV register:", s);
    return r;
}

/* --------------------------- encoders --------------------------------
 *
 * One encoder per RV32 instruction format. Field bit positions follow
 * the unprivileged ISA manual; the B and J immediates are deliberately
 * non-contiguous in the encoding (hardware shortcut), so bit
 * scrambling is mandatory.
 */

/* R-type: funct7[31:25] rs2[24:20] rs1[19:15] funct3[14:12] rd[11:7] op[6:0] */
static riv_u32 enc_r(int rd, int f3, int rs1, int rs2, int f7, int op) {
    return  ((riv_u32)f7  << 25)
          | ((riv_u32)rs2 << 20)
          | ((riv_u32)rs1 << 15)
          | ((riv_u32)f3  << 12)
          | ((riv_u32)rd  <<  7)
          |  (riv_u32)op;
}

/* I-type: imm[31:20] rs1[19:15] funct3[14:12] rd[11:7] op[6:0] */
static riv_u32 enc_i(int rd, int f3, int rs1, riv_i32 imm, int op) {
    return  (((riv_u32)imm & 0xFFF) << 20)
          | ((riv_u32)rs1 << 15)
          | ((riv_u32)f3  << 12)
          | ((riv_u32)rd  <<  7)
          |  (riv_u32)op;
}

/* I-type shift: shamt sits in imm[24:20], funct7 in [31:25]. */
static riv_u32 enc_shift(int rd, int f3, int rs1, int shamt, int f7, int op) {
    return  ((riv_u32)f7 << 25)
          | ((riv_u32)(shamt & 0x1F) << 20)
          | ((riv_u32)rs1 << 15)
          | ((riv_u32)f3  << 12)
          | ((riv_u32)rd  <<  7)
          |  (riv_u32)op;
}

/* S-type: imm split into [31:25] and [11:7]. */
static riv_u32 enc_s(int rs2, int f3, int rs1, riv_i32 imm, int op) {
    riv_u32 i = (riv_u32)imm & 0xFFF;
    return  ((i >> 5)       << 25)
          | ((riv_u32)rs2   << 20)
          | ((riv_u32)rs1   << 15)
          | ((riv_u32)f3    << 12)
          | ((i & 0x1F)     <<  7)
          |  (riv_u32)op;
}

/* B-type: imm[12|10:5|4:1|11], LSB implicit zero. */
static riv_u32 enc_b(int rs1, int rs2, int f3, riv_i32 off, int op) {
    riv_u32 i = (riv_u32)off & 0x1FFE;
    return  (((i >> 12) & 1)    << 31)
          | (((i >>  5) & 0x3F) << 25)
          | ((riv_u32)rs2       << 20)
          | ((riv_u32)rs1       << 15)
          | ((riv_u32)f3        << 12)
          | (((i >>  1) & 0xF)  <<  8)
          | (((i >> 11) & 1)    <<  7)
          |  (riv_u32)op;
}

/* U-type: imm[31:12] occupies the upper 20 bits literally. */
static riv_u32 enc_u(int rd, riv_i32 imm, int op) {
    return  ((riv_u32)imm & 0xFFFFF000u)
          | ((riv_u32)rd << 7)
          |  (riv_u32)op;
}

/* J-type: imm[20|10:1|11|19:12], LSB implicit zero. */
static riv_u32 enc_j(int rd, riv_i32 off, int op) {
    riv_u32 i = (riv_u32)off & 0x1FFFFE;
    return  (((i >> 20) & 1)     << 31)
          | (((i >>  1) & 0x3FF) << 21)
          | (((i >> 11) & 1)     << 20)
          | (((i >> 12) & 0xFF)  << 12)
          | ((riv_u32)rd         <<  7)
          |  (riv_u32)op;
}

/* Parse a fence flag string. Each letter maps to one bit of the 4-bit
 * predecessor/successor set: i=in(8), o=out(4), r=read(2), w=write(1). */
static int rv_fence_set(const char *s) {
    int m = 0;
    for (; *s; ++s) {
        switch (*s) {
        case 'i': m |= 8; break;
        case 'o': m |= 4; break;
        case 'r': m |= 2; break;
        case 'w': m |= 1; break;
        default: riv_as_die("bad fence flag:", s);
        }
    }
    return m;
}

/* --------------------------- ISA table -------------------------------
 *
 * Each entry binds a mnemonic to an instruction format kind plus the
 * three encoding fields (funct3, funct7, opcode). The kind drives
 * operand parsing in rv_encode().
 */
enum {
    K_R,        /* register-register arithmetic                  */
    K_I,        /* register-immediate arithmetic                 */
    K_SHIFT,    /* shift by 5-bit immediate                       */
    K_LOAD,     /* lX rd, imm(rs1)                                */
    K_STORE,    /* sX rs2, imm(rs1)                               */
    K_BRANCH,   /* bcc rs1, rs2, label                            */
    K_JAL,      /* jal rd, label                                  */
    K_JALR,     /* jalr rd, rs1, imm  /  jalr rd, imm(rs1)        */
    K_LUI,      /* lui rd, imm20                                  */
    K_AUIPC,    /* auipc rd, imm20                                */
    K_SYS,      /* ecall / ebreak                                 */
    K_FENCE,    /* fence pred, succ  /  fence.i                    */
};

typedef struct {
    const char *name;
    int kind, f3, f7, op;
} insn_t;

static const insn_t rv_isa[] = {
    /* ---- RV32I: register-register --------------------------------- */
    { "add",    K_R,      0x0, 0x00, 0x33 },
    { "sub",    K_R,      0x0, 0x20, 0x33 },
    { "sll",    K_R,      0x1, 0x00, 0x33 },
    { "slt",    K_R,      0x2, 0x00, 0x33 },
    { "sltu",   K_R,      0x3, 0x00, 0x33 },
    { "xor",    K_R,      0x4, 0x00, 0x33 },
    { "srl",    K_R,      0x5, 0x00, 0x33 },
    { "sra",    K_R,      0x5, 0x20, 0x33 },
    { "or",     K_R,      0x6, 0x00, 0x33 },
    { "and",    K_R,      0x7, 0x00, 0x33 },

    /* ---- RV32I: register-immediate -------------------------------- */
    { "addi",   K_I,      0x0, 0,    0x13 },
    { "slti",   K_I,      0x2, 0,    0x13 },
    { "sltiu",  K_I,      0x3, 0,    0x13 },
    { "xori",   K_I,      0x4, 0,    0x13 },
    { "ori",    K_I,      0x6, 0,    0x13 },
    { "andi",   K_I,      0x7, 0,    0x13 },
    { "slli",   K_SHIFT,  0x1, 0x00, 0x13 },
    { "srli",   K_SHIFT,  0x5, 0x00, 0x13 },
    { "srai",   K_SHIFT,  0x5, 0x20, 0x13 },

    /* ---- RV32I: loads --------------------------------------------- */
    { "lb",     K_LOAD,   0x0, 0,    0x03 },
    { "lh",     K_LOAD,   0x1, 0,    0x03 },
    { "lw",     K_LOAD,   0x2, 0,    0x03 },
    { "lbu",    K_LOAD,   0x4, 0,    0x03 },
    { "lhu",    K_LOAD,   0x5, 0,    0x03 },

    /* ---- RV32I: stores -------------------------------------------- */
    { "sb",     K_STORE,  0x0, 0,    0x23 },
    { "sh",     K_STORE,  0x1, 0,    0x23 },
    { "sw",     K_STORE,  0x2, 0,    0x23 },

    /* ---- RV32I: branches ------------------------------------------ */
    { "beq",    K_BRANCH, 0x0, 0,    0x63 },
    { "bne",    K_BRANCH, 0x1, 0,    0x63 },
    { "blt",    K_BRANCH, 0x4, 0,    0x63 },
    { "bge",    K_BRANCH, 0x5, 0,    0x63 },
    { "bltu",   K_BRANCH, 0x6, 0,    0x63 },
    { "bgeu",   K_BRANCH, 0x7, 0,    0x63 },

    /* ---- RV32I: jumps / upper-immediate / system ------------------ */
    { "jal",    K_JAL,    0,   0,    0x6F },
    { "jalr",   K_JALR,   0x0, 0,    0x67 },
    { "lui",    K_LUI,    0,   0,    0x37 },
    { "auipc",  K_AUIPC,  0,   0,    0x17 },
    { "ecall",  K_SYS,    0,   0,    0x73 },
    { "ebreak", K_SYS,    0,   1,    0x73 },

    /* ---- RV32M: multiply / divide extension ----------------------- */
    { "mul",    K_R,      0x0, 0x01, 0x33 },
    { "mulh",   K_R,      0x1, 0x01, 0x33 },
    { "mulhsu", K_R,      0x2, 0x01, 0x33 },
    { "mulhu",  K_R,      0x3, 0x01, 0x33 },
    { "div",    K_R,      0x4, 0x01, 0x33 },
    { "divu",   K_R,      0x5, 0x01, 0x33 },
    { "rem",    K_R,      0x6, 0x01, 0x33 },
    { "remu",   K_R,      0x7, 0x01, 0x33 },

    /* ---- Memory ordering ----------------------------------------- */
    { "fence",   K_FENCE, 0,   0,    0x0F },
    { "fence.i", K_FENCE, 0x1, 0,    0x0F },
};
#define RV_ISA_N (int)(sizeof(rv_isa)/sizeof(rv_isa[0]))

static const insn_t *rv_find(const char *m){
    for (int i = 0; i < RV_ISA_N; ++i)
        if (strcmp(rv_isa[i].name, m) == 0) return &rv_isa[i];
    return NULL;
}

/* --------------------------- pseudo-ops -----------------------------
 *
 * Each pseudo rewrites the operand list in place by replacing the
 * mnemonic and reshuffling operands, then returns the new operand
 * count. The frontend then re-dispatches as if the user had written
 * the canonical form. */
static int rv_expand_pseudo(char **t, int nt, char *scratch[8]) {
    /* nop  ->  addi x0, x0, 0 */
    if (strcmp(t[0], "nop") == 0) {
        scratch[0] = "addi";
        scratch[1] = "x0";
        scratch[2] = "x0";
        scratch[3] = "0";
        for (int i = 0; i < 4; ++i) t[i] = scratch[i];
        return 4;
    }
    /* mv rd, rs  ->  addi rd, rs, 0 */
    if (strcmp(t[0], "mv") == 0) {
        if (nt < 3) riv_as_die("mv needs 2 ops", NULL);
        scratch[0] = "addi";
        scratch[1] = t[1];
        scratch[2] = t[2];
        scratch[3] = "0";
        for (int i = 0; i < 4; ++i) t[i] = scratch[i];
        return 4;
    }
    /* li rd, imm12  ->  addi rd, x0, imm */
    if (strcmp(t[0], "li") == 0) {
        if (nt < 3) riv_as_die("li needs 2 ops", NULL);
        scratch[0] = "addi";
        scratch[1] = t[1];
        scratch[2] = "x0";
        scratch[3] = t[2];
        for (int i = 0; i < 4; ++i) t[i] = scratch[i];
        return 4;
    }
    /* j label  ->  jal x0, label */
    if (strcmp(t[0], "j") == 0) {
        if (nt < 2) riv_as_die("j needs 1 op", NULL);
        scratch[0] = "jal";
        scratch[1] = "x0";
        scratch[2] = t[1];
        for (int i = 0; i < 3; ++i) t[i] = scratch[i];
        return 3;
    }
    /* ret  ->  jalr x0, x1, 0 */
    if (strcmp(t[0], "ret") == 0) {
        scratch[0] = "jalr";
        scratch[1] = "x0";
        scratch[2] = "x1";
        scratch[3] = "0";
        for (int i = 0; i < 4; ++i) t[i] = scratch[i];
        return 4;
    }
    return nt;
}

/* --------------------------- assembler ------------------------------- */
static int rv_encode(char **tok, int nt) {
    char *scratch[8];
    nt = rv_expand_pseudo(tok, nt, scratch);
    const insn_t *ix = rv_find(tok[0]);
    if (!ix) return 0;

    riv_u32 w = 0;
    switch (ix->kind) {
    case K_R:
        if (nt < 4) riv_as_die("need 3 operands", tok[0]);
        w = enc_r(rv_parse_reg(tok[1]), ix->f3,
                  rv_parse_reg(tok[2]), rv_parse_reg(tok[3]),
                  ix->f7, ix->op);
        break;
    case K_I:
        if (nt < 4) riv_as_die("need 3 operands", tok[0]);
        w = enc_i(rv_parse_reg(tok[1]), ix->f3,
                  rv_parse_reg(tok[2]),
                  (riv_i32)riv_as_parse_imm(tok[3]), ix->op);
        break;
    case K_SHIFT:
        if (nt < 4) riv_as_die("need 3 operands", tok[0]);
        w = enc_shift(rv_parse_reg(tok[1]), ix->f3,
                      rv_parse_reg(tok[2]),
                      (int)riv_as_parse_imm(tok[3]) & 0x1F,
                      ix->f7, ix->op);
        break;
    case K_LOAD: {
        if (nt < 3) riv_as_die("need 2 operands", tok[0]);
        char *imm, *rs1;
        if (!riv_as_split_offset(tok[2], &imm, &rs1))
            riv_as_die("expected imm(reg):", tok[2]);
        w = enc_i(rv_parse_reg(tok[1]), ix->f3, rv_parse_reg(rs1),
                  (riv_i32)riv_as_parse_imm(imm), ix->op);
    } break;
    case K_STORE: {
        if (nt < 3) riv_as_die("need 2 operands", tok[0]);
        char *imm, *rs1;
        if (!riv_as_split_offset(tok[2], &imm, &rs1))
            riv_as_die("expected imm(reg):", tok[2]);
        w = enc_s(rv_parse_reg(tok[1]), ix->f3, rv_parse_reg(rs1),
                  (riv_i32)riv_as_parse_imm(imm), ix->op);
    } break;
    case K_BRANCH: {
        if (nt < 4) riv_as_die("need 3 operands", tok[0]);
        riv_i32 tgt = (riv_i32)riv_as_parse_imm(tok[3]);
        riv_i32 off = tgt - (riv_i32)(riv_as_pc + riv_as_base);
        w = enc_b(rv_parse_reg(tok[1]), rv_parse_reg(tok[2]),
                  ix->f3, off, ix->op);
    } break;
    case K_JAL: {
        if (nt < 3) riv_as_die("need 2 operands", tok[0]);
        riv_i32 tgt = (riv_i32)riv_as_parse_imm(tok[2]);
        riv_i32 off = tgt - (riv_i32)(riv_as_pc + riv_as_base);
        w = enc_j(rv_parse_reg(tok[1]), off, ix->op);
    } break;
    case K_JALR:
        if (nt == 3) {
            char *imm, *rs1;
            if (riv_as_split_offset(tok[2], &imm, &rs1)) {
                w = enc_i(rv_parse_reg(tok[1]), ix->f3, rv_parse_reg(rs1),
                          (riv_i32)riv_as_parse_imm(imm), ix->op);
                break;
            }
            riv_as_die("expected imm(reg):", tok[2]);
        }
        if (nt < 4) riv_as_die("need 3 operands", tok[0]);
        w = enc_i(rv_parse_reg(tok[1]), ix->f3, rv_parse_reg(tok[2]),
                  (riv_i32)riv_as_parse_imm(tok[3]), ix->op);
        break;
    case K_LUI:
    case K_AUIPC: {
        if (nt < 3) riv_as_die("need 2 operands", tok[0]);
        riv_i32 v = (riv_i32)riv_as_parse_imm(tok[2]);
        w = enc_u(rv_parse_reg(tok[1]), v << 12, ix->op);
    } break;
    case K_SYS:
        w = ((riv_u32)ix->f7 << 20) | (riv_u32)ix->op;
        break;
    case K_FENCE:
        if (!strcmp(ix->name, "fence.i")) {
            w = (1u << 12) | (riv_u32)ix->op;
            break;
        }
        {
            int pred = 0x3, succ = 0x3;
            if (nt >= 3) {
                pred = rv_fence_set(tok[1]);
                succ = rv_fence_set(tok[2]);
            }
            w = ((riv_u32)pred << 24) | ((riv_u32)succ << 20) | (riv_u32)ix->op;
        }
        break;
    }
    riv_as_emit_u32(w);
    return 1;
}

const riv_arch_as riv_arch_as_riscv32 = { "riscv32", rv_encode };

