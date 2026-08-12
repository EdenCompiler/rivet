/* RIVET — arch.h : architecture detection */
#ifndef RIVET_ARCH_H
#define RIVET_ARCH_H

#if defined(__x86_64__) || defined(_M_X64)
#  define RIVET_ARCH_X86_64 1
#  define RIVET_ARCH_NAME "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
#  define RIVET_ARCH_X86 1
#  define RIVET_ARCH_NAME "x86"
#elif defined(__aarch64__)
#  define RIVET_ARCH_AARCH64 1
#  define RIVET_ARCH_NAME "aarch64"
#elif defined(__arm__)
#  define RIVET_ARCH_ARM 1
#  define RIVET_ARCH_NAME "arm"
#elif defined(__riscv)
#  define RIVET_ARCH_RISCV 1
#  define RIVET_ARCH_NAME "riscv"
#else
#  define RIVET_ARCH_UNKNOWN 1
#  define RIVET_ARCH_NAME "unknown"
#endif

#if RIVET_ARCH_X86_64 || RIVET_ARCH_AARCH64
#  define RIVET_WORD_BITS 64
#else
#  define RIVET_WORD_BITS 32
#endif

#endif /* RIVET_ARCH_H */
