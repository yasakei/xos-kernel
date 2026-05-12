# Makefile for XOS Kernel

CC = gcc
LD = ld
NASM = nasm

CFLAGS = -ffreestanding -fno-stack-protector -fno-builtin -Wall -Wextra -m64 -mno-red-zone
LDFLAGS = -m elf_x86_64 -T src/linker.ld -nostdlib
NASMFLAGS = -f bin

# Output files
BOOTLOADER = build/bootloader.bin
KERNEL     = build/kernel.bin
OS_IMAGE   = build/os.img
STORAGE_IMAGE = build/storage.img

all: $(OS_IMAGE) $(STORAGE_IMAGE)

# Create build directory
build:
	@mkdir -p build

# ── Bootloader ────────────────────────────────────────────────────────────────
$(BOOTLOADER): src/boot/boot.asm | build
	$(NASM) $(NASMFLAGS) src/boot/boot.asm -o $(BOOTLOADER)

build/page_tables.bin: src/boot/page_tables.asm | build
	$(NASM) -f bin src/boot/page_tables.asm -o build/page_tables.bin

# ── Kernel arch ───────────────────────────────────────────────────────────────
build/kernel_entry.o: src/kernel/arch/kernel_entry.asm | build
	$(NASM) -f elf64 src/kernel/arch/kernel_entry.asm -o build/kernel_entry.o

build/interrupt_stubs.o: src/kernel/arch/interrupt_stubs.asm | build
	$(NASM) -f elf64 src/kernel/arch/interrupt_stubs.asm -o build/interrupt_stubs.o

# ── Arch ──────────────────────────────────────────────────────────────────────
build/gdt.o: src/kernel/arch/gdt.c | build
	$(CC) $(CFLAGS) -c src/kernel/arch/gdt.c -o build/gdt.o

build/usermode.o: src/kernel/arch/usermode.asm | build
	$(NASM) -f elf64 src/kernel/arch/usermode.asm -o build/usermode.o

build/user_test.o: src/kernel/arch/user_test.asm | build
	$(NASM) -f elf64 src/kernel/arch/user_test.asm -o build/user_test.o

build/syscall.o: src/kernel/arch/syscall.c | build
	$(CC) $(CFLAGS) -c src/kernel/arch/syscall.c -o build/syscall.o

# ── Kernel core ───────────────────────────────────────────────────────────────
build/kernel.o: src/kernel/kernel.c | build
	$(CC) $(CFLAGS) -c src/kernel/kernel.c -o build/kernel.o

build/idt.o: src/kernel/idt.c | build
	$(CC) $(CFLAGS) -c src/kernel/idt.c -o build/idt.o

# ── Drivers ───────────────────────────────────────────────────────────────────
build/vga.o: src/kernel/drivers/display/vga.c | build
	$(CC) $(CFLAGS) -c src/kernel/drivers/display/vga.c -o build/vga.o

build/serial.o: src/kernel/drivers/serial/serial.c | build
	$(CC) $(CFLAGS) -c src/kernel/drivers/serial/serial.c -o build/serial.o

build/keyboard.o: src/kernel/drivers/input/keyboard.c | build
	$(CC) $(CFLAGS) -c src/kernel/drivers/input/keyboard.c -o build/keyboard.o

build/pci.o: src/kernel/drivers/bus/pci.c | build
	$(CC) $(CFLAGS) -c src/kernel/drivers/bus/pci.c -o build/pci.o

build/ata.o: src/kernel/drivers/storage/ata.c | build
	$(CC) $(CFLAGS) -c src/kernel/drivers/storage/ata.c -o build/ata.o

# ── Timer ─────────────────────────────────────────────────────────────────────
build/pit.o: src/kernel/drivers/timer/pit.c | build
	$(CC) $(CFLAGS) -c src/kernel/drivers/timer/pit.c -o build/pit.o

# ── Shell ─────────────────────────────────────────────────────────────────────
build/shell.o: src/kernel/shell/shell.c | build
	$(CC) $(CFLAGS) -c src/kernel/shell/shell.c -o build/shell.o

# ── Scheduler ────────────────────────────────────────────────────────────────
build/scheduler.o: src/kernel/sched/scheduler.c | build
	$(CC) $(CFLAGS) -c src/kernel/sched/scheduler.c -o build/scheduler.o

build/context_switch.o: src/kernel/sched/context_switch.asm | build
	$(NASM) -f elf64 src/kernel/sched/context_switch.asm -o build/context_switch.o

build/task_bootstrap.o: src/kernel/sched/task_bootstrap.asm | build
	$(NASM) -f elf64 src/kernel/sched/task_bootstrap.asm -o build/task_bootstrap.o

# ── Filesystem ────────────────────────────────────────────────────────────────
build/partition.o: src/kernel/fs/partition.c | build
	$(CC) $(CFLAGS) -c src/kernel/fs/partition.c -o build/partition.o

build/fat32.o: src/kernel/fs/fat32.c | build
	$(CC) $(CFLAGS) -c src/kernel/fs/fat32.c -o build/fat32.o

# ── Memory management ─────────────────────────────────────────────────────────
build/pmm.o: src/kernel/mm/pmm.c | build
	$(CC) $(CFLAGS) -c src/kernel/mm/pmm.c -o build/pmm.o

build/heap.o: src/kernel/mm/heap.c | build
	$(CC) $(CFLAGS) -c src/kernel/mm/heap.c -o build/heap.o

# ── Lib ───────────────────────────────────────────────────────────────────────
build/printf.o: src/kernel/lib/printf.c | build
	$(CC) $(CFLAGS) -c src/kernel/lib/printf.c -o build/printf.o

build/debuglog.o: src/kernel/lib/debuglog.c | build
	$(CC) $(CFLAGS) -c src/kernel/lib/debuglog.c -o build/debuglog.o

# ── Link ──────────────────────────────────────────────────────────────────────
OBJS = build/kernel_entry.o \
       build/interrupt_stubs.o \
       build/kernel.o \
       build/idt.o \
       build/gdt.o \
       build/usermode.o \
       build/user_test.o \
       build/syscall.o \
       build/vga.o \
       build/serial.o \
       build/keyboard.o \
       build/pci.o \
       build/ata.o \
       build/pit.o \
       build/partition.o \
       build/fat32.o \
       build/pmm.o \
       build/heap.o \
       build/printf.o \
       build/debuglog.o \
       build/scheduler.o \
       build/context_switch.o \
       build/task_bootstrap.o \
       build/shell.o

build/kernel_elf: $(OBJS) src/linker.ld | build
	$(LD) $(LDFLAGS) -o build/kernel_elf $(OBJS)

$(KERNEL): build/kernel_elf
	objcopy -O binary build/kernel_elf $(KERNEL)

# ── Images ────────────────────────────────────────────────────────────────────
$(OS_IMAGE): $(BOOTLOADER) $(KERNEL)
	dd if=/dev/zero of=$(OS_IMAGE) bs=512 count=131072
	dd if=$(BOOTLOADER) of=$(OS_IMAGE) conv=notrunc
	dd if=$(KERNEL) of=$(OS_IMAGE) seek=1 conv=notrunc
	@echo "OS image created: $(OS_IMAGE)"

$(STORAGE_IMAGE): | build
	python3 scripts/create_storage_image.py $(STORAGE_IMAGE)

clean:
	rm -rf build/

.PHONY: all clean
