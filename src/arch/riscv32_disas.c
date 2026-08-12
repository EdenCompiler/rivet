/* RIVET arch: RISC-V 32-bit disassembler. */

#include "arch.h"

static const char *rv_abi[32] = {
    "zero","ra","sp","gp","tp","t0","t1","t2","s0","s1",
    "a0","a1","a2","a3","a4","a5","a6","a7",
    "s2","s3","s4","s5","s6","s7","s8","s9","s10","s11",
    "t3","t4","t5","t6"
};

/* --------------------------- disassembler ---------------------------- */
static riv_i32 sext(riv_u32 v, int bits) {
    riv_u32 m = 1u << (bits - 1);
    return (riv_i32)((v ^ m) - m);
}

static int rv_decode(riv_u32 pc, const riv_u8 *bytes, int avail,
                     char *out, int cap) {
    if (avail < 4) return 0;
    riv_u32 w = (riv_u32)bytes[0] | ((riv_u32)bytes[1] << 8)
              | ((riv_u32)bytes[2] << 16) | ((riv_u32)bytes[3] << 24);
    riv_u32 op = w & 0x7F;
    riv_u32 rd  = (w >>  7) & 0x1F;
    riv_u32 f3  = (w >> 12) & 0x7;
    riv_u32 rs1 = (w >> 15) & 0x1F;
    riv_u32 rs2 = (w >> 20) & 0x1F;
    riv_u32 f7  = (w >> 25) & 0x7F;
    const char *RN = "???";
    switch (op) {
    case 0x33: { /* OP / RV32M */
        const char *m = "???";
        switch ((f7 << 3) | f3) {
        case (0x00<<3)|0: m = "add";    break;
        case (0x20<<3)|0: m = "sub";    break;
        case (0x00<<3)|1: m = "sll";    break;
        case (0x00<<3)|2: m = "slt";    break;
        case (0x00<<3)|3: m = "sltu";   break;
        case (0x00<<3)|4: m = "xor";    break;
        case (0x00<<3)|5: m = "srl";    break;
        case (0x20<<3)|5: m = "sra";    break;
        case (0x00<<3)|6: m = "or";     break;
        case (0x00<<3)|7: m = "and";    break;
        case (0x01<<3)|0: m = "mul";    break;
        case (0x01<<3)|1: m = "mulh";   break;
        case (0x01<<3)|2: m = "mulhsu"; break;
        case (0x01<<3)|3: m = "mulhu";  break;
        case (0x01<<3)|4: m = "div";    break;
        case (0x01<<3)|5: m = "divu";   break;
        case (0x01<<3)|6: m = "rem";    break;
        case (0x01<<3)|7: m = "remu";   break;
        }
        snprintf(out, cap, "%-6s %s, %s, %s", m, rv_abi[rd], rv_abi[rs1], rv_abi[rs2]);
        return 4;
    }
    case 0x13: { /* OP-IMM */
        riv_i32 imm = sext(w >> 20, 12);
        const char *m = RN;
        switch (f3) {
        case 0: m = "addi"; break; case 2: m = "slti"; break;
        case 3: m = "sltiu";break; case 4: m = "xori"; break;
        case 6: m = "ori";  break; case 7: m = "andi"; break;
        case 1: m = "slli"; imm &= 0x1F; break;
        case 5: m = (f7 == 0x20) ? "srai" : "srli"; imm &= 0x1F; break;
        }
        snprintf(out, cap, "%-6s %s, %s, %d", m, rv_abi[rd], rv_abi[rs1], imm);
        return 4;
    }
    case 0x03: { /* LOAD */
        const char *m = RN;
        switch (f3) {
        case 0: m = "lb";  break; case 1: m = "lh"; break; case 2: m = "lw"; break;
        case 4: m = "lbu"; break; case 5: m = "lhu"; break;
        }
        snprintf(out, cap, "%-6s %s, %d(%s)", m, rv_abi[rd],
                 sext(w >> 20, 12), rv_abi[rs1]);
        return 4;
    }
    case 0x23: { /* STORE */
        riv_i32 imm = sext(((w >> 25) << 5) | ((w >> 7) & 0x1F), 12);
        const char *m = (f3 == 0) ? "sb" : (f3 == 1) ? "sh" : (f3 == 2) ? "sw" : RN;
        snprintf(out, cap, "%-6s %s, %d(%s)", m, rv_abi[rs2], imm, rv_abi[rs1]);
        return 4;
    }
    case 0x63: { /* BRANCH */
        riv_u32 imm = (((w >> 31) & 1) << 12) | (((w >> 7) & 1) << 11)
                    | (((w >> 25) & 0x3F) << 5) | (((w >> 8) & 0xF) << 1);
        riv_i32 off = sext(imm, 13);
        const char *m = RN;
        switch (f3) {
        case 0: m = "beq";  break; case 1: m = "bne";  break;
        case 4: m = "blt";  break; case 5: m = "bge";  break;
        case 6: m = "bltu"; break; case 7: m = "bgeu"; break;
        }
        snprintf(out, cap, "%-6s %s, %s, 0x%x", m, rv_abi[rs1], rv_abi[rs2], pc + off);
        return 4;
    }
    case 0x6F: { /* JAL */
        riv_u32 imm = (((w >> 31) & 1) << 20) | (((w >> 12) & 0xFF) << 12)
                    | (((w >> 20) & 1) << 11) | (((w >> 21) & 0x3FF) << 1);
        riv_i32 off = sext(imm, 21);
        snprintf(out, cap, "%-6s %s, 0x%x", "jal", rv_abi[rd], pc + off);
        return 4;
    }
    case 0x67:
        snprintf(out, cap, "%-6s %s, %s, %d", "jalr",
                 rv_abi[rd], rv_abi[rs1], sext(w >> 20, 12));
        return 4;
    case 0x37:
        snprintf(out, cap, "%-6s %s, 0x%x", "lui",   rv_abi[rd], w >> 12); return 4;
    case 0x17:
        snprintf(out, cap, "%-6s %s, 0x%x", "auipc", rv_abi[rd], w >> 12); return 4;
    case 0x73:
        snprintf(out, cap, "%s", (w >> 20) ? "ebreak" : "ecall"); return 4;
    case 0x0F: {
        if (f3 == 1) { snprintf(out, cap, "fence.i"); return 4; }
        static const char *flags = "iorw";
        char pr[5], sc[5]; int pn = 0, sn = 0;
        riv_u32 pred = (w >> 24) & 0xF, succ = (w >> 20) & 0xF;
        for (int i = 0; i < 4; ++i) if (pred & (1 << (3 - i))) pr[pn++] = flags[i];
        for (int i = 0; i < 4; ++i) if (succ & (1 << (3 - i))) sc[sn++] = flags[i];
        pr[pn] = 0; sc[sn] = 0;
        snprintf(out, cap, "%-6s %s, %s", "fence",
                 pr[0] ? pr : "(none)", sc[0] ? sc : "(none)");
        return 4;
    }
    }
    snprintf(out, cap, ".word  0x%08x", w);
    return 4;
}

const riv_arch_disas riv_disas_riscv32 = { "riscv32", 4, rv_decode };
