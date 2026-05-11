#ifndef PARTITION_H
#define PARTITION_H

#include <stdint.h>

// Partition types
#define PART_TYPE_UNUSED    0x00
#define PART_TYPE_FAT12     0x01
#define PART_TYPE_FAT16     0x04
#define PART_TYPE_EXTENDED  0x05
#define PART_TYPE_FAT32     0x0B
#define PART_TYPE_FAT32_LBA 0x0C
#define PART_TYPE_LINUX     0x83
#define PART_TYPE_EXTENDED2 0x0F

// MBR Partition Entry (16 bytes)
typedef struct {
    uint8_t  boot_flag;         // 0x80 = bootable
    uint8_t  start_head;        // CHS start head
    uint8_t  start_sector;      // CHS start sector
    uint8_t  start_cylinder;    // CHS start cylinder
    uint8_t  type;              // Partition type
    uint8_t  end_head;          // CHS end head
    uint8_t  end_sector;        // CHS end sector
    uint8_t  end_cylinder;      // CHS end cylinder
    uint32_t start_lba;         // LBA start sector
    uint32_t size_sectors;      // Number of sectors
} __attribute__((packed)) mbr_partition_t;

// MBR structure (Master Boot Record)
typedef struct {
    uint8_t  boot_code[446];        // Boot code
    mbr_partition_t partitions[4];  // 4 partition entries
    uint16_t signature;             // 0xAA55
} __attribute__((packed)) mbr_t;

// Partition info structure
typedef struct {
    uint32_t start_lba;         // Starting LBA
    uint32_t size_sectors;      // Size in sectors
    uint8_t  type;              // Partition type
    uint8_t  bootable;          // Is bootable
    uint8_t  disk;              // Disk number
    uint8_t  partition_index;   // Partition index (0-3)
} partition_t;

// Parse MBR and detect partitions
int partition_detect(uint8_t disk);

// Get partition information
int partition_get_info(uint8_t disk, uint8_t partition_index, partition_t *info);

// Get partition count
int partition_get_count(uint8_t disk);

// Helper function to get partition type name
const char* partition_type_name(uint8_t type);

#endif
