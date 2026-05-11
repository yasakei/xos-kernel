#include <stdint.h>
#include "idt.h"
#include "vga.h"
#include "serial.h"
#include "printf.h"
#include "keyboard.h"
#include "pmm.h"
#include "heap.h"
#include "pci.h"
#include "debuglog.h"
#include "ata.h"
#include "partition.h"
#include "fat32.h"

void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
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
    
    printf("Detecting partitions on Disk 0... ");
    if (partition_detect(0) == 0) {
        printf("OK\n");
        printf("Mounting FAT32 filesystem... ");
        if (fat32_mount(0, 0) == 0) {
            printf("OK\n");
        } else {
            printf("Failed\n");
        }
    } else {
        printf("No valid partitions found\n");
    }

    printf("\nTesting malloc()...\n");
    char* dynamic_string = (char*)malloc(128); // Force standard memory split block!
    dynamic_string[0] = 'M';
    dynamic_string[1] = 'A';
    dynamic_string[2] = 'L';
    dynamic_string[3] = 'L';
    dynamic_string[4] = 'O';
    dynamic_string[5] = 'C';
    dynamic_string[6] = ' ';
    dynamic_string[7] = 'S';
    dynamic_string[8] = 'U';
    dynamic_string[9] = 'C';
    dynamic_string[10] = 'C';
    dynamic_string[11] = 'E';
    dynamic_string[12] = 'S';
    dynamic_string[13] = 'S';
    dynamic_string[14] = '!';
    dynamic_string[15] = '\0';
    
    printf("Dynamically allocated heavily protected 128 Bytes: [ %s ]\n", dynamic_string);
    printf("Memory address actively pointing successfully resolving natively strictly at array: %p\n", dynamic_string);
    
    printf("==========================================\n");
    printf("   Keyboard & Serial Input Test Active\n");
    printf("==========================================\n");
    printf("> ");
    
    while(1) {
        // Asynchronous fail-safe: explicitly poll the Serial Line Status Register natively 
        // bypassing the programmable interrupt controller entirely to instantly catch invisible -nographic input!
        if (inb(0x3F8 + 5) & 1) {
            char c = inb(0x3F8);
            printf("%c", c);
        }
        
        // Also poll the physical PS/2 Keyboard natively just in case QEMU is mapping strokes directly!
        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60);
            if (scancode < 128) {
                // If it isn't an extended/modifier key, violently push it to screen!
                printf("<KT:%d>", scancode);
            }
        }

        // We removed `hlt` to allow hyper-aggressive physical polling for debugging
    }
}
