# Tools Reference

This is a practical reference to the RIVET toolchain. All tools are
plain C programs built from `src/` and produced into `build/`.

## `rivet-as`

Two-pass flat-binary assembler with pluggable architecture backends.

```bash
rivet-as [-m riscv32|x86_64] input.s -o output.bin [-b base_addr]
```

### Options

- `-m <arch>` — select backend (`riscv32` or `x86_64`). Default `riscv32`.
- `-o <file>` — output binary path. Default `a.bin`.
- `-b <addr>` — base address used for PC-relative branches and labels.

### Source Format

A source file is a sequence of lines. Each line may contain:

- comments (`#` or `;` to end of line)
- labels (`name:`)
- directives (`.text`, `.data`, `.org N`, `.word V`, `.byte V`, `.half`,
  `.zero N`, `.ascii`, `.asciz`, `.string`, `.align`, `.p2align`,
  `.global`, `.equ`, `.set`, `.arch`)
- instructions (mnemonic + operands)

### Supported Architectures

The frontend dispatches each instruction line to one of two backends:

- `src/arch/riscv32_as.c` — RV32I + M extension + fence/fence.i
- `src/arch/x86_64_as.c`  — x86_64 subset with full memory operands

The arch can also be switched mid-file with the `.arch` directive.

## `rivet-disas`

Architecture-pluggable disassembler.

```bash
rivet-disas [-m riscv32|x86_64] in.bin [-b base_addr]
```

Reads a flat binary, dispatches each instruction to the chosen
architecture's decoder, and prints `pc: hex   mnemonic operands`.

The RV32 decoder handles all RV32I + M instructions plus
`fence`/`fence.i`. The x86_64 decoder handles REX-prefixed opcodes,
ModR/M, SIB, immediates, REL32 branches, and the same memory operand
formats the assembler emits.

## `rivet-ld`

Locator/linker that takes flat binaries and a layout script and produces
a packed image.

```bash
rivet-ld [-g] script.ld [-o output]
```

### Options

- `-g` — emit a GNU `ld(1)` script instead of linking
- `-o` — override the output path declared in the script

### Layout Script

Line-based, `#` for comments:

```
entry  <symbol_or_addr>
fill   <byte>                       # default fill (default 0x00)
region <name> origin=<addr> length=<bytes> [fill=<byte>]
place  <file.bin> in <region> [at <offset|+offset>]
define <symbol> = <addr>
output <path>
```

Numbers accept `0x`, decimal, or `K`/`M` suffix.

### Example

```
entry  _start
fill   0xff

region rom origin=0x80000000 length=4K  fill=0x00
region ram origin=0x80100000 length=16K

place  boot.bin   in rom
output image.bin
```

## `rivet-img`

Image post-processor.

```bash
rivet-img in.bin -o out.bin [--pad SIZE] [--fill BYTE] [--crc32-trailer]
```

### Options

- `--pad SIZE` — pad the final image to SIZE bytes (accepts `K`/`M`)
- `--fill BYTE` — fill byte for padding (default 0xFF)
- `--crc32-trailer` — append a 4-byte little-endian CRC-32 of the
  preceding bytes at the end of the image

## `rivet-objcopy`

Convert between flat binary and Intel HEX / Motorola SREC formats.

```bash
rivet-objcopy -O ihex in.bin out.hex [-b base_addr]
rivet-objcopy -O srec in.bin out.s19 [-b base_addr]
rivet-objcopy -I ihex in.hex out.bin
```

### Options

- `-O <format>` — output format (`ihex` or `srec`)
- `-I <format>` — input format (currently `ihex`)
- `-b <addr>` — base address for the first record (HEX/SREC only)

### Example

```bash
rivet-objcopy -O ihex bin/boot.bin bin/boot.hex -b 0x80000000
```

## `rivet-strings`

Extract printable ASCII runs from a binary.

```bash
rivet-strings input.bin [-n min_len] [-o output]
```

### Options

- `-n <len>` — minimum run length (default 4)
- `-o <file>` — write to file instead of stdout

Walks the file byte-by-byte; emits any run of `min_len` or more
printable characters terminated by a non-printable byte. Each line
shows the offset in hex followed by the captured string.

### Example

```
$ rivet-strings bin/hello.bin -n 4
00000030  hello from x86_64
```

## `rivet-size`

Report image size with a quick byte-distribution histogram.

```bash
rivet-size input.bin [input2.bin ...]
```

For each file, prints total byte count, non-zero count and ratio, zero
count, 0xFF count, printable ASCII count, control-byte count, high-byte
count, and the first / last non-zero offsets. Useful for sanity-checking
firmware images.

### Example

```
$ rivet-size bin/hello.bin
== bin/hello.bin
  total            67
  non-zero         47 (70.1%)
  zeros            20
  0xFF             1
  printable ASCII  27
  other control    8
  high bytes       11
  first non-zero   0x0
  last  non-zero   0x41
```

## Build Pipeline

The seven tools chain naturally:

```
*.s
  └─ rivet-as ─▶ *.bin
                   ├─ rivet-ld      ─▶ image.bin
                   │                     ├─ rivet-img      ─▶ firmware.bin
                   │                     │                       └─ rivet-objcopy ─▶ firmware.hex
                   ├─ rivet-disas   ─▶ inspection
                   ├─ rivet-strings ─▶ ASCII runs
                   └─ rivet-size    ─▶ byte histogram
```

## Make Targets

The shipped Makefile drives the whole pipeline:

```bash
make            # full build
make tools      # tools only
make examples   # assemble every example .s into bin/
make objects    # compile-check every example .c
make clean      # remove build/ and bin/
make list       # show what will be produced
```
