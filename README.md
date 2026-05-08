# VitOS

Bare-metal educational operating system for Raspberry Pi 4 Model B, written in C and AArch64 assembly.

## Hardware Target

- Raspberry Pi 4 Model B (any RAM size)
- BCM2711 SoC, ARM Cortex-A72 (AArch64)

## Prerequisites

- **Cross-compiler:** `aarch64-none-elf-gcc` (bare-metal variant)
  - Ubuntu/Debian: `sudo apt install gcc-aarch64-none-elf`
  - Or download from [ARM](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) — use the `aarch64-none-elf` target
- **Serial cable:** USB-to-TTL at 3.3V (FTDI FT232RL recommended). **Do NOT use 5V — it will damage the Pi.**
- **SD card:** 16GB+ MicroSD, FAT32 formatted

## Build

```bash
make clean && make
```

This produces `kernel8.img`.

## SD Card Setup

1. Format a MicroSD card as FAT32 (MBR partition table)
2. Download Raspberry Pi firmware files from the [official repo](https://github.com/raspberrypi/firmware/tree/master/boot) and copy to the card:
   - `bootcode.bin`
   - `start4.elf`
   - `fixup4.dat`
3. Copy from this project:
   - `kernel8.img`
   - `config.txt`
4. Insert the card into the Pi 4 and power on

## Serial Connection

Connect the USB-to-TTL cable to the Pi 4 GPIO header:

| Cable Wire | Pi 4 Pin | GPIO |
|------------|----------|------|
| TX         | Pin 10   | GPIO 15 (RXD) |
| RX         | Pin 8    | GPIO 14 (TXD) |
| GND        | Pin 6    | Ground |

Open a serial terminal:

```bash
# Linux
screen /dev/ttyUSB0 115200

# macOS
screen /dev/tty.usbserial-* 115200
```

You should see the VitOS boot banner and an interactive echo prompt.

## QEMU Testing (Limited)

```bash
qemu-system-aarch64 -M raspi4b -m 2G -serial stdio -kernel kernel8.img -nographic
```

Exit QEMU: `Ctrl-A`, then `X`

**Note:** QEMU's `raspi4b` machine has limited peripheral emulation. Always verify on real hardware.

## Troubleshooting

| Problem | Solution |
|---------|----------|
| No output on serial | Check TX/RX wiring (they cross — Pi TX to cable RX). Verify baud rate is 115200 8N1. |
| Garbage characters | Try changing `AUX_MU_BAUD_REG` from 541 to 270 (assumes 250 MHz VPU clock instead of 500 MHz). |
| No rainbow screen on HDMI | Missing `start4.elf` on SD card. Download from the RPi firmware repo. |
| Pi doesn't boot at all | Ensure SD card is FAT32 with MBR partition table. Verify `bootcode.bin`, `start4.elf`, and `fixup4.dat` are present. |
