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

#include "pci.h"
#include "../../lib/printf.h"

// basic 32-bit port i/o wrappers for pci config space access
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ( "outl %0, %1" : : "a"(val), "Nd"(port) );
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ( "inl %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// reads a 32-bit value from the pci config space at the given bus/device/func/offset
static uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (device << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(0xCF8, address);
    return inl(0xCFC);
}

// translates pci class codes into human-readable device type names
static const char* pci_class_name(uint8_t class_code) {
    switch(class_code) {
        case 0x00: return "Non-classified Device";
        case 0x01: return "Mass Storage Controller";
        case 0x02: return "Network Interface";
        case 0x03: return "Display Controller";
        case 0x04: return "Multimedia Device";
        case 0x05: return "Memory Controller";
        case 0x06: return "Bridge Device";
        case 0x07: return "Simple Communication Controller";
        case 0x08: return "Base System Peripheral";
        case 0x09: return "Input Device";
        case 0x0A: return "Docking Station";
        case 0x0B: return "Processor";
        case 0x0C: return "Serial Bus Controller";
        case 0x0D: return "Wireless Controller";
        case 0x0E: return "Intelligent I/O Controller";
        case 0x0F: return "Satellite Communication Controller";
        case 0x10: return "Encryption/Decryption Controller";
        case 0x11: return "Data Acquisition and Signal Processing";
        default: return "Unknown Device";
    }
}

// translates pci subclass codes into human-readable names (only for known class types)
static const char* pci_subclass_name(uint8_t class_code, uint8_t subclass) {
    if(class_code == 0x01) { // mass storage
        switch(subclass) {
            case 0x00: return "SCSI";
            case 0x01: return "IDE";
            case 0x02: return "Floppy";
            case 0x03: return "IPI";
            case 0x04: return "RAID";
            case 0x05: return "ATA";
            case 0x06: return "Serial ATA";
            case 0x07: return "Serial Attached SCSI";
            default: return "Storage Device";
        }
    }
    if(class_code == 0x02) { // network
        switch(subclass) {
            case 0x00: return "Ethernet";
            case 0x01: return "Token Ring";
            case 0x02: return "FDDI";
            case 0x03: return "ATM";
            case 0x04: return "ISDN";
            default: return "Network Device";
        }
    }
    if(class_code == 0x03) { // display
        switch(subclass) {
            case 0x00: return "VGA";
            case 0x01: return "XGA";
            case 0x02: return "3D";
            default: return "Display Device";
        }
    }
    return "Unknown Subclass";
}

// scans the pci bus and prints info about every device found
void pci_init(void) {
    printf("\n");
    printf("================================================\n");
    printf(" PCI Bus Enumeration - Hardware Inventory Scan\n");
    printf("================================================\n");
    int devices_found = 0;
    
    // scan up to 4 buses - qemu usually puts everything on bus 0
    for (uint16_t bus = 0; bus < 4; bus++) {
        for (uint16_t device = 0; device < 32; device++) {
            
            // check function 0 first to see if a device is present here
            uint32_t vendor_device = pci_read_config(bus, device, 0, 0x00);
            uint16_t vendor_id = vendor_device & 0xFFFF;
            
            // if vendor id is 0xffff, no device exists here
            if (vendor_id == 0xFFFF) continue;
            
            // check the multi-function bit in the header type register
            uint32_t header_type_reg = pci_read_config(bus, device, 0, 0x0C);
            uint8_t header_type = (header_type_reg >> 16) & 0xFF;
            int functions_to_scan = (header_type & 0x80) ? 8 : 1;
            
            for (uint16_t func = 0; func < functions_to_scan; func++) {
                uint32_t vd = pci_read_config(bus, device, func, 0x00);
                uint16_t v_id = vd & 0xFFFF;
                if (v_id == 0xFFFF) continue;
                
                uint16_t d_id = vd >> 16;
                uint32_t class_info = pci_read_config(bus, device, func, 0x08);
                uint8_t class_code = (class_info >> 24) & 0xFF;
                uint8_t subclass = (class_info >> 16) & 0xFF;
                
                printf("\n[PCI %d:%d:%d] Vendor: 0x%04x Device: 0x%04x\n", 
                       bus, device, func, v_id, d_id);
                printf("  Class: %s\n", pci_class_name(class_code));
                printf("  Subclass: %s\n", pci_subclass_name(class_code, subclass));
                
                // get bar0 and irq info
                uint32_t bar0 = pci_read_config(bus, device, func, 0x10);
                printf("  BAR0: 0x%08x\n", bar0);
                uint32_t irq_pin = pci_read_config(bus, device, func, 0x3C);
                uint8_t irq_line = irq_pin & 0xFF;
                uint8_t irq_pin_num = (irq_pin >> 8) & 0xFF;
                if (irq_line != 0xFF) {
                    printf("  IRQ Line: %d", irq_line);
                    if (irq_pin_num) printf(" (PIN:%c)\n", 'A' + irq_pin_num - 1);
                    else printf("\n");
                }
                
                devices_found++;
            }
        }
    }
    
    printf("\n================================================\n");
    printf("PCI Scan Complete: %d devices enumerated\n", devices_found);
    printf("================================================\n\n");
}
