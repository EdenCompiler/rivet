# Conceitos Fundamentais

## Tipos de Arquivo

### `.h` (cabeçalho)

Módulos do runtime RIVET.

Cada cabeçalho é autocontido e vive em `include/rivet/`. O cabeçalho
guarda-chuva `include/rivet.h` traz todos eles, mas módulos também
podem ser incluídos individualmente.

### `.c` (fonte)

Código do usuário: firmware, kernels, drivers.

Os exemplos ficam em `examples/firmware/` e `examples/kernel/`. Eles
incluem `rivet.h` (ou um módulo específico) e consomem o runtime como
uma biblioteca somente de cabeçalhos.

### `.s` (assembly)

Arquivos fonte para o assembler RIVET, `rivet-as`.

A arquitetura é selecionada via `-m riscv32` ou `-m x86_64`. O assembly
RIVET é uma sintaxe simples, separada por vírgulas, no estilo GAS, com
rótulos, diretivas e mnemônicos. Pseudo-operações como `nop`, `mv`,
`li`, `j` e `ret` são expandidas no momento do parse.

### `.ld.txt` (script de linker)

Script de layout consumido por `rivet-ld`.

Declara regiões de memória e diretivas `place` que posicionam binários
montados dentro da imagem final. A saída é um único `.bin` flat.

## Modelo de Imagem

Uma imagem RIVET é normalmente composta por:

- um binário de boot (montado de um `.s`)
- um ou mais binários de kernel ou dados

As regiões possuem:

- um nome
- uma origem (endereço de carga)
- um comprimento
- um byte de preenchimento opcional

O stub de boot tipicamente faz a inicialização inicial, carrega as
seções posteriores da flash ou do disco, e transfere o controle para o
ponto de entrada do kernel.

Existe uma separação importante entre:

- estrutura em nível de imagem, que decide quais regiões existem e
  onde elas vivem na memória (lidado por `rivet-ld`)
- estrutura em nível de seção, que decide quais bytes são emitidos
  dentro de cada arquivo de entrada (lidado por `rivet-as` para
  assembly e pelo toolchain C do usuário para código C)

No nível da imagem, `region`, `place`, `entry` e `define` são os
blocos principais do script de layout. No nível da seção, formas como
`.text`, `.data`, `.word`, `.byte`, `.asciz`, `.align` e instruções
puras definem o conteúdo real.

## Modelo de Execução

RIVET atualmente enfatiza duas arquiteturas:

- RISC-V 32-bit: base inteira RV32I mais a extensão M (multiplicação /
  divisão) e as operações de ordenação de memória `fence` / `fence.i`
- x86_64: modo longo com prefixos REX, endereçamento ModR/M e SIB
  completo, imediatos, branches REL32, traps acionadas por IDT,
  interrupções PIC/APIC e paginação acionada por CR3

O runtime distribuído detecta a arquitetura atual em `arch.h` e expõe
primitivas por arquitetura (salvar/restaurar IRQ, contador de ciclos,
operações de tabela de página) através de uma única API em C.

O assembler portanto trabalha com dois modelos de codificação ao mesmo
tempo:

- instruções RV32 de largura fixa de 32 bits
- instruções x86_64 de largura variável com REX opcional, ModR/M, SIB
  e imediatos

Formas como `.text`, `.org` e `.align` funcionam identicamente nas
duas. Sintaxes de mnemônico e operando são específicas por arquitetura
e vivem no backend `src/arch/*_as.c` correspondente.

## Filosofia da DSL

A DSL do runtime é sintaxe C sobre trabalho de baixo nível. Ela tenta
oferecer:

- controle direto sobre layout e instruções
- helpers reusáveis amparados por arquitetura
- formas explícitas para coisas como GDT, IDT, paginação, IRQs, MMIO,
  atômicos, spinlocks, processos, syscalls, VFS e dispositivos

Onde a DSL não abstrai algo, você ainda pode recorrer a assembly inline
via `__asm__ __volatile__`.

Na prática, RIVET se comporta como uma biblioteca em camadas:

- algumas macros são substituições puramente em tempo de compilação
  (`kernel_entry`, `irq_safe`, `forever`, `mmio_reg32`)
- algumas funções inline emitem uma única instrução (`riv_wfi`,
  `riv_dmb`, `riv_cpu_relax`, `riv_irq_save`)
- algumas funções implementam pequenos pedaços de política (alocadores,
  schedulers, percorredores de tabela de página)
- alguns cabeçalhos são vtables esperando por um driver (gpio, i2c,
  spi, dma, uart, disk, flash)

Essa última regra é o que torna o framework prático para trabalho de
SO: você não fica preso quando a camada de alto nível ainda não tem um
driver dedicado.

## Pipeline de Compilação

O caminho normal de fonte até imagem é:

1. `make` invoca o compilador do host nas fontes em `src/`.
2. O assembler é compilado uma vez com ambos os backends linkados.
3. Cada fonte `.s` é montada para um `.bin` flat em `bin/`.
4. Fontes em C nos `examples/` são verificadas contra o runtime.
5. `rivet-ld` pode ser invocado para combinar binários em uma imagem
   final.
6. `rivet-img` pode preencher e adicionar trailer CRC à imagem final.
7. `rivet-objcopy` pode converter a imagem final para Intel HEX ou
   SREC.

Por isso a ordem importa dentro de scripts de layout, e por isso
referências baseadas em rótulo funcionam mesmo quando o rótulo é
definido mais tarde no arquivo.

## Tempo de Compilação Versus Tempo de Execução

Dentro de fontes RIVET, o C comum ainda existe. Você pode definir
macros auxiliares, constantes ou incluir arquivos no tempo do
preprocessador.

Mas macros do `bare.h` e companhia devem ser lidas como formas de
construção de imagem:

- `const` (`#define`) é uma substituição em tempo de compilação
- `mmio_reg32(name, addr)` emite funções inline tipadas de
  leitura/escrita
- `kthread(name) { body }` declara uma função de thread de kernel
- `init_func` / `exit_func` colocam uma função numa seção nomeada do
  linker
- `module_init(fn)` / `module_exit(fn)` colocam um ponteiro numa
  tabela
- `device_register(tag, ops)` registra um dispositivo para enumeração

Se você precisa de estado em tempo de execução, armazene em dados
emitidos, não em expansões de macro.

## Controle Estruturado

RIVET já fornece o controle de fluxo C comum. A DSL também expõe
formas estruturadas para idiomas bare-metal:

- `forever { ... }`
- `irq_safe { ... }`
- `critical_section { ... }`
- `spin_locked(&lock) { ... }`
- `defer(cleanup_fn)`

A semântica é intencionalmente simples:

- `forever` emite um laço infinito
- `irq_safe` e `critical_section` desabilitam interrupções pela
  duração do bloco, restaurando o estado anterior na saída
- `spin_locked` adquire um spinlock pela duração do bloco
- `defer` registra uma função de limpeza chamada quando a variável
  sai do escopo

Elas são úteis quando você quer a legibilidade do fluxo estruturado
sem abrir mão do controle exato orientado à máquina.

## Escopo Atual de Hardware

RIVET não é uma camada universal de abstração de hardware. Os módulos
atuais assumem:

- RV32I + M para alvos RISC-V
- modo longo x86_64 com codificações prefixadas por REX
- PIC 8259A legado, PIT 8253, UART 16550A, UART PL011 e teclado PS/2
  em plataformas x86
- acesso a espaço de configuração PCI tipo-1 padrão nas portas de I/O
  0xCF8 / 0xCFC

Isso significa que o framework é bem adequado para desenvolvimento de
SO acionado por emulador, firmware para microcontroladores RV32 e
kernels x86_64 amadores.
