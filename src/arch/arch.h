/* RIVET arch interface — shared between frontends (rivet-as / rivet-disas)
 * and per-architecture backends. */
#ifndef RIVET_TOOL_ARCH_H
#define RIVET_TOOL_ARCH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef unsigned char       riv_u8;
typedef unsigned short      riv_u16;
typedef unsigned int        riv_u32;
typedef unsigned long long  riv_u64;
typedef signed   int        riv_i32;
typedef signed   long long  riv_i64;

#define ARCH_MAX_TOKENS 12

/* --------------------------------------------------------------------- */
/* Frontend-provided services — implemented in rivet-as.c.               */
/* --------------------------------------------------------------------- */
extern riv_u32      riv_as_pc;          /* current output PC (offset)   */
extern riv_u32      riv_as_base;        /* base address added on emit   */
extern int          riv_as_pass;        /* 0 = collect symbols, 1 = emit */
extern const char  *riv_as_file;
extern int          riv_as_line;

void riv_as_die(const char *msg, const char *extra);

void riv_as_emit_u8 (riv_u8  v);
void riv_as_emit_u16(riv_u16 v);
void riv_as_emit_u32(riv_u32 v);
void riv_as_emit_u64(riv_u64 v);

int  riv_as_sym_find(const char *name);
void riv_as_sym_set (const char *name, riv_u32 value);
riv_u32 riv_as_sym_value(int idx);

/* Returns 0 if numeric literal, 1 if symbol, -1 on unknown symbol in pass 1.
 * In pass 0 returns 0 for unresolved symbols. */
riv_i64 riv_as_parse_imm(const char *tok);

/* Split "imm(reg)" tokens in-place. Returns 1 if split, 0 otherwise. */
int  riv_as_split_offset(char *tok, char **imm_out, char **reg_out);

/* --------------------------------------------------------------------- */
/* Assembler backend interface.                                          */
/* encode_line returns 1 if the mnemonic was handled, 0 otherwise.       */
/* --------------------------------------------------------------------- */
typedef struct riv_arch_as {
    const char *name;
    int (*encode_line)(char **tok, int nt);
} riv_arch_as;

extern const riv_arch_as riv_arch_as_riscv32;
extern const riv_arch_as riv_arch_as_x86_64;

/* --------------------------------------------------------------------- */
/* Disassembler backend interface.                                       */
/* decode returns number of bytes consumed; 0 = unable to decode.        */
/* --------------------------------------------------------------------- */
typedef struct riv_arch_disas {
    const char *name;
    int min_unit;   /* smallest decode unit, in bytes */
    int (*decode)(riv_u32 pc, const riv_u8 *bytes, int avail,
                  char *out, int cap);
} riv_arch_disas;

extern const riv_arch_disas riv_disas_riscv32;
extern const riv_arch_disas riv_disas_x86_64;

#endif /* RIVET_TOOL_ARCH_H */
