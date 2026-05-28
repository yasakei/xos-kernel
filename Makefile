# -------------------------------------------------------------------
# mit license
# 
# copyright (c) 2026 xos
# 
# permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation
# files (the "software"), to deal in the software without
# restriction, including without limitation the rights to use,
# copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the software, and to permit persons to whom the
# software is furnished to do so, subject to the following
# conditions:
# 
# the above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the software.
# 
# the software is provided "as is", without warranty of any kind,
# express or implied, including but not limited to the warranties
# of merchantability, fitness for a particular purpose and
# noninfringement. in no event shall the authors or copyright
# holders be liable for any claim, damages or other liability,
# whether in an action of contract, tort or otherwise, arising
# from, out of or in connection with the software or the use or
# other dealings in the software.
# -------------------------------------------------------------------

# xos build system
# this is the main makefile that builds the kernel, bootloader,
# and disk images

CC = gcc
LD = ld
NASM = nasm

CFLAGS = -ffreestanding -fno-stack-protector -fno-builtin -Wall -Wextra -m64 -mno-red-zone
LDFLAGS = -m elf_x86_64 -T src/linker.ld -nostdlib
NASMFLAGS = -f bin

# debug logging defaults - override like 'make DEBUG_LOG=0 DEBUG_PRINT=1'
DEBUG_LOG ?= 1
DEBUG_PRINT ?= 0
CFLAGS += -DDEBUG_LOG_DEFAULT=$(DEBUG_LOG) -DDEBUG_PRINT_DEFAULT=$(DEBUG_PRINT)

# output files
BOOTLOADER = build/bootloader.bin
KERNEL     = build/kernel.bin
OS_IMAGE   = build/os.img
STORAGE_IMAGE = build/storage.img
USER_ELF = build/HELLO.ELF

all: $(OS_IMAGE) $(STORAGE_IMAGE)

# create the build directory
build:
	@mkdir -p build

# -- bootloader -------------------------------------------------------
# our custom 16-bit real mode bootloader - reads kernel from disk
$(BOOTLOADER): src/boot/boot.asm | build
	$(NASM) $(NASMFLAGS) src/boot/boot.asm -o $(BOOTLOADER)

build/page_tables.bin: src/boot/page_tables.asm | build
	$(NASM) -f bin src/boot/page_tables.asm -o build/page_tables.bin

# -- kernel assembly stubs --------------------------------------------
# these are small assembly entry points that bootstrap c code
build/kernel_entry.o: src/kernel/arch/kernel_entry.asm | build
	$(NASM) -f elf64 src/kernel/arch/kernel_entry.asm -o build/kernel_entry.o

build/interrupt_stubs.o: src/kernel/arch/interrupt_stubs.asm | build
	$(NASM) -f elf64 src/kernel/arch/interrupt_stubs.asm -o build/interrupt_stubs.o

# -- architecture-specific c code -------------------------------------
build/gdt.o: src/kernel/arch/gdt.c | build
	$(CC) $(CFLAGS) -c src/kernel/arch/gdt.c -o build/gdt.o

build/usermode.o: src/kernel/arch/usermode.asm | build
	$(NASM) -f elf64 src/kernel/arch/usermode.asm -o build/usermode.o

build/user_test.o: src/kernel/arch/user_test.asm | build
	$(NASM) -f elf64 src/kernel/arch/user_test.asm -o build/user_test.o

build/syscall.o: src/kernel/arch/syscall.c | build
	$(CC) $(CFLAGS) -c src/kernel/arch/syscall.c -o build/syscall.o

build/elf.o: src/kernel/arch/elf.c | build
	$(CC) $(CFLAGS) -c src/kernel/arch/elf.c -o build/elf.o

# -- kernel core ------------------------------------------------------
build/kernel.o: src/kernel/kernel.c | build
	$(CC) $(CFLAGS) -c src/kernel/kernel.c -o build/kernel.o

build/idt.o: src/kernel/idt.c | build
	$(CC) $(CFLAGS) -c src/kernel/idt.c -o build/idt.o

# -- drivers ----------------------------------------------------------
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

# -- network ----------------------------------------------------------
build/rtl8139.o: src/kernel/drivers/network/rtl8139.c | build
	$(CC) $(CFLAGS) -c src/kernel/drivers/network/rtl8139.c -o build/rtl8139.o

build/net.o: src/kernel/drivers/network/net.c | build
	$(CC) $(CFLAGS) -c src/kernel/drivers/network/net.c -o build/net.o

# -- timer ------------------------------------------------------------
build/pit.o: src/kernel/drivers/timer/pit.c | build
	$(CC) $(CFLAGS) -c src/kernel/drivers/timer/pit.c -o build/pit.o

# -- shell ------------------------------------------------------------
build/shell.o: src/kernel/shell/shell.c | build
	$(CC) $(CFLAGS) -c src/kernel/shell/shell.c -o build/shell.o

# -- scheduler --------------------------------------------------------
build/scheduler.o: src/kernel/sched/scheduler.c | build
	$(CC) $(CFLAGS) -c src/kernel/sched/scheduler.c -o build/scheduler.o

build/context_switch.o: src/kernel/sched/context_switch.asm | build
	$(NASM) -f elf64 src/kernel/sched/context_switch.asm -o build/context_switch.o

build/task_bootstrap.o: src/kernel/sched/task_bootstrap.asm | build
	$(NASM) -f elf64 src/kernel/sched/task_bootstrap.asm -o build/task_bootstrap.o

# -- filesystem -------------------------------------------------------
build/partition.o: src/kernel/fs/partition.c | build
	$(CC) $(CFLAGS) -c src/kernel/fs/partition.c -o build/partition.o

build/fat32.o: src/kernel/fs/fat32.c | build
	$(CC) $(CFLAGS) -c src/kernel/fs/fat32.c -o build/fat32.o

# -- memory management ------------------------------------------------
build/pmm.o: src/kernel/mm/pmm.c | build
	$(CC) $(CFLAGS) -c src/kernel/mm/pmm.c -o build/pmm.o

build/heap.o: src/kernel/mm/heap.c | build
	$(CC) $(CFLAGS) -c src/kernel/mm/heap.c -o build/heap.o

# -- lib --------------------------------------------------------------
build/printf.o: src/kernel/lib/printf.c | build
	$(CC) $(CFLAGS) -c src/kernel/lib/printf.c -o build/printf.o

build/debuglog.o: src/kernel/lib/debuglog.c | build
	$(CC) $(CFLAGS) -c src/kernel/lib/debuglog.c -o build/debuglog.o

# -- user programs (elf64) -------------------------------------------
build/hello_user.o: src/user/hello_user.asm | build
	$(NASM) -f elf64 src/user/hello_user.asm -o build/hello_user.o

$(USER_ELF): build/hello_user.o | build
	$(LD) -m elf_x86_64 -nostdlib -e _start -Ttext 0x210000 -o $(USER_ELF) build/hello_user.o

# -- link the final kernel elf ----------------------------------------
OBJS = build/kernel_entry.o \
       build/interrupt_stubs.o \
       build/kernel.o \
       build/idt.o \
       build/gdt.o \
       build/usermode.o \
       build/user_test.o \
       build/syscall.o \
       build/elf.o \
       build/vga.o \
       build/serial.o \
       build/keyboard.o \
       build/pci.o \
       build/ata.o \
       build/rtl8139.o \
       build/net.o \
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

# strip the elf headers to get a flat binary for the bootloader
$(KERNEL): build/kernel_elf
	objcopy -O binary build/kernel_elf $(KERNEL)

# -- disk images ------------------------------------------------------
# the os image has the bootloader at sector 0 and kernel at sector 1
$(OS_IMAGE): $(BOOTLOADER) $(KERNEL)
	dd if=/dev/zero of=$(OS_IMAGE) bs=512 count=131072
	dd if=$(BOOTLOADER) of=$(OS_IMAGE) conv=notrunc
	dd if=$(KERNEL) of=$(OS_IMAGE) seek=1 conv=notrunc
	@echo "os image created: $(OS_IMAGE)"

# the storage image has a fat32 filesystem with demo files
$(STORAGE_IMAGE): $(USER_ELF) | build
	python3 scripts/create_storage_image.py $(STORAGE_IMAGE) $(USER_ELF)

clean:
	rm -rf build/

.PHONY: all clean
