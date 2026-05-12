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

void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

void kernel_main(void) {
    // ── Core init ────────────────────────────────────────────────────────────
    debug_log_init();
    vga_init();
    serial_init();
    keyboard_init();

    printf("SDKXOS 64-bit Native Core Booted Successfully!\n");

    printf("Initializing Interrupts... ");
    idt_init();
    printf("OK\n");

    printf("Initializing GDT + TSS... ");
    gdt_init();
    printf("OK\n");

    printf("Initializing Syscalls... ");
    syscall_init();
    printf("OK\n");

    printf("Initializing Memory... ");
    pmm_init();

    // ── Hardware discovery ───────────────────────────────────────────────────
    pci_init();

    // ── Storage ──────────────────────────────────────────────────────────────
    printf("Initializing ATA Storage... ");
    ata_init();
    printf("OK\n");

    printf("Detecting partitions... ");
    if (partition_detect(1) >= 0) {
        printf("OK\n");
        printf("Mounting FAT32... ");
        if (fat32_mount(1, 0) == 0) {
            printf("OK\n");
        } else {
            printf("FAILED\n");
        }
    } else {
        printf("No partitions found\n");
    }

    // ── Scheduler + Shell ────────────────────────────────────────────────────
    scheduler_init();
    scheduler_create_task("shell", shell_run);
    scheduler_start();

    // Never reached
    while (1) { __asm__ volatile("hlt"); }
}
