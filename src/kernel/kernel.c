// -------------------------------------------------------------------
// mit license
// 
// copyright (c) 2026 xos
// 
// permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "software"), to deal in the software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the software, and to permit persons to whom the
// software is furnished to do so, subject to the following
// conditions:
// 
// the above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the software.
// 
// the software is provided "as is", without warranty of any kind,
// express or implied, including but not limited to the warranties
// of merchantability, fitness for a particular purpose and
// noninfringement. in no event shall the authors or copyright
// holders be liable for any claim, damages or other liability,
// whether in an action of contract, tort or otherwise, arising
// from, out of or in connection with the software or the use or
// other dealings in the software.
// -------------------------------------------------------------------

#include <stdint.h>
#include "idt.h"
#include "drivers/display/vga.h"
#include "drivers/serial/serial.h"
#include "lib/printf.h"
#include "drivers/input/keyboard.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "drivers/bus/pci.h"
#include "lib/debuglog.h"
#include "drivers/storage/ata.h"
#include "fs/partition.h"
#include "fs/fat32.h"
#include "drivers/timer/pit.h"
#include "sched/scheduler.h"
#include "shell/shell.h"
#include "arch/gdt.h"
#include "arch/syscall.h"
#include "drivers/network/rtl8139.h"
#include "drivers/network/net.h"

void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

void kernel_main(void) {
    // core init ---------------------------------------------------------------
    // always start debug log first so we can capture early boot messages
    debug_log_init();
    // vga lets us print to screen, serial gives us remote logging
    vga_init();
    serial_init();

    printf("XOS 64-bit Native Core Booted Successfully!\n");

    printf("Initializing Interrupts... ");
    // bring up the idt so we can handle exceptions and irqs
    idt_init();
    printf("OK\n");

    printf("Initializing Keyboard... ");
    keyboard_init();
    printf("OK\n");

    printf("Initializing GDT + TSS... ");
    // set up segmentation and the tss for user-mode transitions
    gdt_init();
    printf("OK\n");

    printf("Initializing Syscalls... ");
    syscall_init();
    printf("OK\n");

    printf("Initializing Memory... ");
    // physical memory manager — we need to know what ram is available
    pmm_init();

    // hardware discovery ------------------------------------------------------
    // scan the pci bus to find devices like network cards and storage
    pci_init();

    printf("\nInitializing Network Devices...\n");
    rtl8139_detect_and_init();
    net_init();

    // storage -----------------------------------------------------------------
    printf("Initializing ATA Storage... ");
    ata_init();
    printf("OK\n");

    printf("Detecting partitions... ");
    // check drive 1 for a partition table
    if (partition_detect(1) >= 0) {
        printf("OK\n");
        printf("Mounting FAT32... ");
        // mount the first partition on drive 1
        if (fat32_mount(1, 0) == 0) {
            printf("OK\n");
        } else {
            printf("FAILED\n");
        }
    } else {
        printf("No partitions found\n");
    }

    // scheduler + shell -------------------------------------------------------
    scheduler_init();
    scheduler_create_task("shell", shell_run);
    scheduler_start();

    // never reached — just halt the cpu if the scheduler stops
    while (1) { __asm__ volatile("hlt"); }
}
