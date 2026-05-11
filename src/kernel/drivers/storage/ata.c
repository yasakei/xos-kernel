#include "ata.h"
#include "../../lib/printf.h"

#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376

// Get I/O base for channel
static uint16_t ata_get_io_base(uint8_t channel) {
    return (channel == ATA_PRIMARY) ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;
}

// Get control base for channel
static uint16_t ata_get_ctrl_base(uint8_t channel) {
    return (channel == ATA_PRIMARY) ? ATA_PRIMARY_CTRL : ATA_SECONDARY_CTRL;
}

// Read from ATA port
static uint8_t ata_read(uint16_t base, uint8_t offset) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"((uint16_t)(base + offset)));
    return ret;
}

// Write to ATA port
static void ata_write(uint16_t base, uint8_t offset, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"((uint16_t)(base + offset)));
}

// Read 16-bit value from ATA port
static uint16_t ata_read_word(uint16_t base, uint8_t offset) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"((uint16_t)(base + offset)));
    return ret;
}

// Perform a 400ns delay by reading the alternate status register 4 times
static void ata_delay400ns(uint16_t ctrl) {
    ata_read(ctrl, 0);
    ata_read(ctrl, 0);
    ata_read(ctrl, 0);
    ata_read(ctrl, 0);
}

// Wait until BSY clears (and optionally ERR/DF are not set)
static int ata_wait_bsy_clear(uint16_t base, int timeout_iters) {
    while (timeout_iters--) {
        uint8_t status = ata_read(base, 7);
        if (!(status & ATA_SR_BSY)) {
            if (status & (ATA_SR_ERR | ATA_SR_DF)) return -1;
            return 0;
        }
    }
    return -1;  // Timeout
}

// Wait until BSY clears AND DRQ is set (data ready to transfer)
static int ata_wait_drq(uint16_t base, int timeout_iters) {
    while (timeout_iters--) {
        uint8_t status = ata_read(base, 7);
        if (status & (ATA_SR_ERR | ATA_SR_DF)) return -1;
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) return 0;
    }
    return -1;  // Timeout
}

// Wait for device to become ready (BSY clear, no error) — kept for compatibility
static int ata_wait_ready(uint16_t base, int timeout_ms) {
    return ata_wait_bsy_clear(base, timeout_ms * 1000);
}

// Check if device exists (quick check)
static int ata_device_exists(uint16_t base) {
    // Try reading status register - if we get consistent values, device might exist
    uint8_t status1 = ata_read(base, 7);
    uint8_t status2 = ata_read(base, 7);
    
    // Both reads should give a valid value if device exists
    // If all bits are 1 (0xFF), likely no device
    if (status1 == 0xFF && status2 == 0xFF) {
        return 0;
    }
    
    return 1;
}

void ata_init(void) {
    printf("\n[ATA] Initializing IDE/ATA subsystem...\n");
}

int ata_detect_device(uint8_t channel, uint8_t drive, ata_device_t *device) {
    uint16_t base = ata_get_io_base(channel);
    
    printf("[ATA] Detecting %s drive %d...\n", 
           channel == ATA_PRIMARY ? "Primary" : "Secondary",
           drive == ATA_MASTER ? 0 : 1);
    
    // Select drive (bit 4 = slave, bit 0-3 = 0)
    ata_write(base, 6, 0xA0 | (drive << 4));
    
    // Issue IDENTIFY command
    ata_write(base, 7, ATA_CMD_IDENTIFY_DRIVE);
    
    // Wait for device to respond
    if (ata_wait_ready(base, 100) != 0) {
        printf("[ATA] Device not found\n");
        return -1;
    }
    
    // Check for data ready
    uint8_t status = ata_read(base, 7);
    if (!(status & ATA_SR_DRQ)) {
        printf("[ATA] Device did not respond to IDENTIFY\n");
        return -1;
    }
    
    // Read identify data (256 words = 512 bytes)
    uint8_t identify_data[512];
    for (int i = 0; i < 256; i++) {
        uint16_t word = ata_read_word(base, 0);
        identify_data[i*2] = word & 0xFF;
        identify_data[i*2+1] = (word >> 8) & 0xFF;
    }
    
    // Fill device structure
    device->channel = channel;
    device->drive = drive;
    device->type = identify_data[0] & 0xFF;
    device->signature = *(uint16_t*)&identify_data[0];
    device->capabilities = *(uint16_t*)&identify_data[98];
    device->command_sets = *(uint32_t*)&identify_data[164];
    device->size = *(uint32_t*)&identify_data[120];  // LBA capacity
    
    // Extract model and serial (with endianness swap)
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
    
    printf("[ATA] Device found: %s\n", device->model);
    printf("[ATA] Size: %d sectors (%d MB)\n", device->size, (device->size * 512) / 1024 / 1024);
    
    return 0;
}

int ata_read_sectors(uint8_t channel, uint8_t drive, uint32_t lba, uint16_t count, void *buffer) {
    uint16_t base = ata_get_io_base(channel);
    uint16_t ctrl = ata_get_ctrl_base(channel);
    uint8_t *buf = (uint8_t*)buffer;
    
    // Quick check: if device doesn't exist, fail immediately
    if (!ata_device_exists(base)) {
        printf("[ATA] No device detected on channel %d\n", channel);
        return -1;
    }

    // Wait for controller to be idle before selecting drive
    if (ata_wait_bsy_clear(base, 100000) != 0) {
        printf("[ATA] Controller busy before drive select\n");
        return -1;
    }

    // Select drive with LBA28 mode (bits 7,6,5 = 1,1,1; bit 4 = drive; bits 3:0 = LBA[27:24])
    ata_write(base, 6, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));

    // 400ns delay after drive select (read alt-status 4 times)
    ata_delay400ns(ctrl);

    // Wait for BSY to clear after drive select
    if (ata_wait_bsy_clear(base, 100000) != 0) {
        printf("[ATA] Drive select timeout\n");
        return -1;
    }

    // Set sector count and LBA address
    ata_write(base, 2, count & 0xFF);
    ata_write(base, 3,  lba        & 0xFF);
    ata_write(base, 4, (lba >>  8) & 0xFF);
    ata_write(base, 5, (lba >> 16) & 0xFF);
    
    // Issue READ PIO command
    ata_write(base, 7, ATA_CMD_READ_PIO);

    // Read each sector
    for (int sector = 0; sector < count; sector++) {
        // 400ns delay then wait for BSY+DRQ
        ata_delay400ns(ctrl);

        if (ata_wait_drq(base, 500000) != 0) {
            printf("[ATA] Read timeout at sector %d (LBA %d)\n", sector, lba + sector);
            return -1;
        }
        
        // Read 256 words (512 bytes) from data port
        for (int i = 0; i < 256; i++) {
            uint16_t word = ata_read_word(base, 0);
            buf[0] = word & 0xFF;
            buf[1] = (word >> 8) & 0xFF;
            buf += 2;
        }

        // Small delay between sectors
        ata_delay400ns(ctrl);
    }
    
    return count;
}

int ata_write_sectors(uint8_t channel, uint8_t drive, uint32_t lba, uint16_t count, const void *buffer) {
    // TODO: Implement write support
    printf("[ATA] Write not yet implemented\n");
    return -1;
}

int ata_get_device_count(void) {
    // TODO: Return actual device count
    return 0;
}

ata_device_t* ata_get_device(int index) {
    // TODO: Return device from list
    return NULL;
}
