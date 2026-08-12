# Referência dos Módulos

Este é um tour por cada cabeçalho em `include/rivet/`. Cada seção
informa o propósito do módulo, dependências e uma dica de uso em uma
linha. Para assinaturas completas, consulte o próprio cabeçalho.

## Fundação

### `arch.h`

Macros de detecção de arquitetura. Define `RIVET_ARCH_X86_64`,
`RIVET_ARCH_RISCV`, `RIVET_ARCH_AARCH64`, `RIVET_ARCH_ARM`,
`RIVET_WORD_BITS` e a string amigável `RIVET_ARCH_NAME`.

### `core.h`

Tipos de largura fixa (`riv_u8` .. `riv_u64`, `riv_uptr`, `riv_size`),
macros de atributo do compilador (`RIV_USED`, `RIV_ALWAYS`,
`RIV_PACKED`, `RIV_ALIGN`, `RIV_SECTION`, `RIV_NORETURN`, `RIV_WEAK`,
`RIV_INTERRUPT`, `RIV_LIKELY` / `RIV_UNLIKELY`) e macros de palavras-chave
da DSL (`kernel_entry`, `isr`, `naked`, `forever`).

### `mem.h`

`riv_memset` / `riv_memcpy` / `riv_memcmp` freestanding, barreiras de
memória (`riv_mb`, `riv_rmb`, `riv_wmb`) e `riv_cpu_relax()`.

### `mmio.h`

Acessores de I/O mapeado em memória em 8/16/32/64 bits, helpers RMW,
declaradores de banco de registradores (`mmio_bank`, `mmio_block`) e
helpers de bit-field (`riv_field`, `riv_field_set`).

### `io.h`

I/O por porta x86 (`riv_inb` / `riv_outb` / etc). Não declarado fora
de x86 para que o uso indevido produza erro em tempo de compilação.

### `irq.h`

Habilitar/desabilitar IRQ, halt, `riv_irq_save` / `riv_irq_restore`
por arquitetura, bloco com escopo `critical_section { ... }`, macro
`isr_vector`.

### `link.h`

Helpers de seção do linker, `riv_zero_range` e `riv_copy_range` para
limpeza de BSS e cópia de .data.

## Concorrência

### `atomic.h`

Atômicos C11 sobre builtins `__atomic_*` do GCC/Clang: load, store,
add, sub, or, and, xchg, CAS.

### `spin.h`

Spinlock test-and-set construído sobre `riv_atomic_cas`. Inclui
variante com IRQ-save e bloco com escopo `spin_locked(&lock) { ... }`.

### `waitq.h`

Fila de espera. Suspende o processo atual em uma condição; acorda um
ou todos os esperantes.

### `mutex.h`

Mutex com dormida sobre waitq. Owner-tracked.

### `sem.h`

Semáforo contador com `wait` / `trywait` / `post`.

## Estruturas de Dados

### `list.h`

Lista circular duplamente ligada intrusiva (estilo Linux). Inclui
macros de iteração, `riv_list_entry(ptr, type, member)`.

### `ring.h`

Buffer circular SPSC de bytes. Lockless, capacidade potência de dois.

### `bitmap.h`

Array de bits de largura fixa: set / clear / test / flip / ffs / ffz /
popcount.

### `hash.h`

Tabela de hash string→uptr com endereçamento aberto. FNV-1a 64; sem
tombstones.

### `slab.h`

Alocador slab de objetos de tamanho fixo sobre arena fornecida pelo
chamador.

### `heap.h`

Alocador bump mais lista livre first-fit com coalescência adjacente.

## Strings, I/O e Tempo

### `str.h`

Operações de string freestanding (`riv_strlen`, `riv_strcmp`,
`riv_strlcpy`), formatação inteiro→string e `riv_vsnprintf` /
`riv_snprintf` mínimos suportando `%d %u %x %X %p %s %c`, largura e
zero-pad.

### `log.h`

Logging nivelado sobre um sink de caracteres fornecido pelo usuário.
Níveis: ERROR, WARN, INFO, DEBUG.

### `time.h`

Contador de ticks (alimentado pelo usuário a partir de uma ISR do
SysTick), monotônico ms/us, contador de ciclos de hardware por
arquitetura (`riv_cycle_count`), `riv_busy_wait_*` e `riv_deadline_*`.

### `crc.h`

CRC-32 (IEEE 802.3 / zlib), CRC-16 (CCITT-FALSE), CRC-8 (1-Wire). Bit
a bit, sem tabelas.

### `rand.h`

PRNG xorshift64* semeado por splitmix64. Inclui helper de intervalo
estilo Lemire.

### `base64.h`

Encode e decode base64 RFC-4648. Alfabeto padrão, padding com `=`,
seguro quanto a buffer.

### `sha256.h`

Hash SHA-256 em streaming (FIPS 180-4). `init` / `update` / `final`.
Adequado para integridade de imagem; não constant-time.

## Drivers

### `uart.h`

Vtable UART mais dois drivers backend:

- 16550A (passo de registrador configurável para acesso de 8 bits ou
  32 bits)
- ARM PL011 (QEMU virt aarch64, RPi mini-UART)

Mais backend debugcon x86 escrevendo na porta 0xE9.

### `gpio.h`

Vtable genérica de GPIO: configure (direção, pull), set função
alternativa, read, write, toggle, IRQ-set.

### `i2c.h`

Vtable I2C master: setup em uma dada frequência, xfer multi-mensagem
com repeated-start, mais conveniências `read_reg` / `write_reg` /
`read_block`.

### `spi.h`

Vtable SPI master: setup (modo 0..3, ordem de bits), xfer, controle
manual de CS.

### `dma.h`

Vtable de canal DMA: start, stop, busy, restante. Direção, burst,
circular, incremento de origem/destino configuráveis.

### `wdt.h`

Vtable de watchdog: habilitar com timeout, kick, desabilitar, consulta
last-reset-was-wdt.

### `flash.h`

Vtable de flash NOR/SPI: info, read, program, erase, polling de busy.

### `adc.h`

Vtable de conversor analógico-digital mais conversão de raw para
milivolts usando a tensão de referência do dispositivo.

### `pwm.h`

Vtable de canal PWM: setup numa dada frequência, set de duty em Q16,
enable/disable, mais helper de percent-duty.

### `rtc.h`

Vtable de real-time clock: get/set de tempo quebrado em campos, mais
conversão de tempo quebrado → segundos do epoch Unix.

### `can.h`

Vtable de barramento CAN: frames clássico + CAN-FD, IDs padrão +
estendidos, flags RTR/BRS. Payload de até 64 bytes.

### `fb.h`

Framebuffer linear ARGB de 32 bpp: plot, clear, fill, glyph, puts.
Vem com um subconjunto de fonte ASCII 8×8.

### `kbd.h`

Driver de teclado PS/2 por polling (scancode set 1). Rastreia shift /
ctrl / alt / capslock; produz caracteres ASCII.

### `tty.h`

TTY com disciplina de linha envolvendo um `riv_uart_ops`. Dois modos:

- raw — bytes passam inalterados, sem eco, sem edição
- cooked — buffer de linha, backspace, eco, tradução de EOL

## CPU e Barramento

### `cpu.h`

Identificação de CPU por arquitetura: CPUID x86 (vendor, bits de
feature), ARM MIDR/MPIDR, RISC-V misa/mvendorid/marchid.

### `gdt.h` (x86_64)

Entrada GDT, descritor TSS, layout TSS de 64 bits, mais helpers `lgdt`
e `ltr`.

### `pic.h` (x86)

PIC 8259A legado: remap / mask / EOI. Inclui `pic_mask_all` para
takeover do APIC.

### `pit.h` (x86)

8253/8254 canal 0 modo 2 rate-generator setup.

### `apic.h` (x86_64)

LAPIC enable, EOI, timer periódico. Roteamento de redirect-table do
I/O APIC.

### `pci.h`

Acesso ao espaço de configuração PCI tipo-1 via portas de I/O 0xCF8 /
0xCFC, mais enumeração de barramento completa com suporte a
dispositivos multi-função.

### `acpi.h`

Percorredor mínimo de tabelas ACPI: busca de assinatura RSDP, lookup
em RSDT/XSDT por assinatura de 4 bytes, verificação de checksum.
Localiza MADT/FADT/HPET/MCFG e companhia; o parse delas fica a cargo
do chamador.

### `smp.h`

Armazenamento por-CPU (`RIV_PERCPU(type, name)` mais `riv_percpu(name)`),
contagem declarada de cores (`riv_smp_set_count` / `riv_smp_count`) e
um `riv_cpu_id()` específico por arquitetura lendo APIC ID / MPIDR /
mhartid.

## Subsistemas de SO

### `paging.h`

Gerenciador de memória física baseado em bitmap (`riv_pmm_alloc` /
`riv_pmm_free`) mais tabelas de página específicas por arquitetura:
x86_64 de 4 níveis (PML4 → PDPT → PD → PT) e RISC-V Sv32. Map /
unmap / walk / activate / TLB-flush.

### `trap.h`

Quadro de trap, tabela de dispatch, `riv_trap_set(vec, handler)`,
`riv_trap_dispatch`. Struct de entrada IDT x86_64 e loader `lidt`;
setter de mtvec RISC-V.

### `proc.h`

Bloco de controle de processo (`riv_proc`): pid, estado, contexto
salvo, stack de kernel, mapa de página, tabela de arquivos. Run
queue, scheduler round-robin, `riv_proc_alloc` / `_ready` / `_yield`
/ `_exit` / `_current` / `riv_sched_tick_irq`.

### `syscall.h`

Constantes de número de syscall (layout Linux x86_64), tabela de
dispatch, `riv_syscall_register`, `riv_syscall_dispatch` e um wrapper
inline `riv_syscall(nr, ...)` no lado do usuário em x86_64.

### `signal.h`

Sinais estilo POSIX: tabela de handlers, raise, mask, dispatch no
retorno de syscall.

### `pipe.h`

IPC de pipe de bytes com bloqueio, ancorado em buffer circular mais
duas filas de espera.

### `vfs.h`

Sistema de arquivos virtual: struct de inode, vtable de operações de
arquivo, tabela de mount, tabela de fd por processo, `open` / `close`
/ `read` / `write` / `lseek`.

### `ramfs.h`

Filesystem em RAM com pool fixo de 32 inodes × 4 KiB cada. Plugável
no VFS.

### `elf.h`

Tipos de cabeçalho ELF64 + callback de carregador de programa.
Percorre segmentos PT_LOAD e chama `place(vaddr, data, filesz, memsz,
flags, ctx)` fornecido pelo usuário.

### `disk.h`

Vtable de dispositivo de bloco: read LBA, write LBA, flush.

### `fat.h`

Driver somente leitura FAT12 / FAT16 / FAT32. Auto-detecta tipo a
partir do BPB.

### `dt.h`

Leitor de flat device tree (FDT). Validação de magic, helpers BE32/64,
callback de walk de nó.

### `buf.h`

Camada de cache de blocos sobre `riv_disk_ops`. Mantém 32 blocos de
512 bytes com eviction clock-sweep e write-back de blocos sujos via
`riv_buf_sync`.

### `net.h`

Structs packed de cabeçalho Ethernet, IPv4, UDP e ICMP. Helpers de
byte-order (`riv_htons` / `riv_htonl`), checksum IPv4 de complemento de
um, construtor de IP em quad-pontilhado, vtable genérica `riv_netif`.

### `arp.h`

Struct de cabeçalho ARP, cache ARP de 16 entradas com eviction LRU e
um helper que monta o frame de 42 bytes de broadcast de request
completo.

## Runtime

### `boot.h`

Helpers de símbolo do linker: `riv_boot_clear_bss`,
`riv_boot_copy_data`, `riv_boot_run_ctors`, mais o guarda-chuva
`riv_boot_runtime_init`.

### `panic.h`

Pânico / assert / halt. Handler sobrescrevível pelo usuário;
implementação padrão (sob `RIVET_PANIC_IMPL`) roteia uma mensagem
formatada para o UART padrão mais porta debug x86 0xE9, depois para
infinitamente.

### `task.h`

Scheduler de tarefas cooperativo dirigido por tick: entradas
`riv_task` com período, `riv_sched_run` em super-loop,
`riv_sched_tick` em ISR do SysTick.

### `bare.h`

Extensões da DSL com sabor bare-metal: atributos (`RIV_MUST_CHECK`,
`RIV_HOT`, `RIV_COLD`, etc.), palavras-chave de seção (`init_func`,
`exit_func`, `early_func`, `place_at`), blocos com escopo
(`irq_safe`, `defer`), intrínsecos de instrução (`riv_wfe`, `riv_dmb`,
etc.), helpers em tempo de compilação (`panic_on`,
`static_assert_size`), macros de registrador tipado (`mmio_reg32` /
16 / 8) e as macros de registro de thread de kernel / módulo /
dispositivo.
