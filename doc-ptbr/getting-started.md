# Primeiros Passos

## Requisitos

- GCC (ou Clang) com suporte a C99
- GNU make
- Um toolchain cruzado para a arquitetura alvo ao rodar em hardware real
- QEMU para testes em emulador (opcional)

RIVET é compilado com o compilador do host. A compilação cruzada para o
alvo acontece depois, quando seu código de kernel ou firmware consome
os cabeçalhos do runtime.

## Compilar Todos os Artefatos

A partir da raiz do projeto:

```bash
make
```

Isso compila o toolchain dentro de `build/`, monta cada `.s` de exemplo
dentro de `bin/`, e faz a verificação de compilação de cada `.c` de
exemplo.

## Compilar Apenas as Ferramentas

```bash
make tools
```

Produz `build/rivet-as`, `build/rivet-disas`, `build/rivet-ld`,
`build/rivet-objcopy` e `build/rivet-img`.

## Compilar um Único Arquivo Fonte

Você pode montar um arquivo diretamente:

```bash
build/rivet-as -m riscv32 examples/asm/riscv32/boot.s \
                -o bin/boot.bin -b 0x80000000
```

Para x86_64:

```bash
build/rivet-as -m x86_64 examples/asm/x86_64/hello.s \
                -o bin/hello.bin -b 0x400000
```

Se nenhuma arquitetura for informada, o assembler usa `riscv32` por
padrão.

## Desmontar um Binário

```bash
build/rivet-disas -m x86_64 bin/hello.bin -b 0x400000
```

## Construir uma Imagem a partir de Múltiplos Binários

```bash
build/rivet-ld examples/linker/sample.ld.txt
```

O script de layout declara regiões de memória e diretivas `place` que
posicionam binários montados dentro da imagem final.

## Usar o Runtime no Próprio Código

Adicione `-Iinclude` ao seu comando de compilação, então:

```c
#include "rivet.h"

kernel_entry void kmain(void) {
    riv_log_info("kernel", "RIVET v%d.%d no ar",
                 RIVET_VERSION_MAJOR, RIVET_VERSION_MINOR);
    forever { riv_cpu_halt(); }
}
```

Compile freestanding:

```bash
gcc -std=c99 -ffreestanding -nostdlib -Iinclude -c meu_kernel.c
```

## Firmware Mínimo em RIVET

```c
#include "rivet.h"

RIV_UART_DECLARE();
RIV_TIME_DECLARE(1000);

static riv_uart_16550 uart;

kernel_entry void firmware_main(void) {
    riv_uart_16550_init(&uart, 0x10000000, 0);
    riv_uart_set_default(&uart.ops);

    riv_uart_puts("Olá do firmware RIVET\n");
    forever {
        riv_busy_wait_us(500000, 24000000);
        riv_uart_puts(".");
    }
}
```

## Conselhos Práticos

- Comece a partir de `examples/firmware/blink.c` ou
  `examples/kernel/kernel.c`.
- Use `examples/kernel/kernel_full.c` e `examples/kernel/x86_init.c`
  como referências para um layout de kernel maior com múltiplos
  subsistemas.
- Prefira compilar e testar pelo QEMU primeiro.
- Trate o alvo atual como RV32 ou x86_64 — não como "todas as
  arquiteturas".
