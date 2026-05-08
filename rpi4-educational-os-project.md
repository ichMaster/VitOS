# Educational Operating System for Raspberry Pi 4

## Project Overview

A bare-metal educational operating system written in C (with minimal AArch64 assembly) for the Raspberry Pi 4 Model B. The project is structured in phases, each delivering a visible, demonstrable result. The primary goal is deep personal learning of OS internals, ARM architecture, and hardware-level programming.

## Target Hardware

- **Board:** Raspberry Pi 4 Model B
- **SoC:** BCM2711
- **CPU:** Quad-core ARM Cortex-A72 (ARMv8-A / AArch64), 1.5 GHz
- **Interrupt Controller:** GIC-400 (differs from legacy Pi models)
- **Peripheral base address:** 0xFE000000 (differs from Pi 3's 0x3F000000)
- **Boot sequence:** GPU loads `bootcode.bin` and `start4.elf` from SD card, then jumps to `kernel8.img` at address 0x80000

## Language and Toolchain

- **Language:** C (freestanding) + AArch64 assembly for boot/exception stubs
- **Cross-compiler:** `aarch64-none-elf-gcc` (ARM's bare-metal toolchain)
- **Build system:** Makefile producing `kernel8.img`
- **Linker:** Custom linker script defining memory layout
- **Output format:** ELF extracted via `objcopy` to raw binary `kernel8.img`
- **Debugging:** UART serial output via USB-to-TTL cable (FTDI recommended)
- **Emulation (optional):** QEMU `raspi4b` machine (limited peripheral support)

---

## Phases

### Phase 1 -- Boot + UART "Hello World"

**Goal:** Boot the Pi 4 into your own code and print text to a serial terminal.

**What happens:**
- GPU loads firmware blobs (`bootcode.bin`, `start4.elf`) and jumps to `kernel8.img` at 0x80000
- Minimal AArch64 assembly stub: check processor ID (run on core 0 only, park cores 1-3), set up stack pointer, zero BSS section, branch to C `main()`
- Configure Mini UART (or PL011 UART) via MMIO registers
- Print "Hello from VitOS" to serial console

**Visible result:** Text appears on laptop screen via USB-to-serial adapter.

**Key learnings:** Cross-compilation, linker scripts, SD card image creation, MMIO register access, AArch64 boot sequence.

---

### Phase 2 -- GPIO + Blinking LED

**Goal:** Control physical hardware from your OS code.

**What happens:**
- MMIO access to GPIO controller registers
- Set GPIO pin to output mode, toggle it with busy-wait delay loop
- Read UART input, toggle LED on typed commands
- Build a minimal `printf`-like function and rudimentary command parser (embryo of a shell)

**Visible result:** Physical LED blinks; LED responds to typed commands over serial.

**Key learnings:** GPIO peripheral programming, MMIO patterns, basic I/O loop, formatted output.

---

### Phase 3 -- Interrupts + System Timer

**Goal:** Replace polling with interrupt-driven architecture.

**What happens:**
- Set up the GIC-400 (Generic Interrupt Controller) -- this is the major Pi 4 departure from older models that used the legacy BCM interrupt controller
- Write exception vector table in assembly (EL1 exception vectors)
- Route ARM system timer interrupt to generate periodic ticks
- Implement a tick counter; replace busy-wait LED blink with timer-driven blink
- Add `uptime` command to UART shell

**Visible result:** LED blinks at precise frequency; `uptime` command shows elapsed seconds.

**Key learnings:** ARM exception model (EL0/EL1/EL2), GIC-400 configuration, interrupt service routines, exception vector table layout.

---

### Phase 4 -- Framebuffer + Screen Output

**Goal:** Draw to the screen via HDMI.

**What happens:**
- Use VideoCore mailbox interface to negotiate a framebuffer from the GPU
- Draw pixels directly to the framebuffer (colored rectangles, test patterns)
- Implement a bitmap font renderer (PSF or custom font)
- Print boot log and OS name to HDMI display

**Visible result:** OS name and boot log visible on a real monitor.

**Key learnings:** Mailbox protocol, framebuffer concepts, pixel formats, bitmap font rendering.

---

### Phase 5 -- Memory Management (MMU + Allocator)

**Goal:** Enable virtual memory and dynamic memory allocation.

**What happens:**
- Configure the MMU with translation tables (4KB granule)
- Start with identity mapping, then implement proper kernel/user address space split
- Implement a page allocator (buddy system or bitmap-based)
- Build a small `kmalloc` / `kfree` on top of the page allocator
- Add `meminfo` command reporting total/free/used pages

**Visible result:** `meminfo` command displays memory statistics in the shell.

**Key learnings:** ARMv8-A translation tables, TLB management, page allocation algorithms, heap management.

---

### Phase 6 -- Processes + Scheduler

**Goal:** Run multiple tasks concurrently.

**What happens:**
- Define a task/process control block (saved registers, stack pointer, state, PID)
- Implement context switching in assembly (save/restore register state)
- Start with cooperative multitasking (explicit `yield()`)
- Upgrade to preemptive multitasking (timer interrupt forces context switch)
- Create demo tasks: one prints to UART, one blinks LED, one draws a counter on framebuffer

**Visible result:** Multiple tasks running visibly simultaneously on different output channels.

**Key learnings:** Context switching mechanics, scheduling algorithms (round-robin), process state management, preemption.

---

### Phase 7 -- System Calls + User Mode

**Goal:** Establish kernel/user boundary with privilege separation.

**What happens:**
- Drop processes to EL0 (user mode); kernel runs at EL1
- Implement SVC-based syscall interface (`write`, `sleep`, `yield`, `exit`)
- Set up MMU page table entries to enforce memory protection between kernel and user
- User processes cannot access kernel memory directly

**Visible result:** A user process crashes (e.g., null pointer dereference) and the kernel survives, reports the fault, and continues running other processes.

**Key learnings:** Privilege levels (EL0/EL1), SVC exception handling, syscall dispatch, memory protection, fault handling.

---

### Phase 8 -- FAT32 Filesystem + SD Card Driver

**Goal:** Read files from the SD card.

**What happens:**
- Implement EMMC/SD card driver for the BCM2711 (one of the harder peripherals)
- Read the MBR, parse a FAT32 partition
- Implement directory traversal, file reading
- Add `ls` and `cat` commands to the shell

**Visible result:** `ls` lists files on the SD card; `cat` displays file contents. Files edited on a laptop are readable by the OS.

**Key learnings:** SD/EMMC protocol, block device drivers, FAT32 filesystem structures (BPB, FAT table, directory entries), DMA.

---

### Phase 9 -- Shell + Loadable Programs

**Goal:** Load and execute standalone programs from the filesystem.

**What happens:**
- Build a proper interactive shell (command history, line editing)
- Implement a simple ELF loader (or flat binary loader)
- Load programs from FAT32 filesystem into user memory
- Execute loaded programs as user-mode processes via the syscall interface
- Write a few demo programs: `hello`, `fibonacci`, `memtest`

**Visible result:** Type `hello` in the shell, the OS loads `/bin/hello` from the SD card, runs it in user mode, and displays output. This is the "it feels like a real OS" moment.

**Key learnings:** ELF format parsing, program loading, address space setup for user processes, argument passing.

---

### Phase 10 -- Extension (Pick Your Adventure)

Choose one or more directions based on interest:

| Direction | Description |
|-----------|-------------|
| **Multicore** | Wake cores 1-3 from spin tables, implement per-core scheduling, inter-core communication |
| **Networking** | USB Ethernet or native Gigabit (BCM54213PE PHY) driver, basic TCP/IP stack |
| **USB HID** | USB host controller driver for keyboard input on the framebuffer console |
| **LoRa/Meshtastic** | SPI driver for LoRa module, enabling mesh communication between Pi nodes (ties into swarm robotics work) |
| **Sound** | PWM audio or HDMI audio output, simple wave player |
| **IPC** | Pipes, shared memory, message queues between processes |

---

## Essential References and Documentation

### Datasheets and Manuals

| Document | Description |
|----------|-------------|
| [BCM2711 ARM Peripherals](https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf) | Pi 4 peripheral register map (UART, GPIO, timers, EMMC, mailbox) |
| [ARM Architecture Reference Manual (ARMv8-A)](https://developer.arm.com/documentation/ddi0487/latest) | Definitive reference for AArch64 instruction set, exception model, MMU |
| [ARM Cortex-A72 Technical Reference Manual](https://developer.arm.com/documentation/100095/latest) | Core-specific details (caches, branch prediction, debug) |
| [ARM GIC-400 Technical Reference Manual](https://developer.arm.com/documentation/ddi0471/latest) | Interrupt controller used by Pi 4 (replaces legacy BCM interrupt controller) |
| [Raspberry Pi Firmware Wiki](https://github.com/raspberrypi/firmware/wiki) | Boot process, config.txt options, mailbox property interface |

---

## Tutorials and Written Guides (C, Bare-Metal)

### Tier 1 -- Pi 4 Specific

- **rpi4os.com** -- Writing a bare-metal OS for Raspberry Pi 4
  - Tutorial: <https://www.rpi4os.com/>
  - GitHub: <https://github.com/babbleberry/rpi4-osdev>
  - Covers: bootstrapping, building, UART, mailbox, framebuffer, Bluetooth, GPIO, interrupts, sound, multicore
  - Language: C + AArch64 assembly
  - Targets exact Pi 4 hardware (BCM2711, GIC-400)

### Tier 2 -- Pi 3 (AArch64, Easily Adapted to Pi 4)

- **Raspberry Pi OS** by Sergey Matyukevich -- Learning OS development using Linux kernel and Raspberry Pi
  - Tutorial: <https://s-matyukevich.github.io/raspberry-pi-os/>
  - GitHub: <https://github.com/s-matyukevich/raspberry-pi-os>
  - Covers: UART, interrupts, process management, scheduler, virtual memory, user mode, fork()
  - Each lesson explains RPi OS implementation, then shows how Linux does the same thing
  - Language: C + AArch64 assembly
  - Targets Pi 3 (BCM2837) -- requires peripheral base address change for Pi 4 (0x3F000000 to 0xFE000000) and GIC-400 adaptation

- **raspi3-tutorial** by bzt -- Bare metal Raspberry Pi 3 tutorials
  - GitHub: <https://github.com/bztsrc/raspi3-tutorial>
  - Covers: UART, mailbox, framebuffer, SD card, keyboard, random numbers, file reading
  - Focused on hardware interfacing rather than OS theory
  - Language: C + AArch64 assembly

### Tier 3 -- Older Pi Models (Conceptually Useful)

- **Valvers Bare Metal C** -- Bare metal programming in C on Raspberry Pi
  - Tutorial: <https://www.valvers.com/open-software/raspberry-pi/bare-metal-programming-in-c-part-1/>
  - Covers: LED blinking, C runtime setup, interrupts, mailbox
  - Good for understanding MMIO fundamentals

- **Computer Systems Lab** by Sean Lawless
  - Tutorial: <https://sean-lawless.github.io/computersystems/>
  - Step-by-step book/lab from LED blinking to a simple Ultima-style game

### Tier 4 -- Architecture-Agnostic OS Dev Reference

- **OSDev Wiki** -- Community encyclopedia for OS development
  - Tutorials: <https://wiki.osdev.org/Tutorials>
  - Bare Bones: <https://wiki.osdev.org/Bare_Bones>
  - Raspberry Pi Bare Bones: <https://wiki.osdev.org/Raspberry_Pi_Bare_Bones>
  - x86-centric, but conceptual articles on scheduling, memory management, ELF loading, and filesystem design are architecture-neutral

- **Circle** by rsta2 -- Mature bare-metal C++ environment for Pi 1-5
  - GitHub: <https://github.com/rsta2/circle>
  - Documentation: <https://circle-rpi.readthedocs.io/>
  - Not a tutorial, but an excellent reference implementation when stuck on specific BCM2711 peripherals

---

## Video Tutorials

### Primary Series

- **Low Level Devel** -- Bare Metal Raspberry Pi series (C + assembly)
  - Channel: <https://www.youtube.com/@LowLevelDevel>
  - Part 1 (start here): <https://www.youtube.com/watch?v=pd9AVmcRc6U>
  - Covers: boot, UART, GPIO, exception levels (EL1), interrupts, framebuffer, mailbox, HDMI video, MMU, SD card/EMMC
  - 17+ parts, follows rpi4os.com approach
  - Discord community: <https://discord.gg/fyFYABCVMX>

- **Low Level Devel** -- OS Development Using Linux Kernel (follows Matyukevich's RPi OS)
  - Playlist: <https://www.youtube.com/playlist?list=PLVxiWMqQvhg8ZisiOBLAVkhLOYCkzTst0>

### Supplementary

- **Low Byte Productions** -- Deep dives into low-level programming concepts
  - Channel: <https://www.youtube.com/@LowByteProductions>

- **LaurieWired** -- ARM Assembly tutorial series (Lessons 1-11+)
  - Channel: <https://www.youtube.com/@LaurieWired>
  - Useful for building AArch64 assembly fluency needed for boot stubs and exception vectors

- **RTOS on ARM Microcontrollers** -- Comprehensive RTOS concepts
  - Video: <https://www.youtube.com/watch?v=FSmisRk7Neg>
  - Cortex-M oriented, but scheduling/synchronization concepts transfer directly

### Rust-Based (For Conceptual Reference Only)

- **Writing an OS in Rust** by Philipp Oppermann
  - Blog: <https://os.phil-opp.com/>
  - GitHub: <https://github.com/phil-opp/blog_os>
  - Video walkthrough: <https://www.youtube.com/playlist?list=PLib6-zlkjfXkdCjQgrZhmfJOWBk_C2FTY>
  - x86_64 + Rust, but the phased progression (freestanding binary, VGA, interrupts, paging, heap, multitasking, async) is an excellent conceptual blueprint

---

## Hardware Shopping List

| Item | Purpose | Notes |
|------|---------|-------|
| Raspberry Pi 4 Model B (2GB+ RAM) | Target hardware | Already available |
| USB-to-TTL Serial Cable (FTDI FT232RL) | UART debugging | Connect to GPIO 14 (TX) and GPIO 15 (RX); 3.3V logic level |
| MicroSD card (16GB+) | Boot media | FAT32 formatted; will hold firmware blobs + kernel8.img |
| HDMI micro cable + monitor | Framebuffer output | For Phase 4+ |
| Breadboard + LEDs + resistors | GPIO experiments | For Phase 2 |
| USB keyboard | Direct input (optional) | For Phase 10 USB HID driver |

---

## Practical Tips

1. **Always test with UART first.** The serial console is your lifeline before framebuffer output works. Invest in a reliable USB-to-TTL cable.

2. **Keep the firmware blobs.** You need `bootcode.bin`, `start4.elf`, and `fixup4.dat` from the official Raspberry Pi firmware repo on the SD card alongside your `kernel8.img`.

3. **The `kernel8.img` filename matters.** It signals 64-bit AArch64 mode. Using `kernel7.img` would boot in 32-bit mode.

4. **Peripheral address gotcha.** Pi 4 uses 0xFE000000 as peripheral base (not 0x3F000000 like Pi 3). Any code ported from Pi 3 tutorials needs this adjustment.

5. **GIC-400 vs legacy interrupts.** Most older Pi tutorials use the BCM-specific interrupt controller. Pi 4 uses the ARM standard GIC-400, which is actually better documented but requires different setup code.

6. **QEMU has limited Pi 4 support.** The `raspi4b` machine in QEMU doesn't emulate all peripherals. Real hardware testing is essential, especially for SD card, USB, and Ethernet.

7. **Iteration speed matters.** Set up a fast workflow: `make` produces `kernel8.img`, script copies it to mounted SD card, eject, insert into Pi, power cycle. Some developers use TFTP boot via Ethernet to skip the SD card swap entirely.

8. **Read the errata.** The BCM2711 datasheet has known errors. Cross-reference with Linux kernel source code and the Circle library when register descriptions seem wrong.

---

## Project Name Ideas

- VitOS
- SykhivOS
- PiCore
- NanoKernel
- BareMind

---

*Document created: May 2026*
*Status: Planning phase*
