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

#include "rtl8139.h"
#include "../../lib/printf.h"
#include "../../arch/gdt.h"
#include "../../mm/pmm.h"
#include <stdint.h>
#include <stddef.h>

// global rtl8139 device instance
static rtl8139_device_t rtl8139_dev = {0};

// helper to allocate contiguous pages (best-effort)
static void* pmm_alloc_contiguous(size_t num_pages) {
    // allocate first page
    void *first = pmm_alloc_page();
    if (!first) return NULL;
    
    // try to allocate remaining pages (they should be contiguous)
    for (size_t i = 1; i < num_pages; i++) {
        void *page = pmm_alloc_page();
        if (!page) {
            printf("[RTL8139] WARNING: Could not allocate contiguous pages\n");
            return first;  // return what we have
        }
    }
    
    return first;
}

// port i/o functions
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// mmio read/write functions
static inline uint8_t mmio_read8(uintptr_t addr, uint16_t offset) {
    return *(volatile uint8_t *)(addr + offset);
}

static inline void mmio_write8(uintptr_t addr, uint16_t offset, uint8_t val) {
    *(volatile uint8_t *)(addr + offset) = val;
}

static inline uint16_t mmio_read16(uintptr_t addr, uint16_t offset) {
    return *(volatile uint16_t *)(addr + offset);
}

static inline void mmio_write16(uintptr_t addr, uint16_t offset, uint16_t val) {
    *(volatile uint16_t *)(addr + offset) = val;
}

static inline uint32_t mmio_read32(uintptr_t addr, uint16_t offset) {
    return *(volatile uint32_t *)(addr + offset);
}

static inline void mmio_write32(uintptr_t addr, uint16_t offset, uint32_t val) {
    *(volatile uint32_t *)(addr + offset) = val;
}

// abstracted read/write functions that work with either mmio or port i/o
static uint8_t rtl8139_read8(uint16_t offset) {
    if (rtl8139_dev.is_mmio) {
        return mmio_read8(rtl8139_dev.mmio_base, offset);
    } else {
        return inb(rtl8139_dev.io_base + offset);
    }
}

static void rtl8139_write8(uint16_t offset, uint8_t val) {
    if (rtl8139_dev.is_mmio) {
        mmio_write8(rtl8139_dev.mmio_base, offset, val);
    } else {
        outb(rtl8139_dev.io_base + offset, val);
    }
}

static uint16_t rtl8139_read16(uint16_t offset) {
    if (rtl8139_dev.is_mmio) {
        return mmio_read16(rtl8139_dev.mmio_base, offset);
    } else {
        return inw(rtl8139_dev.io_base + offset);
    }
}

static void rtl8139_write16(uint16_t offset, uint16_t val) {
    if (rtl8139_dev.is_mmio) {
        mmio_write16(rtl8139_dev.mmio_base, offset, val);
    } else {
        outw(rtl8139_dev.io_base + offset, val);
    }
}

static uint32_t rtl8139_read32(uint16_t offset) {
    if (rtl8139_dev.is_mmio) {
        return mmio_read32(rtl8139_dev.mmio_base, offset);
    } else {
        return inl(rtl8139_dev.io_base + offset);
    }
}

static void rtl8139_write32(uint16_t offset, uint32_t val) {
    if (rtl8139_dev.is_mmio) {
        mmio_write32(rtl8139_dev.mmio_base, offset, val);
    } else {
        outl(rtl8139_dev.io_base + offset, val);
    }
}

// reads the mac address from the nic registers
static void rtl8139_read_mac_address(void) {
    for (int i = 0; i < 6; i++) {
        rtl8139_dev.mac_address[i] = rtl8139_read8(RTL8139_MAC0 + i);
    }
}

// initializes the rtl8139 device found at the given pci location
void rtl8139_init(uint8_t bus, uint8_t device, uint8_t func, uint16_t vendor_id, uint16_t device_id) {
    printf("\n[RTL8139] Initializing RTL8139 Network Device\n");
    printf("[RTL8139] PCI %d:%d:%d - Vendor: 0x%04x Device: 0x%04x\n", 
           bus, device, func, vendor_id, device_id);
    
    // read bar0 for io/memory base
    // pci config address formula: (bus << 16) | (device << 11) | (func << 8) | offset
    uint32_t bar0_addr = 0xCF8;
    uint32_t bar0_data = 0xCFC;
    
    uint32_t pci_addr = ((uint32_t)bus << 16) | ((uint32_t)device << 11) | ((uint32_t)func << 8) | 0x10 | 0x80000000;
    outl(bar0_addr, pci_addr);
    uint32_t bar0 = inl(bar0_data);
    
    printf("[RTL8139] BAR0: 0x%08x\n", bar0);
    
    // determine if mmio or port i/o
    if (bar0 & 0x01) {
        // port i/o
        rtl8139_dev.io_base = bar0 & 0xFFFC;
        rtl8139_dev.is_mmio = 0;
        printf("[RTL8139] Using Port I/O at 0x%04x\n", rtl8139_dev.io_base);
    } else {
        // mmio
        rtl8139_dev.mmio_base = (uintptr_t)(bar0 & 0xFFFFFFF0);
        rtl8139_dev.is_mmio = 1;
        printf("[RTL8139] Using MMIO at %p\n", (void*)rtl8139_dev.mmio_base);
    }
    
    // read irq line
    pci_addr = ((uint32_t)bus << 16) | ((uint32_t)device << 11) | ((uint32_t)func << 8) | 0x3C | 0x80000000;
    outl(bar0_addr, pci_addr);
    uint32_t irq_info = inl(bar0_data);
    rtl8139_dev.irq_line = irq_info & 0xFF;
    printf("[RTL8139] IRQ Line: %d\n", rtl8139_dev.irq_line);
    
    // read mac address
    rtl8139_read_mac_address();
    printf("[RTL8139] MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
           rtl8139_dev.mac_address[0], rtl8139_dev.mac_address[1],
           rtl8139_dev.mac_address[2], rtl8139_dev.mac_address[3],
           rtl8139_dev.mac_address[4], rtl8139_dev.mac_address[5]);
    
    // allocate rx buffer (8kb = 2 pages)
    rtl8139_dev.rx_buffer = (uint8_t *)pmm_alloc_contiguous(2);
    if (!rtl8139_dev.rx_buffer) {
        printf("[RTL8139] ERROR: Failed to allocate RX buffer\n");
        return;
    }
    // for now, assume physical address = virtual address (identity mapping)
    rtl8139_dev.rx_buffer_pa = (uintptr_t)rtl8139_dev.rx_buffer;
    printf("[RTL8139] RX Buffer allocated at %p\n", (void*)rtl8139_dev.rx_buffer_pa);
    
    // allocate tx buffers (4 descriptors, 2kb each)
    for (int i = 0; i < RTL8139_NUM_TX_DESC; i++) {
        rtl8139_dev.tx_buffers[i] = (uint8_t *)pmm_alloc_page(); // 1 page = 4kb (we'll use 2kb)
        if (!rtl8139_dev.tx_buffers[i]) {
            printf("[RTL8139] ERROR: Failed to allocate TX buffer %d\n", i);
            return;
        }
        rtl8139_dev.tx_buffer_pa[i] = (uintptr_t)rtl8139_dev.tx_buffers[i];
        printf("[RTL8139] TX Buffer %d allocated at %p\n", i, (void*)rtl8139_dev.tx_buffer_pa[i]);
    }
    rtl8139_dev.tx_current_desc = 0;
    
    // power on the device - clear the power management offset
    uint32_t pci_pm_offset = 0x52;  // common power management config offset
    pci_addr = ((uint32_t)bus << 16) | ((uint32_t)device << 11) | ((uint32_t)func << 8) | pci_pm_offset | 0x80000000;
    outl(bar0_addr, pci_addr);
    uint32_t pm_val = inl(bar0_data);
    // clear power down bit if set
    if (pm_val & 0x00000003) {
        pm_val &= ~0x00000003;
        outl(bar0_addr, pci_addr);
        outl(bar0_data, pm_val);
    }
    
    // reset the nic
    printf("[RTL8139] Resetting device...\n");
    rtl8139_write8(RTL8139_CR, RTL8139_CR_RST);
    
    // wait for reset to complete (busy-wait with timeout)
    uint32_t timeout = 1000000;
    while ((rtl8139_read8(RTL8139_CR) & RTL8139_CR_RST) && timeout > 0) {
        timeout--;
    }
    if (timeout == 0) {
        printf("[RTL8139] WARNING: Reset timeout\n");
    }
    printf("[RTL8139] Reset complete\n");
    
    // initialize rx buffer address
    rtl8139_write32(RTL8139_RBSTART, rtl8139_dev.rx_buffer_pa);
    printf("[RTL8139] RX buffer address set to 0x%08x\n", rtl8139_dev.rx_buffer_pa);
    
    // initialize tx buffer addresses
    for (int i = 0; i < RTL8139_NUM_TX_DESC; i++) {
        uint16_t txaddr_offset = RTL8139_TXADDR0 + (i * 4);
        rtl8139_write32(txaddr_offset, rtl8139_dev.tx_buffer_pa[i]);
    }
    
    // set rx configuration - accept physical match packets and broadcast
    uint32_t rcr = rtl8139_read32(RTL8139_RCR);
    rcr |= (1 << 15) | (1 << 8) | (1 << 7);  // accept physical match, broadcast, multicast
    rcr &= ~(1 << 6);  // don't accept all packets
    rtl8139_write32(RTL8139_RCR, rcr);
    
    // set tx configuration
    uint32_t tcr = rtl8139_read32(RTL8139_TCR);
    tcr = (tcr & ~0x1F000) | 0x8000;  // set tx retry limit
    rtl8139_write32(RTL8139_TCR, tcr);
    
    // reset rx/tx read pointers
    rtl8139_dev.rx_read_offset = 0;
    rtl8139_write16(RTL8139_CAPR, 0);
    rtl8139_write16(RTL8139_CBR, 0);
    
    // enable interrupts (mask what we want)
    uint16_t imr = RTL8139_INT_ROK | RTL8139_INT_TOK | RTL8139_INT_RER | RTL8139_INT_TER;
    rtl8139_write16(RTL8139_IMR, imr);
    
    // enable rx and tx
    uint8_t cr = rtl8139_read8(RTL8139_CR);
    cr |= RTL8139_CR_RE | RTL8139_CR_TE;
    rtl8139_write8(RTL8139_CR, cr);
    
    printf("[RTL8139] Initialization complete - RX/TX enabled\n\n");
}

// scans the pci bus for an rtl8139 and initializes it if found
void rtl8139_detect_and_init(void) {
    printf("[RTL8139] Scanning for RTL8139 network devices...\n");
    
    // scan pci bus for rtl8139
    for (uint8_t bus = 0; bus < 4; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            // check function 0
            uint32_t pci_addr = ((uint32_t)bus << 16) | ((uint32_t)device << 11) | (0 << 8) | 0x00 | 0x80000000;
            outl(0xCF8, pci_addr);
            uint32_t vendor_device = inl(0xCFC);
            
            uint16_t vendor_id = vendor_device & 0xFFFF;
            if (vendor_id == 0xFFFF) continue;  // no device
            
            uint16_t device_id = vendor_device >> 16;
            
            // check if this is an rtl8139
            if (vendor_id == RTL8139_VENDOR_ID && device_id == RTL8139_DEVICE_ID) {
                printf("[RTL8139] Found RTL8139 at PCI %d:%d:0\n", bus, device);
                rtl8139_init(bus, device, 0, vendor_id, device_id);
                return;
            }
            
            // check for multi-function device
            uint32_t header_type_reg = ((uint32_t)bus << 16) | ((uint32_t)device << 11) | (0 << 8) | 0x0C | 0x80000000;
            outl(0xCF8, header_type_reg);
            uint32_t header_info = inl(0xCFC);
            uint8_t header_type = (header_info >> 16) & 0xFF;
            
            if (!(header_type & 0x80)) continue;  // not multi-function
            
            // check other functions
            for (uint8_t func = 1; func < 8; func++) {
                uint32_t pci_addr_mf = ((uint32_t)bus << 16) | ((uint32_t)device << 11) | ((uint32_t)func << 8) | 0x00 | 0x80000000;
                outl(0xCF8, pci_addr_mf);
                vendor_device = inl(0xCFC);
                
                vendor_id = vendor_device & 0xFFFF;
                if (vendor_id == 0xFFFF) continue;
                
                device_id = vendor_device >> 16;
                if (vendor_id == RTL8139_VENDOR_ID && device_id == RTL8139_DEVICE_ID) {
                    printf("[RTL8139] Found RTL8139 at PCI %d:%d:%d\n", bus, device, func);
                    rtl8139_init(bus, device, func, vendor_id, device_id);
                    return;
                }
            }
        }
    }
    
    printf("[RTL8139] No RTL8139 device found on PCI bus\n");
}

// sends a packet via the rtl8139 nic
void rtl8139_send_packet(const uint8_t *data, uint16_t length) {
    if (!rtl8139_dev.tx_buffers[0]) {
        printf("[RTL8139] ERROR: Device not initialized\n");
        return;
    }
    
    if (length > RTL8139_TX_BUF_SIZE) {
        printf("[RTL8139] ERROR: Packet too large (%d > %d)\n", length, RTL8139_TX_BUF_SIZE);
        return;
    }
    
    // get current tx descriptor
    uint8_t desc = rtl8139_dev.tx_current_desc;
    uint16_t txstatus_offset = RTL8139_TXSTATUS0 + (desc * 4);
    
    // check if descriptor is available (own bit should be 0 or tok should be set)
    uint32_t txstatus = rtl8139_read32(txstatus_offset);
    if (txstatus & RTL8139_TSD_OWN) {
        printf("[RTL8139] WARNING: TX descriptor %d busy\n", desc);
        return;
    }
    
    // copy packet data to tx buffer
    uint8_t *tx_buf = rtl8139_dev.tx_buffers[desc];
    for (uint16_t i = 0; i < length; i++) {
        tx_buf[i] = data[i];
    }
    
    // set packet length and transmit
    uint16_t txlen_offset = RTL8139_TXSTATUS0 + (desc * 4);
    uint32_t txlen = length & RTL8139_TSD_SIZE;
    
    // write status register with own bit set to indicate nic ownership
    rtl8139_write32(txlen_offset, (RTL8139_TSD_OWN | txlen));
    
    printf("[RTL8139] Sent packet %d bytes on descriptor %d\n", length, desc);
    
    // move to next descriptor
    rtl8139_dev.tx_current_desc = (rtl8139_dev.tx_current_desc + 1) % RTL8139_NUM_TX_DESC;
}

// handles an interrupt from the rtl8139
void rtl8139_handle_interrupt(void) {
    // read interrupt status
    uint16_t isr = rtl8139_read16(RTL8139_ISR);
    
    if (isr & RTL8139_INT_ROK) {
        printf("[RTL8139] RX packet received\n");
        // TODO: process received packets
    }
    
    if (isr & RTL8139_INT_TOK) {
        printf("[RTL8139] TX complete\n");
    }
    
    if (isr & RTL8139_INT_RER) {
        printf("[RTL8139] RX error\n");
    }
    
    if (isr & RTL8139_INT_TER) {
        printf("[RTL8139] TX error\n");
    }
    
    // clear interrupts
    rtl8139_write16(RTL8139_ISR, isr);
}

// returns a pointer to the global rtl8139 device instance
rtl8139_device_t* rtl8139_get_device(void) {
    return &rtl8139_dev;
}
