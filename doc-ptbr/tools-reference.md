# Referência das Ferramentas

Esta é uma referência prática do toolchain RIVET. Todas as ferramentas
são programas C simples compilados a partir de `src/` e produzidos em
`build/`.

## `rivet-as`

Assembler de duas passagens com saída flat-binary e backends de
arquitetura plugáveis.

```bash
rivet-as [-m riscv32|x86_64] input.s -o output.bin [-b base_addr]
```

### Opções

- `-m <arch>` — seleciona backend (`riscv32` ou `x86_64`). Padrão
  `riscv32`.
- `-o <file>` — caminho do binário de saída. Padrão `a.bin`.
- `-b <addr>` — endereço base usado para branches PC-relativos e
  rótulos.

### Formato Fonte

Um arquivo fonte é uma sequência de linhas. Cada linha pode conter:

- comentários (`#` ou `;` até o fim da linha)
- rótulos (`name:`)
- diretivas (`.text`, `.data`, `.org N`, `.word V`, `.byte V`,
  `.half`, `.zero N`, `.ascii`, `.asciz`, `.string`, `.align`,
  `.p2align`, `.global`, `.equ`, `.set`, `.arch`)
- instruções (mnemônico + operandos)

### Arquiteturas Suportadas

O frontend distribui cada linha de instrução para um de dois backends:

- `src/arch/riscv32_as.c` — RV32I + extensão M + fence/fence.i
- `src/arch/x86_64_as.c`  — subconjunto x86_64 com operandos de
  memória completos

A arquitetura também pode ser trocada no meio do arquivo via a
diretiva `.arch`.

## `rivet-disas`

Desmontador com arquitetura plugável.

```bash
rivet-disas [-m riscv32|x86_64] in.bin [-b base_addr]
```

Lê um binário flat, distribui cada instrução para o decodificador da
arquitetura escolhida e imprime `pc: hex   mnemônico operandos`.

O decodificador RV32 trata todas as instruções RV32I + M mais
`fence`/`fence.i`. O decodificador x86_64 trata opcodes prefixados por
REX, ModR/M, SIB, imediatos, branches REL32 e os mesmos formatos de
operando de memória que o assembler emite.

## `rivet-ld`

Locator/linker que pega binários flat mais um script de layout e
produz uma imagem empacotada.

```bash
rivet-ld [-g] script.ld [-o output]
```

### Opções

- `-g` — emite um script `ld(1)` GNU em vez de fazer link
- `-o` — sobrescreve o caminho de saída declarado no script

### Script de Layout

Baseado em linhas, `#` para comentários:

```
entry  <symbol_or_addr>
fill   <byte>                       # preenchimento padrão (padrão 0x00)
region <name> origin=<addr> length=<bytes> [fill=<byte>]
place  <file.bin> in <region> [at <offset|+offset>]
define <symbol> = <addr>
output <path>
```

Números aceitam `0x`, decimal ou sufixo `K`/`M`.

### Exemplo

```
entry  _start
fill   0xff

region rom origin=0x80000000 length=4K  fill=0x00
region ram origin=0x80100000 length=16K

place  boot.bin   in rom
output image.bin
```

## `rivet-img`

Pós-processador de imagem.

```bash
rivet-img in.bin -o out.bin [--pad SIZE] [--fill BYTE] [--crc32-trailer]
```

### Opções

- `--pad SIZE` — preenche a imagem final até SIZE bytes (aceita
  `K`/`M`)
- `--fill BYTE` — byte de preenchimento (padrão 0xFF)
- `--crc32-trailer` — adiciona um CRC-32 de 4 bytes little-endian dos
  bytes anteriores no fim da imagem

## `rivet-objcopy`

Converte entre binário flat e formatos Intel HEX / Motorola SREC.

```bash
rivet-objcopy -O ihex in.bin out.hex [-b base_addr]
rivet-objcopy -O srec in.bin out.s19 [-b base_addr]
rivet-objcopy -I ihex in.hex out.bin
```

### Opções

- `-O <format>` — formato de saída (`ihex` ou `srec`)
- `-I <format>` — formato de entrada (atualmente `ihex`)
- `-b <addr>` — endereço base para o primeiro registro (apenas HEX/SREC)

### Exemplo

```bash
rivet-objcopy -O ihex bin/boot.bin bin/boot.hex -b 0x80000000
```

## `rivet-strings`

Extrai sequências ASCII imprimíveis de um binário.

```bash
rivet-strings input.bin [-n min_len] [-o output]
```

### Opções

- `-n <len>` — comprimento mínimo da sequência (padrão 4)
- `-o <file>` — escreve em arquivo em vez de stdout

Percorre o arquivo byte a byte; emite qualquer sequência de
`min_len` ou mais caracteres imprimíveis terminada por byte não
imprimível. Cada linha mostra o offset em hex seguido da string
capturada.

### Exemplo

```
$ rivet-strings bin/hello.bin -n 4
00000030  hello from x86_64
```

## `rivet-size`

Reporta tamanho da imagem com um histograma rápido de distribuição de
bytes.

```bash
rivet-size input.bin [input2.bin ...]
```

Para cada arquivo, imprime contagem total de bytes, contagem e razão de
não-zero, contagem de zeros, contagem de 0xFF, contagem de ASCII
imprimível, contagem de bytes de controle, contagem de bytes altos e o
primeiro / último offset não-zero. Útil para sanity-check de imagens de
firmware.

### Exemplo

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

## Pipeline de Compilação

As sete ferramentas se encadeiam naturalmente:

```
*.s
  └─ rivet-as ─▶ *.bin
                   ├─ rivet-ld      ─▶ image.bin
                   │                     ├─ rivet-img      ─▶ firmware.bin
                   │                     │                       └─ rivet-objcopy ─▶ firmware.hex
                   ├─ rivet-disas   ─▶ inspeção
                   ├─ rivet-strings ─▶ sequências ASCII
                   └─ rivet-size    ─▶ histograma de bytes
```

## Alvos do Make

O Makefile distribuído conduz todo o pipeline:

```bash
make            # build completo
make tools      # apenas ferramentas
make examples   # monta cada .s de exemplo em bin/
make objects    # verifica compilação de cada .c de exemplo
make clean      # remove build/ e bin/
make list       # mostra o que será produzido
```
