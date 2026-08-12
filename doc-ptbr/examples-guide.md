# Guia dos Exemplos

A árvore `examples/` traz demonstrações prontas para compilar de cada
parte importante do framework. Estão organizadas por tipo:

```
examples/
  asm/
    riscv32/     exemplos em assembly RV32
    x86_64/      exemplos em assembly x86_64
  firmware/     demos em C estilo firmware
  kernel/       demos em C estilo kernel
  linker/       scripts de layout para rivet-ld
```

Cada subseção abaixo resume o que cada arquivo ensina e o que ler em
seguida.

## Assembly

### `asm/riscv32/boot.s`

Uma bootrom RV32I mínima. Calcula 1+2+…+10 em `a0`, faz halt com
`ecall`. Exercita:

- declaração de constante `.equ`
- seção `.text` + rótulos
- instruções `li`, `bge`, `add`, `addi`, `j`, `mv`, `ecall`

Bom ponto de partida para entender a resolução de rótulos em duas
passagens do assembler.

### `asm/riscv32/features.s`

Testa a extensão de multiplicação / divisão RV32M mais `fence` e as
diretivas de string. Exercita:

- `mul`, `div`, `rem`
- `fence rw, rw`, `fence.i`
- `.align`, `.asciz "RIVET\n"`, `.zero 8, 0xAA`

### `asm/x86_64/hello.s`

Demo da ABI de syscall Linux x86_64: escreve "hello from x86_64" no fd
1 via SYS_write, depois sai via SYS_exit. Exercita:

- `mov reg, imm32`
- instrução `syscall`
- `.align`, `.asciz`, `.equ`

### `asm/x86_64/branch.s`

Demo de loop / call / push-pop / inc-dec. Exercita:

- `cmp reg, imm32` + `jne rel32`
- `call rel32` para rótulo declarado adiante
- `push reg` / `pop reg`
- `inc` / `dec`

### `asm/x86_64/memops.s`

Exercício abrangente de operandos de memória. Testa cada forma de
endereçamento ModR/M + SIB que o assembler suporta:

- `[rbx]`, `[rbx+8]`, `[rbx+0x1000]`
- `[rsp]` (base forçada via SIB)
- `[rbp]` (disp8=0 forçado)
- `[r13+16]` (base de registrador alto via REX.B)
- `[rbx+rcx*4]`, `[rbx+rcx*8+32]`
- `lea rax, [rbx+64]`
- `mov [mem], reg` e `xor reg, [mem]`

## Firmware

### `firmware/blink.c`

Firmware de blink GPIO estilo STM32. Demonstra:

- declaração de bloco de registradores `mmio_bank`
- `riv_mmio_setbits32` / `riv_mmio_rmw32`
- bloco com escopo `critical_section { }`
- o `riv_scheduler` cooperativo com duas tarefas periódicas
- palavras-chave de posicionamento `kernel_entry` e `isr`

### `firmware/sensor.c`

Firmware multi-driver exercitando a nova DSL bare-metal. Demonstra:

- declaração de registrador tipado `mmio_reg32`
- seções `init_func` / `exit_func`
- tabela de ciclo de vida `module_init` / `module_exit`
- tabela de enumeração de dispositivo `device_register`
- declaração de thread `kthread`
- limpeza automática `defer`
- bloco `irq_safe` + `riv_wfe()` + `panic_on`
- interação I2C master via vtable genérica `riv_i2c_ops`

## Kernel

### `kernel/kernel.c`

Kernel x86_64 minúsculo que escreve no framebuffer de texto VGA em
0xB8000 e sobrescreve `riv_panic_handler`. Demonstra:

- `kernel_entry` + `RIV_NORETURN`
- Limpeza de BSS via símbolo de linker e `riv_zero_range`
- Handler de pânico customizado
- Checagem de runtime `riv_assert`

### `kernel/panic_test.c`

Exercita o handler de pânico padrão quando `RIVET_PANIC_IMPL` está
definido. Demonstra:

- `RIV_UART_DECLARE`, `RIV_LOG_DECLARE`, `RIV_TIME_DECLARE`
- Backend UART debugcon x86 (`riv_uart_debugcon_init`)
- `riv_log_set_sink` + logging nivelado
- Checagem de sanidade do contador de ciclos de hardware
  (`riv_cycle_count`)
- Disparo do handler de pânico padrão

### `kernel/kernel_full.c`

Smoke test apenas de compilação de cada subsistema de SO importante
trabalhando junto. Demonstra:

- PMM (`riv_pmm_init`)
- Ramfs (`riv_ramfs_init`) montado em `/`
- Registro de tabela de syscall (read, write, open, getpid, exit,
  yield)
- Handlers de trap para page fault e timer
- Criação de processo (`riv_proc_alloc`, `riv_proc_ready`)
- Tabela de fd por processo
- Stub de gancho de troca de contexto

### `kernel/x86_init.c`

Init inicial x86_64 exercitando infraestrutura legada + moderna.
Demonstra:

- Setup de GDT + TSS com `riv_gdt_set` / `riv_gdt_set_tss`
- Remap do PIC 8259 + mask-all
- PIT 8253 a 100 Hz
- Enumeração de barramento PCI com callback
- Init de mutex / semáforo / pipe / hash / rand
- Carregador de programa ELF com callback de colocação

## Linker

### `linker/sample.ld.txt`

Layout de duas regiões: 4 KiB de ROM em `0x80000000` mais 16 KiB de
RAM em `0x80100000`. Coloca `boot.bin` na ROM, declara um símbolo de
entrada e produz um único binário de imagem.

## Ordem de Leitura Recomendada

1. `firmware/blink.c` — básicos da DSL + scheduler
2. `asm/riscv32/boot.s` — básicos do assembler
3. `asm/x86_64/hello.s` — formas de registrador/imediato x86_64
4. `asm/x86_64/memops.s` — codificação completa ModR/M + SIB
5. `kernel/kernel.c` — primeira entrada real de kernel
6. `kernel/panic_test.c` — serviços de runtime (uart, log, time)
7. `firmware/sensor.c` — DSL bare-metal completa
8. `kernel/kernel_full.c` — subsistemas de SO interligados
9. `kernel/x86_init.c` — infraestrutura de boot x86_64
10. `linker/sample.ld.txt` + `make` — pipeline de build completo
