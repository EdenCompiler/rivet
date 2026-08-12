/* RIVET arch: x86_64 disassembler. */

#include "arch.h"

static const char *x86_regs[16] = {
    "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
    "r8", "r9", "r10","r11","r12","r13","r14","r15"
};

/* ===================== disassembler =================================== */

static const char *x86_jcc_name(int cc) {
    static const char *t[16] = {
        "jo","jno","jb","jae","je","jne","jbe","ja",
        "js","jns","jp","jnp","jl","jge","jle","jg"
    };
    return (cc >= 0 && cc < 16) ? t[cc] : "j?";
}

static int x86_decode(riv_u32 pc, const riv_u8 *bytes, int avail,
                      char *out, int cap) {
    int idx = 0;
    if (avail < 1) return 0;

    /* REX prefix */
    int rex = 0, w = 0, r = 0, b = 0;
    if ((bytes[idx] & 0xF0) == 0x40) {
        rex = bytes[idx++];
        w = (rex >> 3) & 1; r = (rex >> 2) & 1; b = rex & 1;
        if (idx >= avail) return 0;
    }

    riv_u8 op = bytes[idx++];

    /* helpers to read fields safely */
    #define NEED(n) do { if (idx + (n) > avail) return 0; } while (0)
    #define IMM32() ( (riv_i32)( bytes[idx]   | (bytes[idx+1] << 8) \
                               | (bytes[idx+2] << 16) | (bytes[idx+3] << 24) ) )
    #define IMM64() ( (riv_i64)bytes[idx] | ((riv_i64)bytes[idx+1] << 8) \
                    | ((riv_i64)bytes[idx+2] << 16) | ((riv_i64)bytes[idx+3] << 24) \
                    | ((riv_i64)bytes[idx+4] << 32) | ((riv_i64)bytes[idx+5] << 40) \
                    | ((riv_i64)bytes[idx+6] << 48) | ((riv_i64)bytes[idx+7] << 56) )

    /* zero-operand single-byte */
    switch (op) {
    case 0x90: snprintf(out, cap, "nop");   return idx;
    case 0xF4: snprintf(out, cap, "hlt");   return idx;
    case 0xCC: snprintf(out, cap, "int3");  return idx;
    case 0xC3: snprintf(out, cap, "ret");   return idx;
    case 0xFA: snprintf(out, cap, "cli");   return idx;
    case 0xFB: snprintf(out, cap, "sti");   return idx;
    case 0xC9: snprintf(out, cap, "leave"); return idx;
    case 0xCF: snprintf(out, cap, "iretq"); return idx;
    case 0xCD: NEED(1);
        snprintf(out, cap, "%-6s 0x%x", "int", bytes[idx]);
        return idx + 1;
    case 0xE8: case 0xE9: {
        NEED(4);
        riv_i32 off = IMM32();
        idx += 4;
        snprintf(out, cap, "%-6s 0x%x", op == 0xE8 ? "call" : "jmp", pc + idx + off);
        return idx;
    }
    case 0xEC: snprintf(out, cap, "in     al, dx"); return idx;
    case 0xEE: snprintf(out, cap, "out    dx, al"); return idx;
    case 0xE4: NEED(1);
        snprintf(out, cap, "%-6s al, 0x%x", "in", bytes[idx]); return idx + 1;
    case 0xE6: NEED(1);
        snprintf(out, cap, "%-6s 0x%x, al", "out", bytes[idx]); return idx + 1;
    }

    /* push/pop reg */
    if (op >= 0x50 && op <= 0x57) {
        int reg = (op - 0x50) | (b << 3);
        snprintf(out, cap, "%-6s %s", "push", x86_regs[reg]); return idx;
    }
    if (op >= 0x58 && op <= 0x5F) {
        int reg = (op - 0x58) | (b << 3);
        snprintf(out, cap, "%-6s %s", "pop", x86_regs[reg]); return idx;
    }
    /* mov rXX, imm64  (REX.W + B8+r io) */
    if (w && op >= 0xB8 && op <= 0xBF) {
        NEED(8);
        riv_i64 v = IMM64();
        idx += 8;
        int reg = (op - 0xB8) | (b << 3);
        snprintf(out, cap, "%-6s %s, 0x%llx", "mov",
                 x86_regs[reg], (unsigned long long)v);
        return idx;
    }

    /* two-byte 0x0F xx */
    if (op == 0x0F) {
        NEED(1);
        riv_u8 o2 = bytes[idx++];
        if (o2 == 0x05) { snprintf(out, cap, "syscall"); return idx; }
        if (o2 == 0x07) { snprintf(out, cap, "sysret");  return idx; }
        if (o2 == 0x0B) { snprintf(out, cap, "ud2");     return idx; }
        if (o2 >= 0x80 && o2 <= 0x8F) {
            NEED(4);
            riv_i32 off = IMM32();
            idx += 4;
            snprintf(out, cap, "%-6s 0x%x", x86_jcc_name(o2 - 0x80), pc + idx + off);
            return idx;
        }
        if (o2 == 0xAF) {
            NEED(1);
            riv_u8 mr = bytes[idx++];
            int reg = ((mr >> 3) & 7) | (r << 3);
            int rm  =  (mr       & 7) | (b << 3);
            snprintf(out, cap, "%-6s %s, %s", "imul", x86_regs[reg], x86_regs[rm]);
            return idx;
        }
        snprintf(out, cap, ".byte  0x0f, 0x%02x", o2);
        return idx;
    }

    /* opcodes that take a ModR/M byte */
    int has_imm8 = 0, has_imm32 = 0;
    const char *mnem = NULL;
    int digit_table = 0;
    int rm_form = 0;     /* 1 ⇒ display "reg, r/m" (RM form) */

    switch (op) {
    case 0x89: mnem = "mov";  break;
    case 0x8B: mnem = "mov";  rm_form = 1; break;
    case 0x8D: mnem = "lea";  rm_form = 1; break;
    case 0x87: mnem = "xchg"; break;
    case 0x01: mnem = "add";  break;
    case 0x03: mnem = "add";  rm_form = 1; break;
    case 0x29: mnem = "sub";  break;
    case 0x2B: mnem = "sub";  rm_form = 1; break;
    case 0x21: mnem = "and";  break;
    case 0x23: mnem = "and";  rm_form = 1; break;
    case 0x09: mnem = "or";   break;
    case 0x0B: mnem = "or";   rm_form = 1; break;
    case 0x31: mnem = "xor";  break;
    case 0x33: mnem = "xor";  rm_form = 1; break;
    case 0x39: mnem = "cmp";  break;
    case 0x3B: mnem = "cmp";  rm_form = 1; break;
    case 0x85: mnem = "test"; break;
    case 0x81: has_imm32 = 1; digit_table = 1; break;
    case 0xC7: has_imm32 = 1; mnem = "mov"; break;
    case 0xC1: has_imm8  = 1; digit_table = 2; break;
    case 0xF7: digit_table = 3; break;
    case 0xFF: digit_table = 4; break;
    default:
        snprintf(out, cap, ".byte  0x%02x", op);
        return idx;
    }

    NEED(1);
    riv_u8 modrm = bytes[idx++];
    int mod  = (modrm >> 6) & 3;
    int regf = (modrm >> 3) & 7;
    int rmf  =  modrm       & 7;
    int reg  = regf | (r << 3);
    int rm   = rmf  | (b << 3);

    /* digit-encoded mnemonic selection */
    if (digit_table == 1) {
        static const char *a81[8] = {"add","or","adc","sbb","and","sub","xor","cmp"};
        mnem = a81[regf];
    } else if (digit_table == 2) {
        static const char *sh[8] = {"rol","ror","rcl","rcr","shl","shr","?","sar"};
        mnem = sh[regf];
    } else if (digit_table == 3) {
        static const char *f7[8] = {"test","?","not","neg","mul","imul","div","idiv"};
        mnem = f7[regf];
    } else if (digit_table == 4) {
        static const char *ff[8] = {"inc","dec","call","callf","jmp","jmpf","push","?"};
        mnem = ff[regf];
    }

    /* Memory operand (mod != 3): decode ModR/M + SIB + disp into a string. */
    if (mod != 3) {
        char memstr[80];
        int pos = 0;
        const char *base_name = NULL;
        const char *idx_name  = NULL;
        int scale = 1;
        int has_disp = 0;
        riv_i32 disp = 0;

        if (rmf == 4) {     /* SIB */
            NEED(1);
            riv_u8 sib = bytes[idx++];
            int sc = (sib >> 6) & 3;
            int si = (sib >> 3) & 7;
            int sb =  sib       & 7;
            scale = 1 << sc;
            if (si != 4) idx_name = x86_regs[si | (((rex>>1)&1) << 3)];
            if (sb == 5 && mod == 0) {
                base_name = NULL; has_disp = 1;
                NEED(4); disp = IMM32(); idx += 4;
            } else {
                base_name = x86_regs[sb | (b << 3)];
                if (mod == 1) { NEED(1); disp = (riv_i32)(signed char)bytes[idx++]; has_disp = 1; }
                else if (mod == 2) { NEED(4); disp = IMM32(); idx += 4; has_disp = 1; }
            }
        } else if (rmf == 5 && mod == 0) {
            /* RIP-relative */
            NEED(4); disp = IMM32(); idx += 4;
            snprintf(memstr, sizeof(memstr), "[rip+0x%x]", (unsigned)disp);
            goto disp_done;
        } else {
            base_name = x86_regs[rmf | (b << 3)];
            if (mod == 1) { NEED(1); disp = (riv_i32)(signed char)bytes[idx++]; has_disp = 1; }
            else if (mod == 2) { NEED(4); disp = IMM32(); idx += 4; has_disp = 1; }
        }
        pos = 0; memstr[pos++] = '[';
        if (base_name) pos += snprintf(memstr + pos, sizeof(memstr) - pos, "%s", base_name);
        if (idx_name) pos += snprintf(memstr + pos, sizeof(memstr) - pos,
                                       "%s%s*%d", pos > 1 ? "+" : "", idx_name, scale);
        if (has_disp) {
            if (pos > 1 && disp >= 0)
                pos += snprintf(memstr + pos, sizeof(memstr) - pos, "+0x%x", (unsigned)disp);
            else if (pos > 1)
                pos += snprintf(memstr + pos, sizeof(memstr) - pos, "-0x%x", (unsigned)(-disp));
            else
                pos += snprintf(memstr + pos, sizeof(memstr) - pos, "0x%x", (unsigned)disp);
        }
        memstr[pos++] = ']'; memstr[pos] = 0;
disp_done:
        if (has_imm32) {
            NEED(4); riv_i32 imm = IMM32(); idx += 4;
            snprintf(out, cap, "%-6s %s, 0x%x", mnem, memstr, (unsigned)imm);
        } else if (has_imm8) {
            NEED(1); int imm = bytes[idx++];
            snprintf(out, cap, "%-6s %s, 0x%x", mnem, memstr, imm);
        } else if (digit_table == 3 || digit_table == 4) {
            snprintf(out, cap, "%-6s %s", mnem, memstr);
        } else if (rm_form) {
            snprintf(out, cap, "%-6s %s, %s", mnem, x86_regs[reg], memstr);
        } else {
            snprintf(out, cap, "%-6s %s, %s", mnem, memstr, x86_regs[reg]);
        }
        return idx;
    }

    /* register-register form (mod = 11) */
    if (rm_form) {
        snprintf(out, cap, "%-6s %s, %s", mnem, x86_regs[reg], x86_regs[rm]);
        return idx;
    }
    if (has_imm32) {
        NEED(4); riv_i32 imm = IMM32(); idx += 4;
        snprintf(out, cap, "%-6s %s, 0x%x", mnem, x86_regs[rm], (unsigned)imm);
        return idx;
    }
    if (has_imm8) {
        NEED(1); int imm = bytes[idx++];
        snprintf(out, cap, "%-6s %s, 0x%x", mnem, x86_regs[rm], imm);
        return idx;
    }
    if (digit_table == 3 || digit_table == 4) {
        snprintf(out, cap, "%-6s %s", mnem, x86_regs[rm]);
        return idx;
    }
    snprintf(out, cap, "%-6s %s, %s", mnem, x86_regs[rm], x86_regs[reg]);
    return idx;

    #undef NEED
    #undef IMM32
    #undef IMM64
}

const riv_arch_disas riv_disas_x86_64 = { "x86_64", 1, x86_decode };
