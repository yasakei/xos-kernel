#include "ata.h"
#include "printf.h"

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

// Wait for device to become ready
static int ata_wait_ready(uint16_t base, int timeout_ms) {
    int i = timeout_ms * 5;  // Reduced iterations for faster timeout
    while (i--) {
        uint8_t status = ata_read(base, 7);  // Read status
        if (!(status & ATA_SR_BSY)) {
            if (status & ATA_SR_ERR) return -1;
            return 0;
        }
    }
    return -1;  // Timeout
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
    uint8_t *buf = (uint8_t*)buffer;
    
    // Quick check: if device doesn't exist, fail immediately
    if (!ata_device_exists(base)) {
        printf("[ATA] No device detected on channel %d\n", channel);
        return -1;
    }
    
    // Select drive
    ata_write(base, 6, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));
    
    // Set sector count
    ata_write(base, 2, count & 0xFF);
    
    // Set LBA
    ata_write(base, 3, lba & 0xFF);
    ata_write(base, 4, (lba >> 8) & 0xFF);
    ata_write(base, 5, (lba >> 16) & 0xFF);
    
    // Issue READ command
    ata_write(base, 7, ATA_CMD_READ_PIO);
    
    // Read sectors
    for (int sector = 0; sector < count; sector++) {
        // Wait for data ready
        if (ata_wait_ready(base, 100) != 0) {
            printf("[ATA] Read timeout at sector %d\n", sector);
            return -1;
        }
        
        // Check status
        uint8_t status = ata_read(base, 7);
        if (status & ATA_SR_ERR) {
            printf("[ATA] Read error at sector %d\n", sector);
            return -1;
        }
        
        if (!(status & ATA_SR_DRQ)) {
            printf("[ATA] No data ready at sector %d\n", sector);
            return -1;
        }
        
        // Read 256 words (512 bytes)
        for (int i = 0; i < 256; i++) {
            uint16_t word = ata_read_word(base, 0);
            buf[0] = word & 0xFF;
            buf[1] = (word >> 8) & 0xFF;
            buf += 2;
        }
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
