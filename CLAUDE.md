# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

VitOS is a bare-metal educational operating system for Raspberry Pi 4 Model B, written in C (freestanding, C99) with minimal AArch64 assembly. The project is structured in phases, each delivering a visible result. There is no standard library — everything is built from scratch against hardware registers.

## Target Hardware

- **SoC:** BCM2711 with ARM Cortex-A72 (ARMv8-A / AArch64)
- **Peripheral base address:** `0xFE000000` (NOT `0x3F000000` — that's Pi 3)
- **Interrupt controller:** GIC-400 (NOT the legacy BCM interrupt controller from older Pi models)
- **Boot address:** GPU loads `kernel8.img` to `0x80000`
- **GPIO pull-up/down:** Uses `GPIO_PUP_PDN_CNTRL_REG0` at `0xFE2000E4` (NOT the GPPUD/GPPUDCLK sequence from Pi 3 tutorials)

## Build Commands

```bash
# Prerequisites: aarch64-none-elf-gcc cross-compiler (bare-metal variant)
# Verify: aarch64-none-elf-gcc --version

make                    # Build kernel8.img
make clean              # Remove build artifacts
make clean && make      # Full rebuild

# Deploy to SD card (when mounted)
cp kernel8.img /media/$USER/boot/ && sync

# QEMU test (UART only — limited peripheral emulation)
qemu-system-aarch64 -M raspi4b -m 2G -serial stdio -kernel kernel8.img -nographic
# Exit QEMU: Ctrl-A, X
```

## Inspecting Build Output

```bash
aarch64-none-elf-objdump -h kernel8.elf    # Verify .text starts at 0x80000
aarch64-none-elf-objdump -d kernel8.elf    # Disassemble
aarch64-none-elf-nm kernel8.elf | sort     # Check symbols
```

## Compiler Flags

`-Wall -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles` for C files. Same without `-Wall -O2` for assembly. Linker uses `-nostdlib -T link.ld`. Output is `objcopy -O binary` from ELF to raw `kernel8.img`.

## Architecture

### Boot Sequence
1. GPU loads firmware (`bootcode.bin`, `start4.elf`) from FAT32 SD card
2. GPU jumps to `kernel8.img` at `0x80000` in AArch64 mode
3. `boot.S` (`_start`): parks cores 1-3 via WFE, core 0 sets stack pointer to `0x80000` (grows down), zeros BSS, branches to `kernel_main`
4. `kernel_main` in `kernel.c` initializes hardware and runs the main loop

### Memory-Mapped I/O Pattern
All hardware access is through volatile MMIO reads/writes to peripheral registers at offsets from `0xFE000000`. No DMA or interrupts in Phase 1.

### Linker Script Layout
Sections ordered: `.text.boot` (must be first — contains `_start`), `.text`, `.rodata`, `.data`, `.bss` (NOLOAD, with `__bss_start`/`__bss_end` symbols). Entry at `0x80000`.

### Mini UART
Registers at `0xFE215000`. Configured for 115200 baud, 8N1, polling mode. GPIO pins 14 (TX, ALT5) and 15 (RX, ALT5). Baud divisor 541 assumes 500 MHz VPU clock (set via `enable_uart=1` in `config.txt`). If garbage on serial, try 270 (250 MHz clock).

## Phase Roadmap

| Phase | Goal |
|-------|------|
| 1 | Boot + UART "Hello World" (current) |
| 2 | GPIO + blinking LED + command parser |
| 3 | GIC-400 interrupts + system timer |
| 4 | Framebuffer via mailbox interface |
| 5 | MMU + page allocator + kmalloc |
| 6 | Processes + preemptive scheduler |
| 7 | Syscalls + EL0 user mode |
| 8 | FAT32 filesystem + SD card driver |
| 9 | Shell + ELF loader |
| 10 | Extensions (multicore, networking, USB, etc.) |

## Critical Pi 4 Gotchas

- **Peripheral base is `0xFE000000`**, not `0x3F000000`. Any code ported from Pi 3 tutorials needs this change.
- **GPIO pull-up/down registers are different on Pi 4.** The GPPUD + GPPUDCLK0 + 150-cycle-delay sequence from Pi 3 tutorials does NOT work. Use `GPIO_PUP_PDN_CNTRL_REG0` at offset `0x2000E4`.
- **GIC-400 replaces the legacy BCM interrupt controller.** Most older Pi tutorials use the wrong interrupt controller for Pi 4.
- **Phase 1 runs at EL2.** Do NOT attempt to drop to EL1 — that comes in Phase 3.
- **`kernel8.img` filename signals AArch64 mode.** Using `kernel7.img` would boot in 32-bit mode.
- **QEMU `raspi4b` has limited peripheral support.** GPIO, SD card, USB, and GIC-400 may not work correctly. Always verify on real hardware.

## Code Style

- C99, 4-space indentation, no tabs
- Opening braces on same line as control statement
- Register addresses as macros, no magic numbers
- Explicit `volatile` for all MMIO operations
- No standard library — freestanding headers only (`<stdint.h>`, `<stddef.h>`, `<stdarg.h>`, `<limits.h>`, `<float.h>`)
- `\r\n` (CR+LF) for serial output — `uart_puts` must send `\r` before each `\n`

## Key References

- [BCM2711 ARM Peripherals datasheet](https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf) — register map
- [ARM GIC-400 TRM](https://developer.arm.com/documentation/ddi0471/latest) — interrupt controller
- [rpi4os.com](https://www.rpi4os.com/) — Pi 4 specific bare-metal tutorial (primary reference)
- [Circle library](https://github.com/rsta2/circle) — mature Pi bare-metal reference implementation
- BCM2711 datasheet has known errata — cross-reference with Linux kernel source and Circle when registers behave unexpectedly
