# Phase 1 — Boot + UART "Hello World": Learning Guide

## What You Will Build

A bare-metal program that boots the Raspberry Pi 4 into your own AArch64 code, initializes the Mini UART serial interface, prints a boot banner, and enters an interactive echo loop — all without any operating system underneath.

**Visible result:** Text appears on your laptop screen via a USB-to-serial adapter. You type characters and they echo back.

## What You Will Learn

| Concept | Why It Matters |
|---------|---------------|
| Cross-compilation | You compile on x86/ARM laptop, but the binary runs on a different ARM target with no OS |
| AArch64 assembly basics | The first instructions that execute cannot be C — you need assembly to set up the CPU state |
| Linker scripts | You control exactly where in memory each piece of code and data lives |
| Freestanding C | Writing C without libc, malloc, printf, or any standard library |
| MMIO (Memory-Mapped I/O) | How CPUs talk to hardware peripherals — reading/writing special memory addresses |
| UART serial protocol | The simplest way for your OS to communicate with the outside world |
| SD card boot process | How the Pi's GPU firmware loads and starts your code |

---

## Prerequisites: What to Study Before Coding

### 1. C Programming (Freestanding Context)

You don't need to be a C expert, but you must be comfortable with:
- Pointers and pointer arithmetic
- Bitwise operations (`&`, `|`, `<<`, `>>`, `~`) — used constantly for register manipulation
- `volatile` keyword — critical for MMIO; without it, the compiler may optimize away your hardware reads/writes
- Header files and include guards
- Building with `gcc` and `make` from the command line

**Resource:** If bitwise operations feel rusty, work through these specific patterns before starting:
```c
// Set bit N in a register
reg |= (1 << N);

// Clear bit N
reg &= ~(1 << N);

// Clear a 3-bit field starting at bit offset, then set new value
reg &= ~(0b111 << offset);
reg |= (value << offset);

// Read-modify-write pattern (the core of MMIO programming)
uint32_t val = mmio_read(REG);
val &= ~MASK;
val |= NEW_VALUE;
mmio_write(REG, val);
```

### 2. Number Systems

You need to be fluent in hexadecimal:
- `0xFE000000` — peripheral base address
- `0xFE215040` — UART I/O register (base + offset)
- Quickly converting between hex, binary, and decimal for register bit fields

### 3. Basic ARM Architecture Awareness

You don't need to memorize the instruction set, but understand these concepts:
- **Registers:** ARM has 31 general-purpose 64-bit registers (x0–x30) plus SP (stack pointer) and PC (program counter)
- **Exception Levels:** EL0 (user), EL1 (kernel), EL2 (hypervisor), EL3 (secure monitor). Phase 1 runs at EL2.
- **Stack:** Grows downward on ARM. The stack pointer must be 16-byte aligned.

### 4. How Make Works

The `Makefile` is your build system. Understand:
- Targets, prerequisites, and recipes
- Pattern rules (`%.o: %.c`)
- Variables (`CC`, `CFLAGS`, etc.)
- `make clean` and phony targets

---

## Core Concepts Deep Dive

### The Boot Sequence (What Happens When You Power On)

```
Power on
   │
   ▼
GPU ROM code runs (burned into silicon, you cannot change this)
   │
   ▼
GPU loads bootcode.bin from SD card (first-stage bootloader)
   │
   ▼
GPU loads start4.elf from SD card (GPU firmware, reads config.txt)
   │
   ▼
config.txt says arm_64bit=1 → GPU switches CPU to AArch64 mode
   │
   ▼
GPU loads kernel8.img to RAM address 0x80000
   │
   ▼
GPU releases CPU core 0, which begins executing at 0x80000
   │
   ▼
Your boot.S code runs (_start) → sets up stack, zeros BSS → calls kernel_main()
   │
   ▼
Your C code runs — initializes UART, prints banner, enters echo loop
```

**Key insight:** The GPU is the first processor that runs, not the CPU. The CPU doesn't start until the GPU firmware tells it to. This is unique to Raspberry Pi.

### Memory-Mapped I/O (MMIO)

On ARM, there is no separate `in`/`out` instruction for hardware (unlike x86). Instead, hardware peripherals are mapped to specific memory addresses. Writing to address `0xFE215040` doesn't write to RAM — it sends data to the UART transmit register.

```c
// This is NOT writing to regular memory — it's sending a byte out the serial port
*(volatile uint32_t *)0xFE215040 = 'H';
```

The `volatile` keyword is **mandatory**. Without it, the compiler sees you writing to the same address multiple times and may optimize all but the last write away — your UART output would be a single character instead of a string.

### The Linker Script

Normally the OS + dynamic linker decide where your code lives in memory. In bare metal, **you** decide. The linker script tells the linker:
- Code starts at `0x80000` (where the GPU loads it)
- `.text.boot` section comes first (your assembly entry point must be the first byte)
- BSS section boundaries are exported as symbols so your boot code can zero them

If the linker script is wrong, the Pi will load your binary but the first instruction won't be `_start`, and the CPU will execute garbage and hang.

### The Assembly Boot Stub

Your `boot.S` is ~20 lines that do three critical things the C compiler cannot:

1. **Park secondary cores.** The Pi 4 has 4 CPU cores. All 4 start executing at `0x80000`. You read the core ID from `mpidr_el1` and send cores 1-3 into an infinite `WFE` (Wait For Event) sleep. Only core 0 continues.

2. **Set up the stack pointer.** C functions need a stack for local variables and return addresses. You set `SP = 0x80000`, and the stack grows downward (toward lower addresses), below your code.

3. **Zero the BSS section.** C guarantees that uninitialized global variables start at zero. In a hosted environment, the OS does this. In bare metal, you must do it yourself using the `__bss_start` and `__bss_end` symbols from the linker script.

### Mini UART vs PL011 UART

The BCM2711 has two UARTs:
- **Mini UART** (used in Phase 1): Simpler, fewer features, baud rate depends on VPU clock. Good enough for debug output.
- **PL011 UART**: Full-featured, dedicated clock, supports DMA. Used by Bluetooth on Pi 4 by default.

We use Mini UART because it's simpler and `enable_uart=1` in config.txt fixes its clock rate to make baud rate predictable.

---

## Tutorials and Written Guides

### Primary (Pi 4 Specific — Start Here)

- **rpi4os.com — Writing a Bare-Metal OS for Raspberry Pi 4**
  - Tutorial: https://www.rpi4os.com/
  - GitHub: https://github.com/babbleberry/rpi4-osdev
  - Covers exactly our Phase 1 scope: bootstrapping, UART, GPIO
  - Same hardware target (BCM2711, GIC-400)
  - Language: C + AArch64 assembly

### Secondary (Pi 3, Adaptable to Pi 4)

- **Raspberry Pi OS by Sergey Matyukevich**
  - Tutorial: https://s-matyukevich.github.io/raspberry-pi-os/
  - GitHub: https://github.com/s-matyukevich/raspberry-pi-os
  - Lesson 1 covers kernel boot and Mini UART — directly maps to our Phase 1
  - Each lesson explains the RPi OS implementation, then shows how Linux does the same thing
  - **Adaptation needed:** Change peripheral base from `0x3F000000` to `0xFE000000` and use Pi 4 GPIO pull-up/down registers

- **raspi3-tutorial by bzt**
  - GitHub: https://github.com/bztsrc/raspi3-tutorial
  - Tutorials 01 (bare minimum), 02 (UART0), 03 (UART1), 05 (UART with mailbox) are Phase 1 relevant
  - **Adaptation needed:** Same peripheral base address change as above

### Conceptual Reference

- **OSDev Wiki — Raspberry Pi Bare Bones**
  - https://wiki.osdev.org/Raspberry_Pi_Bare_Bones
  - Community-maintained, good for cross-referencing when something doesn't work

- **Valvers — Bare Metal Programming in C**
  - https://www.valvers.com/open-software/raspberry-pi/bare-metal-programming-in-c-part-1/
  - Older Pi model, but excellent explanation of MMIO fundamentals and C runtime setup

---

## Video Tutorials

### Primary Series — Follow Along

- **Low Level Devel — Bare Metal Raspberry Pi (C + Assembly)**
  - **Start here (Part 1):** https://www.youtube.com/watch?v=pd9AVmcRc6U
  - Channel: https://www.youtube.com/@LowLevelDevel
  - 17+ parts covering boot, UART, GPIO, exception levels, interrupts, framebuffer, mailbox, HDMI, MMU, SD card/EMMC
  - Follows the rpi4os.com approach — maps directly to our project phases
  - Discord community for questions: https://discord.gg/fyFYABCVMX

- **Low Level Devel — OS Development Using Linux Kernel (Matyukevich's RPi OS)**
  - Playlist: https://www.youtube.com/playlist?list=PLVxiWMqQvhg8ZisiOBLAVkhLOYCkzTst0
  - Video companion to the Raspberry Pi OS written tutorial listed above

### ARM Assembly Fundamentals

- **LaurieWired — ARM Assembly Tutorial Series (Lessons 1–11+)**
  - Channel: https://www.youtube.com/@LaurieWired
  - Builds AArch64 assembly fluency needed for understanding `boot.S` and exception vectors
  - Watch at least lessons on registers, memory access, and branching before writing `boot.S`

### Supplementary / Conceptual

- **Low Byte Productions — Low-Level Programming Deep Dives**
  - Channel: https://www.youtube.com/@LowByteProductions
  - Good for building intuition about what happens "below" high-level languages

- **Writing an OS in Rust by Philipp Oppermann (Conceptual Reference)**
  - Blog: https://os.phil-opp.com/
  - Video walkthrough: https://www.youtube.com/playlist?list=PLib6-zlkjfXkdCjQgrZhmfJOWBk_C2FTY
  - x86_64 + Rust (not our target), but the phased progression from freestanding binary → VGA → interrupts → paging is an excellent conceptual blueprint for understanding why each phase exists

---

## Datasheets and Reference Manuals

These are dense — don't try to read them cover-to-cover. Use them as reference when you need to understand a specific register.

| Document | What to Look Up for Phase 1 |
|----------|----------------------------|
| [BCM2711 ARM Peripherals](https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf) | Chapter 2 (Auxiliaries/Mini UART registers), Chapter 5 (GPIO), Section on pull-up/down |
| [ARM Cortex-A72 TRM](https://developer.arm.com/documentation/100095/latest) | `MPIDR_EL1` register (for core ID in boot stub) |
| [Raspberry Pi Firmware Wiki](https://github.com/raspberrypi/firmware/wiki) | Boot process, `config.txt` options |

**Tip:** The BCM2711 datasheet has known errors. When a register doesn't behave as documented, cross-reference with the [Linux kernel source](https://github.com/raspberrypi/linux) or the [Circle library](https://github.com/rsta2/circle) (mature bare-metal C++ environment for Pi 1–5).

---

## Hardware Needed for Phase 1

| Item | Purpose | Notes |
|------|---------|-------|
| Raspberry Pi 4 Model B (2GB+ RAM) | Target board | Any RAM size works |
| USB-to-TTL serial cable (3.3V) | UART serial connection | FTDI FT232RL recommended. **Must be 3.3V — 5V will damage the Pi** |
| MicroSD card (16GB+) | Boot media | FAT32 formatted, MBR partition table |
| USB-C power supply (5V/3A) | Power the Pi | Official Pi supply or equivalent; some cables are charge-only and won't work |

**You do NOT need** an HDMI monitor, keyboard, LEDs, or breadboard for Phase 1. Everything happens over the serial connection.

---

## Suggested Learning Path

```
Week 1: Preparation
├── Watch Low Level Devel Part 1 video
├── Read rpi4os.com chapters on boot + UART
├── Read Matyukevich Lesson 1 (kernel boot)
├── Practice bitwise operations in a C playground
└── Install cross-compiler, verify it works

Week 2: Build and Boot
├── Write boot.S (assembly stub)
├── Write linker script (link.ld)
├── Write gpio.c/h (MMIO functions)
├── Write uart.c/h (Mini UART driver)
├── Write kernel.c (main entry + echo loop)
├── Write Makefile
├── Build with make, inspect with objdump
├── Test in QEMU first (quick sanity check)
└── Deploy to real Pi 4, verify serial output

Milestone: "VitOS v0.1 — Phase 1" banner on serial terminal
```

---

## Common Mistakes in Phase 1

| Mistake | Consequence | Fix |
|---------|-------------|-----|
| Using Pi 3 peripheral base (`0x3F000000`) | UART writes go to unmapped memory, no output | Use `0xFE000000` for Pi 4 |
| Using GPPUD/GPPUDCLK for GPIO pull config | Sequence does nothing on Pi 4 hardware | Use `GPIO_PUP_PDN_CNTRL_REG0` at offset `0x2000E4` |
| Forgetting `volatile` on MMIO pointers | Compiler optimizes away hardware accesses | Always cast to `volatile uint32_t *` |
| `.text.boot` not first in linker script | `_start` isn't at `0x80000`, CPU executes wrong code | Ensure `KEEP(*(.text.boot))` is first in `.text` |
| Missing `\r` before `\n` in serial output | Lines overwrite each other or don't advance | `uart_puts` must send CR+LF (`\r\n`) |
| Not zeroing BSS in boot stub | Global variables have garbage initial values | Loop from `__bss_start` to `__bss_end` storing zero |
| Using `start.elf` instead of `start4.elf` | GPU firmware for wrong Pi model, boot fails | Pi 4 requires `start4.elf` and `fixup4.dat` |
| Connecting 5V serial adapter to GPIO | Permanent hardware damage to GPIO pins | Verify adapter is 3.3V before connecting |
