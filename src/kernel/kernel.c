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

void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// ── Demo tasks ───────────────────────────────────────────────────────────────

static void task_a(void) {
    // Initialize the PIT timer now that we're in the first task.
    // This avoids a race where PIT ticks fire before scheduler_start runs.
    pit_init(100);

    uint32_t counter = 0;
    while (1) {
        counter++;
        if (counter % 50 == 0) {
            printf("[Task A] tick %d\n", counter);
        }
        scheduler_yield();
    }
}

static void task_b(void) {
    uint32_t counter = 0;
    while (1) {
        counter++;
        if (counter % 50 == 0) {
            printf("[Task B] tick %d\n", counter);
        }
        scheduler_yield();
    }
}

void kernel_main(void) {
    debug_log_init();
    vga_init();
    serial_init();
    keyboard_init();
    
    printf("XOS 64-bit Native Core Booted Successfully!\n");
    
    printf("Initializing Interrupts... ");
    idt_init();
    printf("OK\n");

    // Launch Step #4: Memory Management Module
    pmm_init();

    // Launch Step #6: PCI Peripheral Discovery
    pci_init();

    // Launch Step #7: Storage & File Systems
    printf("\nInitializing ATA Storage... ");
    ata_init();
    printf("OK\n");
    
    printf("Detecting partitions on Disk 1... ");
    if (partition_detect(1) >= 0) {
        printf("OK\n");
        printf("Mounting FAT32 filesystem... ");
        if (fat32_mount(1, 0) == 0) {
            printf("OK\n");
            printf("Listing FAT32 root directory...\n");
            fat32_list_root();

            // Read HELLO.TXT
            printf("\nReading HELLO.TXT...\n");
            fat32_file_t *f = fat32_open("/HELLO.TXT");
            if (f) {
                uint8_t buf[64];
                int n = fat32_read(f, buf, sizeof(buf) - 1);
                if (n > 0) {
                    buf[n] = '\0';
                    printf("[FAT32] Contents (%d bytes): %s", n, (char*)buf);
                }
                fat32_close(f);
            }

            // Read README.TXT
            printf("\nReading README.TXT...\n");
            fat32_file_t *r = fat32_open("/README.TXT");
            if (r) {
                uint8_t buf[64];
                int n = fat32_read(r, buf, sizeof(buf) - 1);
                if (n > 0) {
                    buf[n] = '\0';
                    printf("[FAT32] Contents (%d bytes): %s", n, (char*)buf);
                }
                fat32_close(r);
            }
        } else {
            printf("Failed\n");
        }
    } else {
        printf("No valid partitions found\n");
    }

    printf("\nTesting malloc()...\n");
    char* dynamic_string = (char*)malloc(128);
    dynamic_string[0] = 'M'; dynamic_string[1] = 'A'; dynamic_string[2] = 'L';
    dynamic_string[3] = 'L'; dynamic_string[4] = 'O'; dynamic_string[5] = 'C';
    dynamic_string[6] = ' '; dynamic_string[7] = 'S'; dynamic_string[8] = 'U';
    dynamic_string[9] = 'C'; dynamic_string[10] = 'C'; dynamic_string[11] = 'E';
    dynamic_string[12] = 'S'; dynamic_string[13] = 'S'; dynamic_string[14] = '!';
    dynamic_string[15] = '\0';
    printf("malloc(128) OK: [ %s ] @ %p\n", dynamic_string, dynamic_string);

    // ── Step 8: Multitasking ─────────────────────────────────────────────────
    printf("\n");
    printf("================================================\n");
    printf(" Step 8: Multitasking\n");
    printf("================================================\n");

    // Initialize scheduler
    scheduler_init();

    // Create demo tasks
    scheduler_create_task("task_a", task_a);
    scheduler_create_task("task_b", task_b);

    // Dump task list before starting
    scheduler_dump();

    printf("[KERNEL] Handing off to scheduler...\n");

    // Jump into first task — never returns
    scheduler_start();

    // Should never reach here
    printf("[KERNEL] ERROR: scheduler_start returned!\n");
    while (1) { __asm__ volatile("hlt"); }
}
