# VitOS \-- Development, Compilation, Debugging, and Execution Guide

## Table of Contents

1. Host Machine Setup  
2. SD Card Preparation  
3. Hardware Wiring (UART Serial Connection)  
4. Development Workflow  
5. Compilation  
6. Deployment to SD Card  
7. Execution and First Boot  
8. Debugging Techniques  
9. Advanced: TFTP Network Boot (Skip SD Card Swaps)  
10. Advanced: QEMU Emulation  
11. Troubleshooting Reference

---

## 1\. Host Machine Setup

### Operating System

Any Linux distribution works. Ubuntu 22.04+ or Debian 12+ are recommended. macOS works with Homebrew-installed LLVM. Windows requires WSL2 with Ubuntu.

### Install the Cross-Compiler

The key tool is the AArch64 bare-metal cross-compiler. This produces ARM64 binaries that run without an OS underneath.

**Option A \-- Package manager (Ubuntu/Debian):**

sudo apt update

sudo apt install gcc-aarch64-none-elf binutils-aarch64-none-elf make

If this package is not available in your distro's repos, use Option B.

**Option B \-- Download from ARM:**

Go to: [https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)

Download the **AArch64 bare-metal target** (`aarch64-none-elf`) for your host architecture:

- x86\_64 Linux host: `arm-gnu-toolchain-*-x86_64-aarch64-none-elf.tar.xz`  
- AArch64 Linux host: `arm-gnu-toolchain-*-aarch64-aarch64-none-elf.tar.xz`

Extract and add to PATH:

tar xf arm-gnu-toolchain-\*-aarch64-none-elf.tar.xz

export PATH="$PWD/arm-gnu-toolchain-\*/bin:$PATH"

\# Add to \~/.bashrc for persistence:

echo 'export PATH="$HOME/arm-gnu-toolchain-\*/bin:$PATH"' \>\> \~/.bashrc

**Verify installation:**

aarch64-none-elf-gcc \--version

aarch64-none-elf-ld \--version

aarch64-none-elf-objcopy \--version

All three must resolve. If any fails, the toolchain is not correctly installed or not on your PATH.

### Install a Serial Terminal

You need a terminal emulator to communicate with the Pi over UART.

\# Pick one:

sudo apt install minicom

sudo apt install picocom

sudo apt install screen

Recommended: `minicom` for its configuration flexibility, or `picocom` for simplicity.

### Install SD Card Tools (Optional)

sudo apt install dosfstools parted

These help format and partition SD cards from the command line.

---

## 2\. SD Card Preparation

### Format the SD Card

The Pi 4 bootloader expects a FAT32 partition with MBR (not GPT) partition table.

**Using a GUI tool (easiest):** Use GParted, GNOME Disks, or Raspberry Pi Imager to create a single FAT32 partition.

**Using the command line:**

\# Identify your SD card (BE CAREFUL \-- wrong device \= data loss)

lsblk

\# Assuming SD card is /dev/sdX (replace X with actual letter)

\# THIS WILL ERASE ALL DATA ON THE DEVICE

sudo parted /dev/sdX \--script mklabel msdos

sudo parted /dev/sdX \--script mkpart primary fat32 1MiB 100%

sudo mkfs.vfat \-F 32 /dev/sdX1

### Download Raspberry Pi Firmware

Clone or download the boot files from the official firmware repository:

\# Clone only the boot directory (sparse checkout to save bandwidth)

git clone \--depth 1 \--filter=blob:none \--sparse \\

    https://github.com/raspberrypi/firmware.git rpi-firmware

cd rpi-firmware

git sparse-checkout set boot

Or download individual files directly:

mkdir rpi-firmware && cd rpi-firmware

wget https://github.com/raspberrypi/firmware/raw/master/boot/bootcode.bin

wget https://github.com/raspberrypi/firmware/raw/master/boot/start4.elf

wget https://github.com/raspberrypi/firmware/raw/master/boot/fixup4.dat

### Copy Firmware to SD Card

\# Mount the SD card

sudo mount /dev/sdX1 /mnt

\# Copy the three required firmware files

sudo cp bootcode.bin /mnt/

sudo cp start4.elf /mnt/

sudo cp fixup4.dat /mnt/

\# Create config.txt

cat \<\< 'EOF' | sudo tee /mnt/config.txt

arm\_64bit=1

enable\_uart=1

EOF

\# Unmount

sudo umount /mnt

Your SD card now has:

bootcode.bin     \~52 KB   GPU first-stage bootloader

start4.elf       \~2.2 MB  GPU firmware (Pi 4 specific)

fixup4.dat       \~5 KB    GPU memory split configuration

config.txt       \~30 B    Boot configuration

The `kernel8.img` will be added after compilation.

---

## 3\. Hardware Wiring (UART Serial Connection)

### What You Need

A USB-to-TTL serial adapter operating at **3.3V logic level**. Common options:

- FTDI FT232RL breakout (most reliable)  
- CP2102 module  
- CH340G module

**WARNING:** The Pi 4 GPIO is 3.3V. Connecting a 5V serial adapter WILL DAMAGE the GPIO pins permanently. Verify your adapter's logic level before connecting.

### Wiring Diagram

Raspberry Pi 4 GPIO Header          USB-to-TTL Adapter

(looking at the board, USB ports facing down)

Pin 6  \[GND\]  \-------- \[GND\]

Pin 8  \[GPIO14/TXD\] \--- \[RXD\]    (Pi transmits, adapter receives)

Pin 10 \[GPIO15/RXD\] \--- \[TXD\]    (Adapter transmits, Pi receives)

DO NOT CONNECT VCC/5V between them.

Power the Pi from its own USB-C supply.

Physical pin layout reference (top-left corner of the header is pin 1):

        3V3  (1)  (2)  5V

      GPIO2  (3)  (4)  5V

      GPIO3  (5)  (6)  GND    \<-- connect to adapter GND

      GPIO4  (7)  (8)  GPIO14 \<-- connect to adapter RXD (Pi TX)

        GND  (9)  (10) GPIO15 \<-- connect to adapter TXD (Pi RX)

      ...

### Connect the Adapter to Your Computer

Plug the USB end of the adapter into your laptop/desktop. Verify it appears:

\# Linux

ls /dev/ttyUSB\*

\# Expected: /dev/ttyUSB0 (FTDI, CH340) or /dev/ttyACM0 (some adapters)

\# If no device appears:

dmesg | tail \-20

\# Look for USB serial converter messages

\# You may need to install drivers: sudo apt install linux-modules-extra-$(uname \-r)

### Open the Serial Terminal

\# Using minicom:

minicom \-b 115200 \-D /dev/ttyUSB0

\# Using picocom (simpler):

picocom \-b 115200 /dev/ttyUSB0

\# Using screen:

screen /dev/ttyUSB0 115200

Settings: **115200 baud, 8 data bits, no parity, 1 stop bit (8N1), no hardware flow control.**

For minicom, disable hardware flow control:

\# Inside minicom, press Ctrl-A, then O (configure)

\# Navigate to: Serial port setup

\# Set: Hardware Flow Control: No

\# Set: Software Flow Control: No

\# Save as default

**Leave the terminal open.** You will see output here when the Pi boots.

---

## 4\. Development Workflow

### Recommended Directory Structure

\~/projects/vitos/

  src/

    boot.S

    gpio.h

    gpio.c

    uart.h

    uart.c

    common.h

    kernel.c

  link.ld

  Makefile

  config.txt

  README.md

  sdcard/               \# Local copy of firmware files

    bootcode.bin

    start4.elf

    fixup4.dat

### Edit-Compile-Deploy-Test Cycle

The core development loop:

1\. Edit source code on your host machine (any editor/IDE)

2\. Run \`make\` to compile

3\. Copy kernel8.img to SD card

4\. Insert SD card into Pi, power on

5\. Observe output on serial terminal

6\. Power off Pi, remove SD card

7\. Repeat from step 1

This cycle takes about 30-60 seconds per iteration. Section 9 describes TFTP boot to reduce this to under 5 seconds.

### Makefile Convenience Targets

Add these to your Makefile for faster workflow:

\# Mount point for SD card

SDCARD\_MOUNT \= /media/$(USER)/boot

\# Deploy to mounted SD card

deploy: kernel8.img

	cp kernel8.img $(SDCARD\_MOUNT)/

	sync

	@echo "kernel8.img copied to SD card. Safe to eject."

\# Full cycle: build \+ deploy

run: deploy

	@echo "Insert SD card into Pi and power on."

	@echo "Open serial terminal: picocom \-b 115200 /dev/ttyUSB0"

---

## 5\. Compilation

### Build Process Explained

The build has four stages:

boot.S  \--\[assembler\]--\> boot.o       (assembly to object file)

gpio.c  \--\[compiler\]---\> gpio.o       (C to object file)

uart.c  \--\[compiler\]---\> uart.o       (C to object file)

kernel.c \--\[compiler\]--\> kernel.o     (C to object file)

boot.o \+ gpio.o \+ uart.o \+ kernel.o

        \--\[linker\]------\> kernel8.elf (linked ELF executable)

kernel8.elf \--\[objcopy\]--\> kernel8.img (raw binary image)

### Compile Commands (What the Makefile Does)

\# Step 1: Assemble boot stub

aarch64-none-elf-gcc \-ffreestanding \-nostdinc \-nostdlib \-nostartfiles \\

    \-c boot.S \-o boot.o

\# Step 2: Compile C files

aarch64-none-elf-gcc \-Wall \-O2 \-ffreestanding \-nostdinc \-nostdlib \-nostartfiles \\

    \-c gpio.c \-o gpio.o

aarch64-none-elf-gcc \-Wall \-O2 \-ffreestanding \-nostdinc \-nostdlib \-nostartfiles \\

    \-c uart.c \-o uart.o

aarch64-none-elf-gcc \-Wall \-O2 \-ffreestanding \-nostdinc \-nostdlib \-nostartfiles \\

    \-c kernel.c \-o kernel.o

\# Step 3: Link into ELF

aarch64-none-elf-ld \-nostdlib \-T link.ld \-o kernel8.elf \\

    boot.o gpio.o uart.o kernel.o

\# Step 4: Extract raw binary

aarch64-none-elf-objcopy \-O binary kernel8.elf kernel8.img

### Compiler Flags Explained

| Flag | Purpose |
| :---- | :---- |
| `-Wall` | Enable all warnings \-- essential, treat every warning as a bug |
| `-O2` | Optimization level 2 \-- good balance of speed and debuggability |
| `-ffreestanding` | Tell GCC there is no hosted environment (no OS, no libc) |
| `-nostdinc` | Do not search standard system include directories |
| `-nostdlib` | Do not link standard libraries (no libc, no libgcc) |
| `-nostartfiles` | Do not use standard startup files (crt0.o etc.) |

Note: `-O0` (no optimization) is useful for debugging with JTAG since the code maps more directly to source lines. For UART-based printf debugging, `-O2` is fine.

### Inspecting the Build Output

After building, inspect the binary to verify correctness:

\# Check ELF sections and their addresses

aarch64-none-elf-objdump \-h kernel8.elf

\# Expected: .text starts at 0x80000

\# Disassemble to see generated instructions

aarch64-none-elf-objdump \-d kernel8.elf | head \-60

\# Expected: first instructions are from boot.S (\_start)

\# Check symbols (verify kernel\_main, \_\_bss\_start, \_\_bss\_end exist)

aarch64-none-elf-nm kernel8.elf | sort

\# Check binary size

ls \-la kernel8.img

\# Phase 1 should be \~2-4 KB

If `.text` does not start at `0x80000`, the linker script is wrong. If `_start` is not the first symbol, the `.text.boot` section ordering is wrong.

---

## 6\. Deployment to SD Card

### Manual Deployment

\# Insert SD card, identify mount point

lsblk

\# Or it may auto-mount

\# Copy kernel image

cp kernel8.img /media/$USER/boot/

\# IMPORTANT: flush write buffers before removing

sync

\# Safely eject

sudo eject /dev/sdX

\# Or: right-click and "Safely Remove" in file manager

### Script-Based Deployment

Create a `deploy.sh` script:

\#\!/bin/bash

set \-e

MOUNT\_POINT="/media/$USER/boot"

if \[ \! \-d "$MOUNT\_POINT" \]; then

    echo "ERROR: SD card not mounted at $MOUNT\_POINT"

    echo "Insert SD card or adjust MOUNT\_POINT variable."

    exit 1

fi

make clean && make

cp kernel8.img "$MOUNT\_POINT/"

sync

echo ""

echo "Deployed kernel8.img ($(stat \-c%s kernel8.img) bytes)"

echo "Safe to eject SD card."

chmod \+x deploy.sh

./deploy.sh

---

## 7\. Execution and First Boot

### Pre-Flight Checklist

Before powering on the Pi for the first time:

- [ ] SD card contains: `bootcode.bin`, `start4.elf`, `fixup4.dat`, `config.txt`, `kernel8.img`  
- [ ] `config.txt` contains `arm_64bit=1` and `enable_uart=1`  
- [ ] Serial adapter is connected: Pi GPIO14 to adapter RXD, Pi GPIO15 to adapter TXD, GND to GND  
- [ ] Serial adapter is plugged into host machine USB  
- [ ] Terminal emulator is open and connected to the serial port at 115200 8N1  
- [ ] Pi is powered by its own USB-C supply (not through the serial adapter)

### Power On

1. Insert the SD card into the Pi 4  
2. Make sure the serial terminal is open and waiting  
3. Connect the USB-C power cable to the Pi

### What You Should See

**On the HDMI display (if connected):** The four-color "rainbow" splash screen appears briefly, then the screen goes black. This is normal \-- we have no framebuffer driver yet.

**On the serial terminal:**

\================================

  VitOS v0.1 \-- Phase 1

  Raspberry Pi 4 Bare Metal

\================================

\[boot\] UART initialized

\[boot\] Running on core 0 (EL2)

\[boot\] Hello from VitOS\!

UART echo mode (type something):

\>

**Type a few characters.** Each character should echo back immediately. Press Enter to get a new `>` prompt.

### Power Off

There is no shutdown procedure for bare metal \-- simply disconnect the USB-C power cable. The SD card is not being written to during operation, so there is no corruption risk.

---

## 8\. Debugging Techniques

### Level 1: UART Printf Debugging

This is your primary debugging method for all phases. The pattern:

uart\_puts("\[debug\] About to configure GIC...\\n");

// ... code that might fail ...

uart\_puts("\[debug\] GIC configured successfully\\n");

For register values:

uart\_puts("\[debug\] AUX\_MU\_LSR\_REG \= ");

uart\_hex(mmio\_read(AUX\_MU\_LSR\_REG));

uart\_puts("\\n");

**Pro tip:** Use a consistent prefix like `[boot]`, `[uart]`, `[gpio]` so you can quickly scan output for the subsystem that failed.

### Level 2: LED Blink Codes

When UART itself is not working (wrong baud rate, broken wiring, code crashes before UART init), fall back to the activity LED or an external LED on a GPIO pin.

The Pi 4's on-board green ACT LED is on GPIO 42, but accessing it requires mailbox interface. Simpler approach \-- connect an LED \+ 330 ohm resistor to GPIO 21 (physical pin 40\) and GND:

// Emergency debug: blink LED on GPIO 21

\#define GPIO\_SET0   (PERIPHERAL\_BASE \+ 0x20001C)

\#define GPIO\_CLR0   (PERIPHERAL\_BASE \+ 0x200028)

void panic\_blink(int count)

{

    // Set GPIO 21 as output

    gpio\_pin\_set\_func(21, 1);  // function 1 \= output

    while (1) {

        for (int i \= 0; i \< count; i++) {

            mmio\_write(GPIO\_SET0, 1 \<\< 21);  // LED on

            delay(200000);

            mmio\_write(GPIO\_CLR0, 1 \<\< 21);  // LED off

            delay(200000);

        }

        delay(1000000);  // pause between groups

    }

}

Blink pattern encodes the error: 1 blink \= boot stub reached C code, 2 blinks \= UART init failed, 3 blinks \= unexpected exception, etc.

### Level 3: Examining the ELF Binary

When something is wrong with the build, inspect the output:

\# Disassemble \-- see if the entry point code looks correct

aarch64-none-elf-objdump \-d kernel8.elf | less

\# Check that \_start is at 0x80000

aarch64-none-elf-nm kernel8.elf | grep \_start

\# Check section layout

aarch64-none-elf-objdump \-h kernel8.elf

\# Check that .text.boot is the first section

\# Its VMA (Virtual Memory Address) should be 0x80000

### Level 4: QEMU (Limited but Useful)

QEMU can emulate the Pi 4 with limited peripheral support. UART works, which makes it useful for testing boot code without touching hardware:

\# Install QEMU

sudo apt install qemu-system-aarch64

\# Run your kernel

qemu-system-aarch64 \\

    \-M raspi4b \\

    \-m 2G \\

    \-serial stdio \\

    \-kernel kernel8.img \\

    \-nographic

\# Ctrl-A, X to exit QEMU

Limitations: QEMU's raspi4b machine does not emulate all BCM2711 peripherals accurately. GPIO, SD card, and USB may not work. Use it for boot/UART testing only; always verify on real hardware.

### Level 5: JTAG (Advanced, Phase 3+)

Not needed for Phase 1 but documented here for reference.

The Pi 4 exposes JTAG on GPIO pins 22-27 (ALT4 function). You need:

- A JTAG adapter (Segger J-Link, or a cheap FT2232H-based adapter)  
- OpenOCD configured for BCM2711

\# Install OpenOCD

sudo apt install openocd

\# You need a config file for Pi 4 \-- these are community-maintained

\# and require GPIO ALT4 to be configured in your boot code first

JTAG allows single-step debugging, breakpoints, memory inspection, and register examination through GDB:

\# In one terminal: start OpenOCD

openocd \-f interface/jlink.cfg \-f target/bcm2711.cfg

\# In another terminal: connect GDB

aarch64-none-elf-gdb kernel8.elf

(gdb) target remote :3333

(gdb) break kernel\_main

(gdb) continue

This is powerful but the setup is fragile. Defer until you need it.

---

## 9\. Advanced: TFTP Network Boot (Skip SD Card Swaps)

The Pi 4 can boot `kernel8.img` over the network using TFTP. This eliminates the SD-card-swap cycle entirely.

### How It Works

1. Pi 4 still needs the SD card with firmware files (`bootcode.bin`, `start4.elf`, etc.)  
2. But `config.txt` tells it to load the kernel via TFTP from your host machine  
3. Your Makefile copies `kernel8.img` to the TFTP directory  
4. You power-cycle the Pi (or use a watchdog/GPIO reset), and it loads the new kernel over Ethernet in \~1 second

### Setup

**On the host machine:**

\# Install TFTP server

sudo apt install tftpd-hpa

\# Configure TFTP root directory

sudo mkdir \-p /srv/tftp

sudo chown tftp:tftp /srv/tftp

\# Verify TFTP is running

sudo systemctl status tftpd-hpa

**On the SD card, modify `config.txt`:**

arm\_64bit=1

enable\_uart=1

\# Network boot: load kernel from TFTP server

\# Replace with your host machine's IP address

kernel\_address=0x80000

Note: The exact TFTP boot configuration depends on whether you use U-Boot as an intermediate bootloader or configure the Pi firmware directly. The simplest approach is to use U-Boot on the SD card, which provides native TFTP support:

\# In U-Boot console:

setenv serverip 192.168.1.100

setenv ipaddr 192.168.1.200

tftp 0x80000 kernel8.img

go 0x80000

**Add a Makefile target:**

TFTP\_DIR \= /srv/tftp

tftp: kernel8.img

	sudo cp kernel8.img $(TFTP\_DIR)/

	@echo "kernel8.img deployed to TFTP server."

	@echo "Power-cycle Pi to load new kernel."

### Alternative: Serial Upload (XMODEM)

If you implement an XMODEM receiver in your bootloader (a Phase 9+ task), you can upload new kernels over the same UART cable without touching the SD card at all. This is the fastest possible iteration cycle but requires significant bootloader code.

---

## 10\. Advanced: QEMU Emulation

### Basic QEMU Usage

\# Run with serial output to terminal

qemu-system-aarch64 \\

    \-M raspi4b \\

    \-m 2G \\

    \-serial stdio \\

    \-kernel kernel8.img \\

    \-nographic

\# Run with GDB server (for debugging with gdb)

qemu-system-aarch64 \\

    \-M raspi4b \\

    \-m 2G \\

    \-serial stdio \\

    \-kernel kernel8.img \\

    \-nographic \\

    \-S \-gdb tcp::1234

### QEMU \+ GDB Debugging Session

In one terminal, start QEMU with `-S -gdb tcp::1234` (the `-S` flag pauses execution at start).

In another terminal:

aarch64-none-elf-gdb kernel8.elf

(gdb) target remote :1234

(gdb) break kernel\_main

(gdb) continue

\# When breakpoint hits:

(gdb) info registers           \# Show all registers

(gdb) x/10i $pc               \# Disassemble 10 instructions at current PC

(gdb) x/10x 0xFE215000        \# Examine UART registers in memory

(gdb) step                     \# Single step (source level)

(gdb) stepi                    \# Single step (instruction level)

(gdb) print/x $sp              \# Print stack pointer in hex

This is extremely useful for debugging boot code and exception handling before you have UART working. QEMU gives you full visibility into CPU state that you cannot get from serial output alone.

### QEMU Limitations for Pi 4

| Works | Does Not Work (or Limited) |
| :---- | :---- |
| CPU execution (AArch64) | SD card / EMMC |
| Mini UART (serial) | USB |
| Basic memory | GPIO (no physical LEDs) |
| Timer interrupts | GIC-400 (partially) |
| GDB debugging | Framebuffer (partially) |

Always test on real hardware before considering a phase complete.

---

## 11\. Troubleshooting Reference

### No output on serial terminal at all

| Check | Fix |
| :---- | :---- |
| TX/RX wires swapped | Pi TX (pin 8\) goes to adapter RX, Pi RX (pin 10\) goes to adapter TX |
| GND not connected | Must connect Pi GND (pin 6\) to adapter GND |
| Wrong baud rate in terminal | Must be 115200 |
| Wrong serial device | Check `ls /dev/ttyUSB*` or `ls /dev/ttyACM*` |
| Hardware flow control enabled | Disable in minicom: Ctrl-A, O, Serial port setup, F to toggle |
| `config.txt` missing `enable_uart=1` | Add it; this fixes VPU clock and enables Mini UART |
| `kernel8.img` not on SD card | Check filename is exactly `kernel8.img` (case sensitive) |
| Wrong firmware files | Must use `start4.elf` (not `start.elf`) for Pi 4 |
| SD card not FAT32 | Reformat as FAT32 with MBR partition table |

### Garbage / corrupted characters on serial

| Check | Fix |
| :---- | :---- |
| Baud rate mismatch | Try divisor 270 instead of 541 in `AUX_MU_BAUD_REG` |
| VPU clock not fixed | Ensure `enable_uart=1` is in `config.txt` |
| 5V adapter connected | Must be 3.3V logic; 5V will also cause random garbage and may damage the Pi |
| Noisy/long wires | Keep UART wires short (under 30cm); avoid running next to power cables |

### Rainbow screen appears, then nothing

| Check | Fix |
| :---- | :---- |
| `kernel8.img` present but code hangs | Add LED blink at start of `kernel_main()` to verify C code runs |
| Linker script wrong | Verify `.text` starts at `0x80000` with `objdump -h` |
| `.text.boot` not first | Check that `KEEP(*(.text.boot))` is the first entry in `.text` section |
| BSS zeroing loop is infinite | Check the loop termination condition in `boot.S` |
| Stack pointer wrong | Verify `mov sp, x1` with `ldr x1, =_start` sets SP to 0x80000 |

### No rainbow screen at all

| Check | Fix |
| :---- | :---- |
| SD card not detected | Try a different SD card; reformat as FAT32 MBR |
| Missing firmware files | Need `bootcode.bin` AND `start4.elf` AND `fixup4.dat` |
| HDMI not connected (if checking visually) | Rainbow only appears on HDMI; serial output works without HDMI |
| Pi not powering on | Use official USB-C power supply (5V/3A); some cables are charge-only |

### Build errors

| Error | Fix |
| :---- | :---- |
| `aarch64-none-elf-gcc: command not found` | Toolchain not installed or not on PATH |
| `undefined reference to kernel_main` | `kernel.c` not compiled or not included in link step |
| `cannot find -lgcc` | Add `-nostdlib` to linker flags |
| `multiple definition of _start` | Check only `boot.S` defines `_start` |
| Warning: `implicit declaration of function` | Missing `#include` for the header declaring that function |

---

## Quick Reference Card

BUILD:        make clean && make

DEPLOY:       cp kernel8.img /media/$USER/boot/ && sync

SERIAL:       picocom \-b 115200 /dev/ttyUSB0

QEMU:         qemu-system-aarch64 \-M raspi4b \-m 2G \-serial stdio \-kernel kernel8.img \-nographic

DISASM:       aarch64-none-elf-objdump \-d kernel8.elf | less

SYMBOLS:      aarch64-none-elf-nm kernel8.elf | sort

SECTIONS:     aarch64-none-elf-objdump \-h kernel8.elf

SIZE:         ls \-la kernel8.img  
