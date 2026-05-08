#ifndef GPIO_H
#define GPIO_H

#include "common.h"

#define PERIPHERAL_BASE          0xFE000000
#define GPFSEL_BASE              (PERIPHERAL_BASE + 0x200000)
#define GPIO_PUP_PDN_CNTRL_REG0 (PERIPHERAL_BASE + 0x2000E4)

void mmio_write(uint64_t reg, uint32_t val);
uint32_t mmio_read(uint64_t reg);
void gpio_pin_set_func(uint32_t pin, uint32_t func);
void gpio_pin_enable(uint32_t pin);

#endif
