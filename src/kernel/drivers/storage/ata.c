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

#include "ata.h"
#include "../../lib/printf.h"
#include "../../lib/debuglog.h"
#include "../../drivers/display/vga.h"

#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376

// returns the i/o base address for the given channel
static uint16_t ata_get_io_base(uint8_t channel) {
    return (channel == ATA_PRIMARY) ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;
}

// returns the control base address for the given channel
static uint16_t ata_get_ctrl_base(uint8_t channel) {
    return (channel == ATA_PRIMARY) ? ATA_PRIMARY_CTRL : ATA_SECONDARY_CTRL;
}

// reads a byte from an ata port
static uint8_t ata_read(uint16_t base, uint8_t offset) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"((uint16_t)(base + offset)));
    return ret;
}

// writes a byte to an ata port
static void ata_write(uint16_t base, uint8_t offset, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"((uint16_t)(base + offset)));
}

// reads a 16-bit value from an ata port
static uint16_t ata_read_word(uint16_t base, uint8_t offset) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"((uint16_t)(base + offset)));
    return ret;
}

// writes a 16-bit value to an ata port
static void ata_write_word(uint16_t base, uint8_t offset, uint16_t value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"((uint16_t)(base + offset)));
}

// performs a 400ns delay by reading the alternate status register 4 times
static void ata_delay400ns(uint16_t ctrl) {
    ata_read(ctrl, 0);
    ata_read(ctrl, 0);
    ata_read(ctrl, 0);
    ata_read(ctrl, 0);
}

// waits until the bsy flag clears (optionally checks for err/df)
static int ata_wait_bsy_clear(uint16_t base, int timeout_iters) {
    while (timeout_iters--) {
        uint8_t status = ata_read(base, 7);
        if (!(status & ATA_SR_BSY)) {
            if (status & (ATA_SR_ERR | ATA_SR_DF)) return -1;
            return 0;
        }
    }
    return -1;  // timeout
}

// waits until bsy clears and drq is set (data ready to transfer)
static int ata_wait_drq(uint16_t base, int timeout_iters) {
    while (timeout_iters--) {
        uint8_t status = ata_read(base, 7);
        if (status & (ATA_SR_ERR | ATA_SR_DF)) return -1;
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) return 0;
    }
    return -1;  // timeout
}

// waits for device to become ready (bsy clear, no error) - kept for compatibility
static int ata_wait_ready(uint16_t base, int timeout_ms) {
    return ata_wait_bsy_clear(base, timeout_ms * 1000);
}

// quick check if a device exists at the given io base
static int ata_device_exists(uint16_t base) {
    // try reading status register - if we get consistent values, device might exist
    uint8_t status1 = ata_read(base, 7);
    uint8_t status2 = ata_read(base, 7);
    
    // both reads should give a valid value if device exists
    // if all bits are 1 (0xff), likely no device
    if (status1 == 0xFF && status2 == 0xFF) {
        return 0;
    }
    
    return 1;
}

// initializes the ata subsystem
void ata_init(void) {
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("\n[ATA] Initializing IDE/ATA subsystem...\n");
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
}

// detects an ata device on the given channel and drive, filling in the device struct
int ata_detect_device(uint8_t channel, uint8_t drive, ata_device_t *device) {
    uint16_t base = ata_get_io_base(channel);
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[ATA] Detecting %s drive %d...\n", 
               channel == ATA_PRIMARY ? "Primary" : "Secondary",
               drive == ATA_MASTER ? 0 : 1);
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
    
    // select drive (bit 4 = slave, bit 0-3 = 0)
    ata_write(base, 6, 0xA0 | (drive << 4));
    
    // issue identify command
    ata_write(base, 7, ATA_CMD_IDENTIFY_DRIVE);
    
    // wait for device to respond
    if (ata_wait_ready(base, 100) != 0) {
        printf("[ATA] Device not found\n");
        return -1;
    }
    
    // check for data ready
    uint8_t status = ata_read(base, 7);
    if (!(status & ATA_SR_DRQ)) {
        printf("[ATA] Device did not respond to IDENTIFY\n");
        return -1;
    }
    
    // read identify data (256 words = 512 bytes)
    uint8_t identify_data[512];
    for (int i = 0; i < 256; i++) {
        uint16_t word = ata_read_word(base, 0);
        identify_data[i*2] = word & 0xFF;
        identify_data[i*2+1] = (word >> 8) & 0xFF;
    }
    
    // fill device structure
    device->channel = channel;
    device->drive = drive;
    device->type = identify_data[0] & 0xFF;
    device->signature = *(uint16_t*)&identify_data[0];
    device->capabilities = *(uint16_t*)&identify_data[98];
    device->command_sets = *(uint32_t*)&identify_data[164];
    device->size = *(uint32_t*)&identify_data[120];  // lba capacity
    
    // extract model and serial (byte-swapped)
    for (int i = 0; i < 40; i += 2) {
        device->model[i] = identify_data[54 + i + 1];
        device->model[i+1] = identify_data[54 + i];
    }
    device->model[40] = '\0';
    
    for (int i = 0; i < 20; i += 2) {
        device->serial[i] = identify_data[20 + i + 1];
        device->serial[i+1] = identify_data[20 + i];
    }
    device->serial[20] = '\0';
    
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[ATA] Device found: %s\n", device->model);
        printf("[ATA] Size: %d sectors (%d MB)\n", device->size, (device->size * 512) / 1024 / 1024);
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
    
    return 0;
}

// reads count sectors from an ata device starting at the given lba
int ata_read_sectors(uint8_t channel, uint8_t drive, uint32_t lba, uint16_t count, void *buffer) {
    uint16_t base = ata_get_io_base(channel);
    uint16_t ctrl = ata_get_ctrl_base(channel);
    uint8_t *buf = (uint8_t*)buffer;
    
    // quick check: if device doesn't exist, fail immediately
    if (!ata_device_exists(base)) {
        printf("[ATA] No device detected on channel %d\n", channel);
        return -1;
    }

    // wait for controller to be idle before selecting drive
    if (ata_wait_bsy_clear(base, 100000) != 0) {
        printf("[ATA] Controller busy before drive select\n");
        return -1;
    }

    // select drive with lba28 mode (bits 7,6,5 = 1,1,1; bit 4 = drive; bits 3:0 = lba[27:24])
    ata_write(base, 6, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));

    // 400ns delay after drive select (read alt-status 4 times)
    ata_delay400ns(ctrl);

    // wait for bsy to clear after drive select
    if (ata_wait_bsy_clear(base, 100000) != 0) {
        printf("[ATA] Drive select timeout\n");
        return -1;
    }

    // set sector count and lba address
    ata_write(base, 2, count & 0xFF);
    ata_write(base, 3,  lba        & 0xFF);
    ata_write(base, 4, (lba >>  8) & 0xFF);
    ata_write(base, 5, (lba >> 16) & 0xFF);
    
    // issue read pio command
    ata_write(base, 7, ATA_CMD_READ_PIO);

    // read each sector
    for (int sector = 0; sector < count; sector++) {
        // 400ns delay then wait for bsy+drq
        ata_delay400ns(ctrl);

        if (ata_wait_drq(base, 500000) != 0) {
            printf("[ATA] Read timeout at sector %d (LBA %d)\n", sector, lba + sector);
            return -1;
        }
        
        // read 256 words (512 bytes) from data port
        for (int i = 0; i < 256; i++) {
            uint16_t word = ata_read_word(base, 0);
            buf[0] = word & 0xFF;
            buf[1] = (word >> 8) & 0xFF;
            buf += 2;
        }

        // small delay between sectors
        ata_delay400ns(ctrl);
    }
    
    return count;
}

// writes count sectors to an ata device starting at the given lba
int ata_write_sectors(uint8_t channel, uint8_t drive, uint32_t lba, uint16_t count, const void *buffer) {
    uint16_t base = ata_get_io_base(channel);
    uint16_t ctrl = ata_get_ctrl_base(channel);
    const uint8_t *buf = (const uint8_t*)buffer;

    if (!ata_device_exists(base)) {
        printf("[ATA] No device detected on channel %d\n", channel);
        return -1;
    }

    if (ata_wait_bsy_clear(base, 100000) != 0) {
        printf("[ATA] Controller busy before drive select\n");
        return -1;
    }

    ata_write(base, 6, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));
    ata_delay400ns(ctrl);

    if (ata_wait_bsy_clear(base, 100000) != 0) {
        printf("[ATA] Drive select timeout\n");
        return -1;
    }

    ata_write(base, 2, count & 0xFF);
    ata_write(base, 3,  lba        & 0xFF);
    ata_write(base, 4, (lba >>  8) & 0xFF);
    ata_write(base, 5, (lba >> 16) & 0xFF);
    ata_write(base, 7, ATA_CMD_WRITE_PIO);

    for (int sector = 0; sector < count; sector++) {
        if (ata_wait_drq(base, 500000) != 0) {
            printf("[ATA] Write timeout at sector %d (LBA %d)\n", sector, lba + sector);
            return -1;
        }

        for (int i = 0; i < 256; i++) {
            uint16_t word = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
            ata_write_word(base, 0, word);
            buf += 2;
        }

        ata_write(base, 7, 0xE7);  // cache flush
        if (ata_wait_bsy_clear(base, 500000) != 0) {
            printf("[ATA] Flush timeout after sector %d (LBA %d)\n", sector, lba + sector);
            return -1;
        }
    }

    return count;
}

// returns the number of detected ata devices (currently always 0)
int ata_get_device_count(void) {
    return 0;
}

// returns a pointer to the device info for the given index (currently always null)
ata_device_t* ata_get_device(int index) {
    (void)index;
    return NULL;
}
