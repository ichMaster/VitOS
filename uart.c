#include "gpio.h"
#include "uart.h"

#define AUX_BASE         (PERIPHERAL_BASE + 0x215000)
#define AUX_ENABLES      (AUX_BASE + 0x04)
#define AUX_MU_IO_REG    (AUX_BASE + 0x40)
#define AUX_MU_IER_REG   (AUX_BASE + 0x44)
#define AUX_MU_IIR_REG   (AUX_BASE + 0x48)
#define AUX_MU_LCR_REG   (AUX_BASE + 0x4C)
#define AUX_MU_MCR_REG   (AUX_BASE + 0x50)
#define AUX_MU_LSR_REG   (AUX_BASE + 0x54)
#define AUX_MU_CNTL_REG  (AUX_BASE + 0x60)
#define AUX_MU_BAUD_REG  (AUX_BASE + 0x68)

void uart_init(void) {
    mmio_write(AUX_ENABLES, 1);
    mmio_write(AUX_MU_CNTL_REG, 0);
    mmio_write(AUX_MU_IER_REG, 0);
    mmio_write(AUX_MU_LCR_REG, 3);
    mmio_write(AUX_MU_MCR_REG, 0);
    mmio_write(AUX_MU_IIR_REG, 0xC6);
    mmio_write(AUX_MU_BAUD_REG, 541);

    gpio_pin_set_func(14, 2);
    gpio_pin_set_func(15, 2);
    gpio_pin_enable(14);
    gpio_pin_enable(15);

    mmio_write(AUX_MU_CNTL_REG, 3);
}

void uart_send(unsigned int c) {
    while (!(mmio_read(AUX_MU_LSR_REG) & 0x20))
        ;
    mmio_write(AUX_MU_IO_REG, c);
}

char uart_getc(void) {
    while (!(mmio_read(AUX_MU_LSR_REG) & 0x01))
        ;
    return mmio_read(AUX_MU_IO_REG) & 0xFF;
}

void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n')
            uart_send('\r');
        uart_send(*s++);
    }
}

void uart_hex(unsigned int val) {
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        unsigned int nibble = (val >> i) & 0xF;
        uart_send(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
    }
}
