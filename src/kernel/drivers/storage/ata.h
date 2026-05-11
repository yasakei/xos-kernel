#ifndef ATA_H
#define ATA_H

#include <stdint.h>
#include <stddef.h>

// IDE Channel definitions
#define ATA_PRIMARY     0
#define ATA_SECONDARY   1

// IDE Drive definitions
#define ATA_MASTER      0
#define ATA_SLAVE       1

// ATA Commands
#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_IDENTIFY_DRIVE  0xEC

// ATA Status flags
#define ATA_SR_BSY 0x80    // Busy
#define ATA_SR_DRDY 0x40   // Device Ready
#define ATA_SR_DF 0x20     // Device Fault
#define ATA_SR_DSC 0x10    // Device Seek Complete
#define ATA_SR_DRQ 0x08    // Data Request Ready
#define ATA_SR_CORR 0x04   // Corrected Data
#define ATA_SR_IDX 0x02    // Index
#define ATA_SR_ERR 0x01    // Error

// ATA error flags
#define ATA_ER_BBK  0x80   // Bad Block
#define ATA_ER_UNC  0x40   // Uncorrectable Data
#define ATA_ER_MC   0x20   // Media Changed
#define ATA_ER_IDNF 0x10   // ID Not Found
#define ATA_ER_MCR  0x08   // Media Change Request
#define ATA_ER_ABRT 0x04   // Abort
#define ATA_ER_TK0NF 0x02  // Track 0 Not Found
#define ATA_ER_AMNF 0x01   // Address Mark Not Found

// ATA device structure
typedef struct {
    uint8_t channel;        // ATA_PRIMARY or ATA_SECONDARY
    uint8_t drive;          // ATA_MASTER or ATA_SLAVE
    uint8_t type;           // Device type
    uint16_t signature;     // Device signature
    uint16_t capabilities;  // Capabilities
    uint32_t command_sets;  // Command sets
    uint32_t size;          // Size in sectors
    char model[41];         // Model string
    char serial[21];        // Serial number
} ata_device_t;

// Initialize ATA subsystem
void ata_init(void);

// Detect ATA devices
int ata_detect_device(uint8_t channel, uint8_t drive, ata_device_t *device);

// Read sectors from ATA device
int ata_read_sectors(uint8_t channel, uint8_t drive, uint32_t lba, uint16_t count, void *buffer);

// Write sectors to ATA device
int ata_write_sectors(uint8_t channel, uint8_t drive, uint32_t lba, uint16_t count, const void *buffer);

// Get number of detected devices
int ata_get_device_count(void);

// Get device info
ata_device_t* ata_get_device(int index);

#endif
