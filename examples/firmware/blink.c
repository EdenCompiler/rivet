/* RIVET example: STM32-style GPIO blink firmware
 * Demonstrates mmio, task scheduler, critical sections.
 * Adjust addresses for your MCU.
 */
#include "rivet.h"

/* fake STM32F4 GPIOA layout */
#define GPIOA_BASE   0x40020000UL
#define RCC_AHB1ENR  0x40023830UL

mmio_bank(stm32_gpio_t,
    riv_u32 MODER;
    riv_u32 OTYPER;
    riv_u32 OSPEEDR;
    riv_u32 PUPDR;
    riv_u32 IDR;
    riv_u32 ODR;
    riv_u32 BSRR;
    riv_u32 LCKR;
    riv_u32 AFRL;
    riv_u32 AFRH;
);

#define GPIOA (*(stm32_gpio_t*)GPIOA_BASE)

static riv_u32 led_pin = 5; /* PA5 */

static void task_blink(void *ctx) {
    riv_u32 pin = *(riv_u32*)ctx;
    GPIOA.ODR ^= RIV_BIT(pin);
}

static void task_heartbeat(void *ctx) {
    (void)ctx;
    critical_section {
        /* atomic counter bump */
        static volatile riv_u32 beats;
        beats++;
    }
}

static riv_task tasks[] = {
    task_entry(task_blink,     &led_pin, 500),
    task_entry(task_heartbeat, RIV_NULL,  1000),
};
static riv_scheduler sched;

kernel_entry void firmware_main(void) {
    /* enable GPIOA clock */
    riv_mmio_setbits32(RCC_AHB1ENR, RIV_BIT(0));

    /* PA5 as output (MODER bits 11:10 = 01) */
    riv_mmio_rmw32((riv_uptr)&GPIOA.MODER,
                  riv_field_set(0x3u, 11, 10),
                  riv_field_set(0x1u, 11, 10));

    riv_sched_init(&sched, tasks, RIV_ARRLEN(tasks));
    riv_irq_enable();

    forever {
        riv_sched_run(&sched);
        riv_cpu_relax();
    }
}

/* SysTick ISR drives scheduler */
isr void SysTick_Handler(void) {
    riv_sched_tick(&sched);
}
