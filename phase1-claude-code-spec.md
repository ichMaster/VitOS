# VitOS Phase 1 \-- Claude Code Implementation Spec

## Project Goal

Create a bare-metal "Hello World" OS for Raspberry Pi 4 that boots into AArch64 mode, initializes the Mini UART, prints a boot banner over serial, and enters an interactive echo loop. This validates the complete toolchain, boot sequence, and MMIO access pattern.

## Target Hardware

- Raspberry Pi 4 Model B  
- SoC: BCM2711, CPU: ARM Cortex-A72 (ARMv8-A / AArch64)  
- Peripheral base address: `0xFE000000`  
- Mini UART registers base: `0xFE215000`

## Prerequisites

The following cross-compiler toolchain must be installed on the host machine before running any build commands:

\# Ubuntu/Debian

sudo apt install gcc-aarch64-none-elf binutils-aarch64-none-elf make

\# Or download from ARM directly:

\# https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads

\# Use the "aarch64-none-elf" (bare-metal) variant

Verify installation:

aarch64-none-elf-gcc \--version

aarch64-none-elf-objcopy \--version

## Project Structure

Create the following directory layout:

vitos/

  boot.S              \# AArch64 assembly boot stub

  gpio.h              \# GPIO register definitions and helpers

  gpio.c              \# GPIO MMIO implementation

  uart.h              \# UART function declarations

  uart.c              \# Mini UART driver

  kernel.c            \# Kernel entry point (kernel\_main)

  common.h            \# Shared type definitions

  link.ld             \# Linker script

  Makefile            \# Build system

  config.txt          \# Pi 4 boot configuration

  README.md           \# Project documentation

## File Specifications

### common.h

Provide minimal type definitions without relying on standard library:

\#ifndef COMMON\_H

\#define COMMON\_H

typedef unsigned char       uint8\_t;

typedef unsigned short      uint16\_t;

typedef unsigned int        uint32\_t;

typedef unsigned long       uint64\_t;

typedef signed int          int32\_t;

typedef signed long         int64\_t;

typedef uint64\_t            size\_t;

\#define NULL ((void \*)0)

\#endif

NOTE: If the toolchain provides `<stdint.h>` in freestanding mode (GCC does), you may use it instead. Freestanding C guarantees `<stdint.h>`, `<stddef.h>`, `<limits.h>`, `<float.h>`, and `<stdarg.h>`.

### link.ld \-- Linker Script

Requirements:

- Entry point starts at address `0x80000` (where GPU loads kernel8.img)  
- `.text.boot` section must come first (contains assembly entry point)  
- Sections in order: `.text.boot`, `.text`, `.rodata`, `.data`, `.bss`  
- BSS section marked NOLOAD with `__bss_start` and `__bss_end` symbols exported  
- BSS aligned to 16 bytes (AArch64 stack alignment requirement)  
- Discard `.comment`, `.gnu*`, `.note*`, `.eh_frame*` sections

### boot.S \-- Assembly Boot Stub

Section: `.text.boot` Global symbol: `_start`

Logic:

1. Read processor ID from `mpidr_el1` system register, mask bottom 2 bits  
2. If core ID is not 0, enter WFE (Wait For Event) infinite loop (park the core)  
3. Core 0 continues: a. Set stack pointer to `_start` (0x80000), stack grows downward b. Zero the BSS section using `__bss_start` and `__bss_end` symbols, 8 bytes at a time c. Branch-and-link to `kernel_main` (C function) d. If `kernel_main` returns, branch to the WFE parking loop

Important: Do NOT attempt to drop from EL2 to EL1 in Phase 1\. That comes in Phase 3\. The Mini UART works fine from EL2.

### gpio.h / gpio.c \-- GPIO Driver

Define peripheral base: `0xFE000000`

Implement these functions:

- `void mmio_write(uint64_t reg, uint32_t val)` \-- volatile write to MMIO register  
- `uint32_t mmio_read(uint64_t reg)` \-- volatile read from MMIO register  
- `void gpio_pin_set_func(uint32_t pin, uint32_t func)` \-- set GPIO pin alternate function  
  - Each GPFSEL register covers 10 pins, 3 bits per pin  
  - GPFSEL base: peripheral\_base \+ 0x200000  
  - Register offset: `(pin / 10) * 4`  
  - Bit offset: `(pin % 10) * 3`  
  - Read-modify-write: clear 3 bits, set new function  
- `void gpio_pin_enable(uint32_t pin)` \-- disable pull-up/pull-down on a pin  
  - Pi 4 uses `GPIO_PUP_PDN_CNTRL_REG0` at peripheral\_base \+ 0x2000E4  
  - NOT the older GPPUD/GPPUDCLK mechanism from Pi 3 tutorials  
  - Each pin uses 2 bits: 00 \= no pull, 01 \= pull-up, 10 \= pull-down  
  - Register offset: `(pin / 16) * 4`  
  - Bit offset: `(pin % 16) * 2`

CRITICAL: Many Pi 3 tutorials use the GPPUD \+ GPPUDCLK0 \+ 150-cycle-delay sequence. This does NOT work on Pi 4\. The BCM2711 replaced it with direct pull-up/down control registers.

### uart.h / uart.c \-- Mini UART Driver

Register definitions (offsets from peripheral base \+ 0x215000):

- `AUX_ENABLES`      \+0x04 \-- Auxiliary enables (bit 0 \= Mini UART enable)  
- `AUX_MU_IO_REG`    \+0x40 \-- I/O data (read \= RX, write \= TX)  
- `AUX_MU_IER_REG`   \+0x44 \-- Interrupt enable (set to 0, polling mode)  
- `AUX_MU_IIR_REG`   \+0x48 \-- Interrupt identify (write 0xC6 to clear FIFOs)  
- `AUX_MU_LCR_REG`   \+0x4C \-- Line control (set to 3 for 8-bit mode)  
- `AUX_MU_MCR_REG`   \+0x50 \-- Modem control (set to 0, RTS high)  
- `AUX_MU_LSR_REG`   \+0x54 \-- Line status (bit 0 \= data ready, bit 5 \= TX empty)  
- `AUX_MU_CNTL_REG`  \+0x60 \-- Extra control (bit 0 \= RX enable, bit 1 \= TX enable)  
- `AUX_MU_BAUD_REG`  \+0x68 \-- Baud rate register

Implement these functions:

`void uart_init(void)`:

1. Write 1 to AUX\_ENABLES (enable Mini UART)  
2. Write 0 to AUX\_MU\_CNTL\_REG (disable TX/RX during config)  
3. Write 0 to AUX\_MU\_IER\_REG (no interrupts, polling mode)  
4. Write 3 to AUX\_MU\_LCR\_REG (8-bit data)  
5. Write 0 to AUX\_MU\_MCR\_REG (RTS high)  
6. Write 0xC6 to AUX\_MU\_IIR\_REG (clear TX and RX FIFOs)  
7. Write 541 to AUX\_MU\_BAUD\_REG (115200 baud at 500 MHz VPU clock)  
   - Formula: baudrate\_reg \= (system\_clock / (8 \* baud)) \- 1  
   - With `enable_uart=1` in config.txt, VPU clock is fixed at 500 MHz  
   - 500000000 / (8 \* 115200\) \- 1 \= 541.97, truncated to 541  
   - NOTE: if garbage appears on serial, try 270 (assumes 250 MHz clock)  
8. Set GPIO pin 14 to ALT5 (function value 2\) \-- Mini UART TXD  
9. Set GPIO pin 15 to ALT5 (function value 2\) \-- Mini UART RXD  
10. Disable pull-up/down on pins 14 and 15  
11. Write 3 to AUX\_MU\_CNTL\_REG (enable TX and RX)

`void uart_send(unsigned int c)`:

- Spin-wait on AUX\_MU\_LSR\_REG bit 5 (TX empty)  
- Write character to AUX\_MU\_IO\_REG

`char uart_getc(void)`:

- Spin-wait on AUX\_MU\_LSR\_REG bit 0 (data ready)  
- Read and return byte from AUX\_MU\_IO\_REG (mask with 0xFF)

`void uart_puts(const char *s)`:

- Iterate through string  
- For each '\\n', send '\\r' first (serial terminals expect CR+LF)  
- Send each character via uart\_send

`void uart_hex(unsigned int val)`:

- Print "0x" prefix  
- Print each nibble (4 bits) from most significant to least significant as hex digit  
- Useful for debugging register values in later phases

### kernel.c \-- Kernel Entry Point

void kernel\_main(void)

{

    uart\_init();

    uart\_puts("\\n\\n");

    uart\_puts("================================\\n");

    uart\_puts("  VitOS v0.1 \-- Phase 1\\n");

    uart\_puts("  Raspberry Pi 4 Bare Metal\\n");

    uart\_puts("================================\\n");

    uart\_puts("\\n");

    uart\_puts("\[boot\] UART initialized\\n");

    uart\_puts("\[boot\] Running on core 0 (EL2)\\n");

    uart\_puts("\[boot\] Hello from VitOS\!\\n");

    uart\_puts("\\n");

    uart\_puts("UART echo mode (type something):\\n");

    uart\_puts("\> ");

    while (1) {

        char c \= uart\_getc();

        if (c \== '\\r' || c \== '\\n') {

            uart\_puts("\\n\> ");

        } else {

            uart\_send(c);

        }

    }

}

### config.txt

arm\_64bit=1

enable\_uart=1

### Makefile

Toolchain prefix: `aarch64-none-elf` Compiler flags: `-Wall -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles` Assembler flags: `-ffreestanding -nostdinc -nostdlib -nostartfiles`

Targets:

- `all` depends on `kernel8.img`  
- `boot.o` from `boot.S` using `$(CC) $(ASFLAGS) -c`  
- `%.o` from `%.c` using `$(CC) $(CFLAGS) -c`  
- `kernel8.elf` from all .o files using `$(LD) -nostdlib -T link.ld`  
  - Link order: boot.o must come first  
- `kernel8.img` from `kernel8.elf` using `$(OBJCOPY) -O binary`  
- `clean` removes `*.o kernel8.elf kernel8.img`

Objects list: `boot.o gpio.o uart.o kernel.o`

### README.md

Write a concise README containing:

- Project title and one-line description  
- Hardware target (Pi 4\)  
- Prerequisites (cross-compiler, serial cable, SD card with firmware)  
- Build instructions (`make clean && make`)  
- SD card setup instructions (what files to copy)  
- Serial connection instructions (GPIO pins, baud rate, terminal command)  
- Troubleshooting section (no output, garbage chars, no rainbow screen)

## Validation Criteria

Phase 1 is complete when:

1. `make` produces `kernel8.img` with no warnings or errors  
2. Copying `kernel8.img` \+ firmware files to FAT32 SD card boots the Pi 4  
3. Boot banner "VitOS v0.1 \-- Phase 1" appears on serial terminal at 115200 8N1  
4. Typing characters in the terminal echoes them back  
5. Enter/Return key starts a new prompt line

## Code Style

- C99 standard  
- 4-space indentation, no tabs  
- Opening braces on same line as control statement  
- All functions documented with a one-line comment above  
- Register addresses defined as macros, not magic numbers  
- No use of standard library functions (no libc, no printf, no memset)  
- Explicit volatile access for all MMIO operations

