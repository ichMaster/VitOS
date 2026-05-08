CC = aarch64-none-elf-gcc
LD = aarch64-none-elf-ld
OBJCOPY = aarch64-none-elf-objcopy

CFLAGS = -Wall -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles
ASFLAGS = -ffreestanding -nostdinc -nostdlib -nostartfiles

OBJS = boot.o gpio.o uart.o kernel.o

all: kernel8.img

boot.o: boot.S
	$(CC) $(ASFLAGS) -c boot.S -o boot.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel8.elf: $(OBJS)
	$(LD) -nostdlib -T link.ld -o kernel8.elf $(OBJS)

kernel8.img: kernel8.elf
	$(OBJCOPY) -O binary kernel8.elf kernel8.img

clean:
	rm -f *.o kernel8.elf kernel8.img

.PHONY: all clean
