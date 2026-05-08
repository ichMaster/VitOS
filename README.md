# VitOS

Bare-metal educational operating system for Raspberry Pi 4 Model B, written in C and AArch64 assembly. No Linux, no RTOS, no standard library — everything is built from scratch, talking directly to hardware registers.

**Current phase:** Phase 1 — Boot + UART "Hello World"

## What Does It Do?

When you power on the Pi 4 with VitOS on the SD card, it:

1. Boots into your own code (no OS underneath)
2. Initializes the serial port (Mini UART)
3. Prints a boot banner over the serial connection
4. Enters an interactive echo loop — anything you type is echoed back

All output goes to a **serial terminal** on your laptop, not HDMI. You connect to the Pi via a USB-to-serial cable.

## What You Need

### Hardware

| Item | What It Is | Where to Get It |
|------|-----------|-----------------|
| Raspberry Pi 4 Model B | The target board (any RAM size works) | You likely have this already |
| USB-to-TTL serial cable (3.3V) | Connects Pi's serial pins to your laptop's USB port | Amazon/AliExpress, search "FTDI FT232RL 3.3V" or "CP2102 3.3V" (~$5-10) |
| MicroSD card (16GB+) | Boot media for the Pi | Any brand, must be FAT32 formatted |
| USB-C power supply (5V/3A) | Powers the Pi 4 | Official RPi power supply or equivalent |
| SD card reader | To write files from your laptop to the MicroSD | Built into most laptops, or a USB adapter |

> **WARNING:** Your serial cable MUST be 3.3V. A 5V serial cable will permanently damage the Pi's GPIO pins. Check the cable's documentation before connecting.

> **You do NOT need** a monitor, HDMI cable, keyboard, or mouse for Phase 1. Everything happens over the serial connection.

### Software (on your development machine)

| Software | Purpose |
|----------|---------|
| `aarch64-none-elf-gcc` | Cross-compiler — compiles C code on your machine into ARM64 binaries for the Pi |
| `make` | Build automation tool |
| A serial terminal program | To see output from the Pi (`screen`, `minicom`, or PuTTY) |

## Step-by-Step Setup

### Step 1: Install the Cross-Compiler

You're compiling code on your laptop (x86 or ARM Mac) that will run on the Pi's ARM Cortex-A72. This requires a **cross-compiler** — a special version of GCC that outputs ARM64 machine code.

**macOS (Homebrew):**

```bash
# Install the ARM bare-metal toolchain
brew install --cask gcc-aarch64-embedded
```

If that doesn't work, download directly from ARM:
1. Go to https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
2. Download the **AArch64 bare-metal target (aarch64-none-elf)** package for your OS
3. Extract it and add the `bin/` directory to your `PATH`

**Ubuntu/Debian Linux:**

```bash
sudo apt update
sudo apt install gcc-aarch64-none-elf binutils-aarch64-none-elf make
```

**Verify it works:**

```bash
aarch64-none-elf-gcc --version
```

You should see something like `aarch64-none-elf-gcc (Arm GNU Toolchain ...) 13.x.x`. If you get "command not found", the toolchain isn't installed or isn't in your `PATH`.

### Step 2: Build VitOS

```bash
# Clone the repo (if you haven't already)
git clone git@github.com:ichMaster/VitOS.git
cd VitOS

# Build — this compiles everything and produces kernel8.img
make clean && make
```

**What happens during the build:**

```
boot.S       → boot.o        (assembly → object file)
gpio.c       → gpio.o        (C → object file)
uart.c       → uart.o        (C → object file)
kernel.c     → kernel.o      (C → object file)
    ↓
All .o files → kernel8.elf   (linked together using link.ld)
    ↓
kernel8.elf  → kernel8.img   (stripped to raw binary — this is what the Pi loads)
```

**Verify the build output:**

```bash
# Check that the .text section starts at 0x80000 (where the Pi loads the binary)
aarch64-none-elf-objdump -h kernel8.elf

# View the disassembled instructions (optional, for the curious)
aarch64-none-elf-objdump -d kernel8.elf

# Check symbol addresses
aarch64-none-elf-nm kernel8.elf | sort
```

### Step 3: Prepare the SD Card

The Pi 4 boots from a FAT32-formatted MicroSD card. It needs GPU firmware files (from Raspberry Pi) plus your kernel.

#### 3a. Format the SD Card

**macOS:**
1. Insert the MicroSD card
2. Open **Disk Utility**
3. Select the SD card (careful — don't format the wrong drive!)
4. Click **Erase**, choose format **MS-DOS (FAT)**, scheme **Master Boot Record**
5. Click Erase

**Linux:**
```bash
# Find your SD card device (usually /dev/sdX or /dev/mmcblkX)
lsblk

# Format as FAT32 (replace /dev/sdX with your actual device — BE CAREFUL)
sudo mkfs.vfat -F 32 /dev/sdX1
```

#### 3b. Download Raspberry Pi Firmware

The Pi 4's GPU needs three firmware files to boot. Download them from the official repo:

```bash
# Create a temporary directory for firmware
mkdir -p /tmp/rpi-firmware && cd /tmp/rpi-firmware

# Download the three required files
curl -LO https://github.com/raspberrypi/firmware/raw/master/boot/bootcode.bin
curl -LO https://github.com/raspberrypi/firmware/raw/master/boot/start4.elf
curl -LO https://github.com/raspberrypi/firmware/raw/master/boot/fixup4.dat
```

**What these files do:**
- `bootcode.bin` — First-stage bootloader, loaded by the GPU from silicon ROM
- `start4.elf` — GPU firmware for Pi 4, reads `config.txt`, loads your kernel
- `fixup4.dat` — Memory split configuration between GPU and CPU

#### 3c. Copy Everything to the SD Card

Your SD card should end up with exactly these 5 files in the root directory:

```
SD Card (FAT32)
├── bootcode.bin      ← from Raspberry Pi firmware
├── start4.elf        ← from Raspberry Pi firmware
├── fixup4.dat        ← from Raspberry Pi firmware
├── config.txt        ← from this project (tells GPU to use 64-bit mode)
└── kernel8.img       ← from this project (your OS!)
```

```bash
# Mount the SD card and copy files
# Replace /Volumes/BOOT with your SD card mount point

# Firmware files
cp /tmp/rpi-firmware/bootcode.bin /Volumes/BOOT/
cp /tmp/rpi-firmware/start4.elf /Volumes/BOOT/
cp /tmp/rpi-firmware/fixup4.dat /Volumes/BOOT/

# Your OS
cp kernel8.img /Volumes/BOOT/
cp config.txt /Volumes/BOOT/

# Make sure everything is written to the card
sync
```

**On Linux**, the mount point is typically `/media/$USER/BOOT` or similar.

Eject the SD card safely before removing it.

### Step 4: Connect the Serial Cable

This is how your laptop talks to the Pi. The Pi's Mini UART outputs text on GPIO pins, and the serial cable converts it to USB for your laptop.

#### Wiring Diagram

```
USB-to-TTL Cable          Raspberry Pi 4 GPIO Header
                          (pin 1 is closest to the corner)

                          Pin 1  Pin 2
                            ●      ●
                            ●      ●
                            ●  ←── ● Pin 6  (GND)   ← Cable GND (black)
                            ●      ● Pin 8  (TXD)   ← Cable RX  (white/yellow)
                            ●      ● Pin 10 (RXD)   ← Cable TX  (green)
                            ...
```

| Cable Wire | Color (typical) | Connect To | Pi 4 Physical Pin |
|------------|----------------|------------|-------------------|
| GND        | Black          | Ground     | Pin 6             |
| RX (receive) | White/Yellow | GPIO 14 (Pi's TX) | Pin 8      |
| TX (transmit) | Green      | GPIO 15 (Pi's RX) | Pin 10     |

> **Key concept:** TX and RX cross over. The Pi's TX (transmit) connects to the cable's RX (receive), and vice versa. This is the most common wiring mistake.

> **Do NOT connect the cable's VCC/power wire (red) to the Pi.** Power the Pi from its USB-C port instead.

### Step 5: Open a Serial Terminal

**Before powering on the Pi**, open a serial terminal on your laptop so you don't miss the boot output.

**macOS:**
```bash
# Find the serial device
ls /dev/tty.usbserial-*
# or
ls /dev/tty.SLAB_USBtoUART*

# Connect (replace with your actual device name)
screen /dev/tty.usbserial-1420 115200
```

**Linux:**
```bash
# Find the serial device
ls /dev/ttyUSB*

# Connect
screen /dev/ttyUSB0 115200

# If permission denied:
sudo usermod -aG dialout $USER
# Then log out and back in, or run:
sudo screen /dev/ttyUSB0 115200
```

**Windows (PuTTY):**
1. Open Device Manager, expand "Ports (COM & LPT)", note the COM port number
2. Open PuTTY, select "Serial", enter the COM port and speed 115200
3. Click Open

**Serial settings:** 115200 baud, 8 data bits, no parity, 1 stop bit (8N1).

### Step 6: Power On and Test

1. Insert the SD card into the Pi 4
2. Make sure the serial cable is connected
3. Make sure the serial terminal is open on your laptop
4. Plug in the USB-C power cable

You should see:

```
================================
  VitOS v0.1 -- Phase 1
  Raspberry Pi 4 Bare Metal
================================

[boot] UART initialized
[boot] Running on core 0 (EL2)
[boot] Hello from VitOS!

UART echo mode (type something):
>
```

Type characters — they echo back. Press Enter for a new prompt line.

**To exit `screen`:** Press `Ctrl-A`, then `K`, then `Y`.

## Project Structure

```
VitOS/
├── boot.S          AArch64 assembly — first code that runs on the CPU
├── common.h        Type definitions (uint32_t, etc.) for freestanding C
├── gpio.h / gpio.c GPIO and MMIO functions (how the CPU talks to hardware)
├── uart.h / uart.c Mini UART serial driver (send/receive characters)
├── kernel.c        Main entry point — prints banner, runs echo loop
├── link.ld         Linker script — controls memory layout of the binary
├── Makefile        Build system — type "make" to compile everything
├── config.txt      Pi 4 boot config — tells GPU to use 64-bit mode
└── docs/           Learning guides and reference material
```

### How the Boot Sequence Works

```
Power On
   │
   ▼
GPU ROM (burned into silicon, you can't change this)
   │
   ▼
GPU loads bootcode.bin from SD card
   │
   ▼
GPU loads start4.elf, reads config.txt
   │  config.txt says arm_64bit=1 → switches CPU to AArch64 mode
   │  config.txt says enable_uart=1 → fixes VPU clock to 500 MHz
   │
   ▼
GPU loads kernel8.img into RAM at address 0x80000
   │
   ▼
GPU releases CPU core 0 → execution starts at 0x80000
   │
   ▼
boot.S runs:
   ├── Reads core ID — parks cores 1, 2, 3 (puts them to sleep)
   ├── Sets stack pointer to 0x80000 (stack grows downward)
   ├── Zeros the BSS section (uninitialized global variables)
   └── Calls kernel_main() in kernel.c
   │
   ▼
kernel.c runs:
   ├── Initializes Mini UART (serial port)
   ├── Prints boot banner
   └── Enters echo loop (waits for your keystrokes)
```

## QEMU Testing (Optional)

You can do a quick sanity check with QEMU before deploying to real hardware. Note that QEMU's Pi 4 emulation is limited — UART works but many peripherals don't.

```bash
# Install QEMU (if not already installed)
# macOS:
brew install qemu
# Linux:
sudo apt install qemu-system-aarch64

# Run VitOS in QEMU
qemu-system-aarch64 -M raspi4b -m 2G -serial stdio -kernel kernel8.img -nographic

# Exit QEMU: press Ctrl-A, then X
```

## Quick Iteration Workflow

After making code changes, the cycle is:

```bash
# 1. Rebuild
make clean && make

# 2. Copy to SD card (adjust mount point for your system)
cp kernel8.img /Volumes/BOOT/ && sync

# 3. Eject SD card safely
diskutil eject /Volumes/BOOT    # macOS
# or: sudo umount /media/$USER/BOOT   # Linux

# 4. Move SD card to Pi, power cycle, check serial output
```

## Troubleshooting

### No output on serial terminal

- **Check wiring.** TX and RX must cross: Pi's TX (pin 8) → cable's RX, Pi's RX (pin 10) → cable's TX. This is the #1 mistake.
- **Check baud rate.** Must be 115200. If your terminal is set to 9600 or anything else, you'll see nothing or garbage.
- **Check the serial device.** Run `ls /dev/tty.usb*` (macOS) or `ls /dev/ttyUSB*` (Linux). If nothing shows up, your cable driver may not be installed.
- **Check SD card contents.** All 5 files must be present: `bootcode.bin`, `start4.elf`, `fixup4.dat`, `config.txt`, `kernel8.img`.
- **Try a different USB port.** Some USB hubs don't work well with serial adapters.

### Garbage characters on serial terminal

- The VPU clock speed might be 250 MHz instead of 500 MHz. Open `uart.c`, find `mmio_write(AUX_MU_BAUD_REG, 541)`, and change `541` to `270`. Rebuild and re-deploy.

### Pi shows rainbow screen on HDMI but no serial output

- The GPU firmware is loading but your kernel isn't running correctly. Verify `kernel8.img` is on the SD card (not `kernel.img` or `kernel7.img` — the "8" matters, it signals 64-bit mode).

### Pi doesn't boot at all (no activity LED blink, no HDMI)

- SD card might not be FAT32 with MBR partition table. Reformat it.
- Missing firmware files. Make sure `bootcode.bin` and `start4.elf` are in the root of the SD card.
- Bad SD card. Try a different one.

### `make` says "command not found"

- The cross-compiler isn't installed or isn't in your `PATH`. See Step 1 above.
- If you installed via the ARM download, add the `bin/` folder to your PATH:
  ```bash
  export PATH="$PATH:/path/to/arm-gnu-toolchain/bin"
  ```

### `screen` says "permission denied"

- On Linux, add yourself to the `dialout` group:
  ```bash
  sudo usermod -aG dialout $USER
  ```
  Then log out and back in.

## Key Concepts for Beginners

### What is "bare metal"?

Normally, your code runs on top of an operating system (Linux, macOS, Windows) that manages hardware for you. In bare-metal programming, there is no OS — your code IS the first thing that runs on the CPU. You talk to hardware by reading and writing specific memory addresses (called MMIO — Memory-Mapped I/O).

### What is a cross-compiler?

Your laptop has an x86 (Intel/AMD) or Apple Silicon (ARM) CPU. The Pi 4 has an ARM Cortex-A72 CPU. They speak different machine languages. A cross-compiler runs on your laptop but produces binaries that only the Pi can execute.

### Why `kernel8.img` and not `kernel.img`?

The Pi's GPU firmware uses the filename to decide which CPU mode to use:
- `kernel.img` → 32-bit ARMv6 mode (Pi 1)
- `kernel7.img` → 32-bit ARMv7 mode (Pi 2/3)
- `kernel8.img` → 64-bit AArch64 mode (Pi 3/4) — **this is what we use**

### Why no `printf`?

`printf` is part of the C standard library (`libc`), which depends on an OS to work. We have no OS, so we have no `libc`. Instead, we wrote our own `uart_puts()` that sends characters directly to the serial port hardware.

## References

- [BCM2711 ARM Peripherals datasheet](https://datasheets.raspberrypi.com/bcm2711/bcm2711-peripherals.pdf) — register map for all Pi 4 hardware
- [rpi4os.com](https://www.rpi4os.com/) — Pi 4 bare-metal tutorial (primary reference for this project)
- [Raspberry Pi OS by Matyukevich](https://s-matyukevich.github.io/raspberry-pi-os/) — excellent OS dev tutorial (Pi 3, adaptable)
- [Low Level Devel (YouTube)](https://www.youtube.com/watch?v=pd9AVmcRc6U) — video companion to rpi4os.com
- [Circle library](https://github.com/rsta2/circle) — mature bare-metal reference implementation

## License

See [LICENSE](LICENSE) for details.
