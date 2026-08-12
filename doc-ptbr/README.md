# Documentação RIVET

RIVET é um framework em C99 para construção de sistemas operacionais e
firmwares bare-metal. A documentação descreve como compilar imagens,
atacar arquiteturas específicas e estender o runtime com novos drivers
e subsistemas.

## Capacidades Principais

O framework atualmente é especializado nas arquiteturas RISC-V 32-bit e
x86_64, com módulos e demonstrações cobrindo stubs de boot, paginação,
interrupções, processos, syscalls, VFS, ramfs, FAT, ELF, PCI, ACPI,
SMP, framebuffer, UART serial, teclado PS/2, estruturas de cabeçalho
Ethernet/IPv4/UDP, cache ARP, cache de blocos, disciplina de linha
TTY, SHA-256, base64 e uma seleção ampla de primitivas de CPU e
periféricos (GPIO, I²C, SPI, DMA, WDT, flash, ADC, PWM, RTC, CAN).

## Estrutura da Documentação

O guia oferece seis seções principais começando com Primeiros Passos,
seguido por Conceitos Fundamentais e a referência da DSL. Ferramentas,
módulos e um tour pelos exemplos completam o percurso de aprendizado,
projetado para ser seguido sequencialmente para melhor compreensão.

- [Primeiros Passos](getting-started.md)
- [Conceitos Fundamentais](core-concepts.md)
- [Referência da DSL](dsl-reference.md)
- [Referência dos Módulos](modules-reference.md)
- [Referência das Ferramentas](tools-reference.md)
- [Guia dos Exemplos](examples-guide.md)

## Limitações Atuais

Como mencionado no README raiz, "RIVET ainda não é uma plataforma de SO
de propósito geral multi-arquitetura para produção". O assembler emite
apenas binários flat, as arquiteturas suportadas são RV32 e x86_64, e o
runtime é header-only. Adicionar novas arquiteturas exige escrever dois
arquivos de backend em C dentro de `src/arch/` e atualizar o cabeçalho
guarda-chuva.
