#include "gpio.h"

void mmio_write(uint64_t reg, uint32_t val) {
    *(volatile uint32_t *)reg = val;
}

uint32_t mmio_read(uint64_t reg) {
    return *(volatile uint32_t *)reg;
}

void gpio_pin_set_func(uint32_t pin, uint32_t func) {
    uint64_t reg = GPFSEL_BASE + (pin / 10) * 4;
    uint32_t shift = (pin % 10) * 3;
    uint32_t val = mmio_read(reg);
    val &= ~(0x7 << shift);
    val |= (func << shift);
    mmio_write(reg, val);
}

void gpio_pin_enable(uint32_t pin) {
    uint64_t reg = GPIO_PUP_PDN_CNTRL_REG0 + (pin / 16) * 4;
    uint32_t shift = (pin % 16) * 2;
    uint32_t val = mmio_read(reg);
    val &= ~(0x3 << shift);
    mmio_write(reg, val);
}
