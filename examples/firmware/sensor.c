/* RIVET demo: firmware reading an I2C sensor, exercising the new
 * bare-metal DSL keywords + peripheral abstractions.
 *
 * Compile-only: drivers are stubbed but the patterns demonstrate the
 * full module flow (init_func / device_register / mmio_reg32 / kthread). */

#include "rivet.h"

RIV_UART_DECLARE();
RIV_LOG_DECLARE();
RIV_TIME_DECLARE(1000);

/* ---- typed MMIO registers via DSL macro ----------------------------- */
mmio_reg32(CLK_CTRL, 0x40020000)
mmio_reg32(CLK_DIV,  0x40020004)

/* ---- cleanup helper used by `defer` --------------------------------- */
static void release_pin(int *p) { (void)p; /* user-supplied cleanup */ }

/* ---- I2C driver stub: real backend wires the vtable ----------------- */
static int sensor_xfer(riv_i2c_ops *self, riv_i2c_msg *msgs, int n) {
    (void)self; (void)msgs; (void)n; return 0;
}
static riv_i2c_ops sensor_bus = { "demo_i2c", (int(*)(riv_i2c_ops*,riv_u32))0,
                                  sensor_xfer };

/* ---- device descriptor + register macro ----------------------------- */
static int sensor_probe(riv_device_ops *self)  { (void)self; return 0; }
static int sensor_remove(riv_device_ops *self) { (void)self; return 0; }
static riv_device_ops sensor_dev = {
    "demo_sensor", sensor_probe, sensor_remove, (void*)0
};
device_register(sensor_dev_entry, &sensor_dev);

/* ---- background polling thread via `kthread` macro ------------------ */
kthread(sensor_task) {
    (void)_unused_ctx_;
    riv_u8 who_am_i = 0;
    if (riv_i2c_read_reg(&sensor_bus, 0x68, 0x75, &who_am_i) == 0) {
        riv_log_info("sensor", "WHO_AM_I=0x%x", who_am_i);
    }
}

/* ---- module lifecycle: registered via macros ------------------------ */
init_func void early_setup(void) {
    int dummy defer(release_pin) = 0;
    CLK_CTRL_write(CLK_CTRL_read() | 0x1);
    CLK_DIV_write(8);
    riv_dmb();
    (void)dummy;
}
module_init(early_setup);

exit_func void shutdown(void) {
    CLK_CTRL_write(0);
}
module_exit(shutdown);

kernel_entry void firmware_main(void) {
    panic_on(sizeof(riv_uptr) != sizeof(void*), "size mismatch");

    irq_safe {
        CLK_CTRL_write(CLK_CTRL_read() | 0x2);
    }

    forever {
        sensor_task((void*)0);
        riv_wfe();
    }
}
