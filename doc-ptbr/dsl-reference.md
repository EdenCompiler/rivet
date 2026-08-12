# Referência da DSL

Esta é uma referência prática da DSL de macros C do RIVET. O foco é
significado e uso, não uma gramática formal.

## Lendo a DSL Corretamente

Dentro de um arquivo fonte RIVET, a maioria das macros não são
expressões C comuns. São construções que:

- emitem uma ou mais funções inline
- marcam funções para colocação em seções de linker nomeadas
- declaram registradores MMIO tipados
- registram dispositivos ou módulos para enumeração
- influenciam o layout das seções

Se uma macro não é reconhecida como uma forma da DSL RIVET, ela cai
para o preprocessador C como um identificador normal.

## Formas de Nível Superior

### `kernel_entry`

Anotação que marca uma função como ponto de entrada do kernel/firmware.

```c
kernel_entry void kmain(void) {
    ...
}
```

Expande para `RIV_USED RIV_SECTION(".text.entry")`, então o linker
mantém a função e a coloca na seção de entrada.

### `init_func` / `exit_func` / `early_func`

Anotações de seção para funções de tempo de boot e desligamento.

```c
init_func void setup_clocks(void) {
    ...
}
```

Cada uma coloca a função em `.init.text`, `.exit.text` ou
`.text.early`, então um stub de boot pode percorrer e executá-las em
ordem.

### `module_init` / `module_exit`

Ciclo de vida de módulo estilo Linux: coloca um ponteiro de função em
`.init.fns` ou `.exit.fns`.

```c
init_func void my_init(void) { ... }
module_init(my_init);
```

### `device_register`

Coloca um `riv_device_ops *` na seção `.dev.tbl` para que um
enumerador genérico possa fazer probe de cada dispositivo no boot.

```c
static riv_device_ops my_dev = { "my_dev", probe, remove, NULL };
device_register(my_dev_entry, &my_dev);
```

O argumento `tag` é um identificador arbitrário usado para nomear o
símbolo de colocação; só precisa ser único na unidade de tradução.

## Formas Estruturais Centrais

### `forever`

Laço infinito.

```c
forever {
    riv_cpu_halt();
}
```

### `naked`

Atributo de função para stubs de boot e ISRs que não devem ter prólogo
ou epílogo C.

```c
naked void _start(void) { ... }
```

### `packed_struct`

Equivalente a `struct __attribute__((packed))`.

```c
packed_struct mbr_part {
    riv_u8 status;
    riv_u8 chs_start[3];
    ...
};
```

### `unreachable()`

Dica do compilador de que a chamada não pode ser alcançada.

```c
forever { riv_cpu_halt(); }
unreachable();
```

## Blocos com Escopo

### `irq_safe { ... }` / `critical_section { ... }`

Desabilita interrupções pela duração do bloco, restaurando o estado
anterior na saída.

```c
irq_safe {
    shared_counter++;
}
```

### `spin_locked(&lock) { ... }`

Adquire um spinlock (com IRQs salvos) pela duração do bloco.

```c
spin_locked(&my_lock) {
    push_to_list(&my_list, node);
}
```

### `defer(fn)`

Wrapper para `__attribute__((cleanup))` do GCC. A função de cleanup
nomeada é chamada quando a variável anotada sai do escopo.

```c
static void release_lock(riv_spin **p) { riv_spin_unlock(*p); }

riv_spin *l defer(release_lock) = &my_lock;
riv_spin_lock(l);
/* liberação automática no fim do bloco */
```

## Formas de Dados e Layout

### `mmio_reg32(name, addr)` / `mmio_reg16` / `mmio_reg8`

Declara um registrador MMIO tipado e emite helpers inline `name_read()`
e `name_write(v)`.

```c
mmio_reg32(CLK_CTRL, 0x40020000)

CLK_CTRL_write(CLK_CTRL_read() | 0x1);
```

### `mmio_bank(typename, fields...)`

Declara uma `volatile struct` packed descrevendo um banco de
registradores, com uma asserção em tempo de compilação de tamanho
alinhado.

```c
mmio_bank(stm32_gpio_t,
    riv_u32 MODER;
    riv_u32 OTYPER;
    riv_u32 OSPEEDR;
);

#define GPIOA (*(stm32_gpio_t*)0x40020000)
```

### `place_at(addr)` / `aligned_as(n)`

Helpers de posicionamento para o linker.

```c
place_at(0x10000) static const riv_u32 fixed_table[] = { ... };
aligned_as(64) static riv_u8 buffer[256];
```

### `persistent_data` / `no_init_data`

Anotações de seção para BSS não zerado ou armazenamento persistente.

## Threads e Tarefas

### `kthread(name) { body }`

Declara uma função de thread de kernel. Recebe um argumento de
contexto não usado na assinatura para compatibilidade com o scheduler
de tarefas.

```c
kthread(sensor_task) {
    riv_u8 v;
    if (riv_i2c_read_reg(&bus, 0x68, 0x75, &v) == 0) {
        riv_log_info("sensor", "WHO_AM_I=0x%x", v);
    }
}
```

### `register_kthread(fn, period)`

Macro que produz uma entrada `riv_task` inicializada para o scheduler
cooperativo.

```c
static riv_task tasks[] = {
    register_kthread(sensor_task, 500),
    register_kthread(blink_task,  250),
};
```

## Formas para Rotinas de Serviço de Interrupção

### `isr`

Anota uma função como ISR. Expande apenas para `RIV_USED` — o atributo
`interrupt` é intencionalmente opt-in via `isr_attr` porque assinaturas
exigidas diferem por arquitetura.

```c
isr void SysTick_Handler(void) {
    riv_tick();
}
```

### `isr_vector(name)`

Declara uma tabela de vetores.

```c
isr_vector(vectors) = {
    reset_handler,
    nmi_handler,
    hardfault_handler,
    ...
};
```

## Intrínsecos de Instrução

Wrappers de inline-asm expostos sob nomes curtos e neutros à
arquitetura:

- `riv_pause()` — `pause` / `yield` / `nop` por arquitetura
- `riv_wfe()` — espera-por-evento (ou halt fora de ARM)
- `riv_sev()` — envia-evento (somente ARM; no-op em outros)
- `riv_dmb()` — barreira de memória de dados
- `riv_dsb()` — barreira de sincronização de dados
- `riv_isb()` — barreira de sincronização de instrução
- `riv_irq_enable()` / `riv_irq_disable()` / `riv_cpu_halt()`
- `riv_irq_save()` / `riv_irq_restore(state)`
- `riv_compiler_barrier()` — sem instrução, apenas clobber de memória

## Diagnósticos

### `panic_on(cond, msg)`

```c
panic_on(!ptr, "ponteiro nulo no init");
```

### `riv_assert(cond)`

```c
riv_assert(sizeof(riv_uptr) == sizeof(void*));
```

### `static_assert_size(type, n)`

```c
static_assert_size(riv_idt_entry, 16);
```

### `riv_panic(msg)`

Chama o handler de pânico registrado. O handler padrão (quando
`RIVET_PANIC_IMPL` está definido) formata a mensagem e a escreve para
o UART registrado mais a porta debugcon x86 0xE9, depois para
infinitamente.

## Atributos de Decoração da DSL

- `RIV_MUST_CHECK` — aviso se o valor de retorno for ignorado
- `RIV_PURE` / `RIV_CONST` — anotações de pureza
- `RIV_HOT` / `RIV_COLD` — dicas de colocação
- `RIV_PRINTF_LIKE(fmt, va)` — checagem de formato printf

## Interoperabilidade com o Assembler

Para trabalho bare-metal específico de arquitetura que o runtime ainda
não abstrai, use assembly inline estendido do GCC:

```c
__asm__ __volatile__("invlpg (%0)" :: "r"(vaddr) : "memory");
```

### Boas Práticas

- use macros da DSL quando elas descrevem claramente a ação que você
  quer
- use assembly inline quando precisar de seleção exata de instrução
  ou comportamento de caso especial
- mantenha trocas de modo, setup de stack e premissas de máquina
  explícitas

## Um Pequeno Exemplo Real

```c
#include "rivet.h"

RIV_UART_DECLARE();
RIV_TIME_DECLARE(1000);

static riv_uart_16550 uart;

mmio_reg32(LED_CTRL, 0x40010000)

kthread(blink) {
    LED_CTRL_write(LED_CTRL_read() ^ 1);
}

init_func void board_init(void) {
    riv_uart_16550_init(&uart, 0x10000000, 0);
    riv_uart_set_default(&uart.ops);
}
module_init(board_init);

kernel_entry void firmware_main(void) {
    riv_uart_puts("firmware RIVET inicializou\n");
    forever {
        irq_safe { blink(NULL); }
        riv_wfe();
    }
}
```

Isso mostra as três camadas principais trabalhando juntas:

- entrada e ciclo de vida em nível de módulo (`kernel_entry`,
  `module_init`)
- corpos por thread (`kthread`)
- acesso direto a hardware (`mmio_reg32` + intrínsecos)
