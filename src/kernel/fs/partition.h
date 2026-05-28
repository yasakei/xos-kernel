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

#ifndef PARTITION_H
#define PARTITION_H

#include <stdint.h>

// partition types
#define PART_TYPE_UNUSED    0x00
#define PART_TYPE_FAT12     0x01
#define PART_TYPE_FAT16     0x04
#define PART_TYPE_EXTENDED  0x05
#define PART_TYPE_FAT32     0x0b
#define PART_TYPE_FAT32_LBA 0x0c
#define PART_TYPE_LINUX     0x83
#define PART_TYPE_EXTENDED2 0x0f

// mbr partition entry (16 bytes)
typedef struct {
    uint8_t  boot_flag;         // 0x80 = bootable
    uint8_t  start_head;        // chs start head
    uint8_t  start_sector;      // chs start sector
    uint8_t  start_cylinder;    // chs start cylinder
    uint8_t  type;              // partition type
    uint8_t  end_head;          // chs end head
    uint8_t  end_sector;        // chs end sector
    uint8_t  end_cylinder;      // chs end cylinder
    uint32_t start_lba;         // lba start sector
    uint32_t size_sectors;      // number of sectors
} __attribute__((packed)) mbr_partition_t;

// mbr structure (master boot record)
typedef struct {
    uint8_t  boot_code[446];        // boot code
    mbr_partition_t partitions[4];  // 4 partition entries
    uint16_t signature;             // 0xaa55
} __attribute__((packed)) mbr_t;

// partition info structure
typedef struct {
    uint32_t start_lba;         // starting lba
    uint32_t size_sectors;      // size in sectors
    uint8_t  type;              // partition type
    uint8_t  bootable;          // is bootable
    uint8_t  disk;              // disk number
    uint8_t  partition_index;   // partition index (0-3)
} partition_t;

// parse mbr and detect partitions
int partition_detect(uint8_t disk);

// get partition information
int partition_get_info(uint8_t disk, uint8_t partition_index, partition_t *info);

// get partition count
int partition_get_count(uint8_t disk);

// helper function to get partition type name
const char* partition_type_name(uint8_t type);

#endif
