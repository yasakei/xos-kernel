# Makefile for XOS Kernel

CC = gcc
LD = ld
NASM = nasm

CFLAGS = -ffreestanding -fno-stack-protector -fno-builtin -Wall -Wextra -m64 -mno-red-zone
LDFLAGS = -m elf_x86_64 -T src/linker.ld -nostdlib
NASMFLAGS = -f bin

# Output files
BOOTLOADER = build/bootloader.bin
KERNEL = build/kernel.bin
OS_IMAGE = build/os.img

all: $(OS_IMAGE)

# Create build directory
build:
	@mkdir -p build

# Build bootloader
$(BOOTLOADER): src/boot/boot.asm | build
	$(NASM) $(NASMFLAGS) src/boot/boot.asm -o $(BOOTLOADER)

# Build page tables
build/page_tables.bin: src/boot/page_tables.asm | build
	$(NASM) -f bin src/boot/page_tables.asm -o build/page_tables.bin

# Build kernel object files
build/kernel_entry.o: src/kernel/kernel_entry.asm | build
	$(NASM) -f elf64 src/kernel/kernel_entry.asm -o build/kernel_entry.o

build/interrupt_stubs.o: src/kernel/interrupt_stubs.asm | build
	$(NASM) -f elf64 src/kernel/interrupt_stubs.asm -o build/interrupt_stubs.o

build/kernel.o: src/kernel/kernel.c | build
	$(CC) $(CFLAGS) -c src/kernel/kernel.c -o build/kernel.o

build/idt.o: src/kernel/idt.c | build
	$(CC) $(CFLAGS) -c src/kernel/idt.c -o build/idt.o

build/vga.o: src/kernel/vga.c | build
	$(CC) $(CFLAGS) -c src/kernel/vga.c -o build/vga.o

build/serial.o: src/kernel/serial.c | build
	$(CC) $(CFLAGS) -c src/kernel/serial.c -o build/serial.o

build/printf.o: src/kernel/printf.c | build
	$(CC) $(CFLAGS) -c src/kernel/printf.c -o build/printf.o

build/keyboard.o: src/kernel/keyboard.c | build
	$(CC) $(CFLAGS) -c src/kernel/keyboard.c -o build/keyboard.o

build/pmm.o: src/kernel/pmm.c | build
	$(CC) $(CFLAGS) -c src/kernel/pmm.c -o build/pmm.o

build/heap.o: src/kernel/heap.c | build
	$(CC) $(CFLAGS) -c src/kernel/heap.c -o build/heap.o

build/pci.o: src/kernel/pci.c | build
	$(CC) $(CFLAGS) -c src/kernel/pci.c -o build/pci.o

build/debuglog.o: src/kernel/debuglog.c | build
	$(CC) $(CFLAGS) -c src/kernel/debuglog.c -o build/debuglog.o

build/ata.o: src/kernel/ata.c | build
	$(CC) $(CFLAGS) -c src/kernel/ata.c -o build/ata.o

build/partition.o: src/kernel/partition.c | build
	$(CC) $(CFLAGS) -c src/kernel/partition.c -o build/partition.o

build/fat32.o: src/kernel/fat32.c | build
	$(CC) $(CFLAGS) -c src/kernel/fat32.c -o build/fat32.o

# Link kernel
build/kernel_elf: build/kernel_entry.o build/interrupt_stubs.o build/kernel.o build/idt.o build/vga.o build/serial.o build/printf.o build/keyboard.o build/pmm.o build/heap.o build/pci.o build/debuglog.o build/ata.o build/partition.o build/fat32.o src/linker.ld | build
	$(LD) $(LDFLAGS) -o build/kernel_elf build/kernel_entry.o build/interrupt_stubs.o build/kernel.o build/idt.o build/vga.o build/serial.o build/printf.o build/keyboard.o build/pmm.o build/heap.o build/pci.o build/debuglog.o build/ata.o build/partition.o build/fat32.o

# Extract binary from ELF
$(KERNEL): build/kernel_elf
	objcopy -O binary build/kernel_elf $(KERNEL)

# Create OS image
$(OS_IMAGE): $(BOOTLOADER) $(KERNEL)
	dd if=/dev/zero of=$(OS_IMAGE) bs=512 count=2880
	dd if=$(BOOTLOADER) of=$(OS_IMAGE) conv=notrunc
	dd if=$(KERNEL) of=$(OS_IMAGE) seek=1 conv=notrunc
	@echo "OS image created: $(OS_IMAGE)"

clean:
	rm -rf build/

.PHONY: all clean
