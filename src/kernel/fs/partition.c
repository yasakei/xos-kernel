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

#include "partition.h"
#include "../drivers/storage/ata.h"
#include "../lib/printf.h"
#include "../lib/debuglog.h"
#include "../drivers/display/vga.h"

// simple memcpy implementation for freestanding environment
static void* memcpy(void *dest, const void *src, int n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (int i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

#define MAX_PARTITIONS 16
static partition_t partitions[MAX_PARTITIONS];
static int partition_count = 0;

// convert our disk numbering to ata channel/drive
static void partition_disk_to_ata(uint8_t disk, uint8_t *channel, uint8_t *drive) {
    switch (disk) {
        case 0:
            *channel = ATA_PRIMARY;
            *drive = ATA_MASTER;
            break;
        case 1:
            *channel = ATA_PRIMARY;
            *drive = ATA_SLAVE;
            break;
        case 2:
            *channel = ATA_SECONDARY;
            *drive = ATA_MASTER;
            break;
        default:
            *channel = ATA_SECONDARY;
            *drive = ATA_SLAVE;
            break;
    }
}

// return a human-readable name for a partition type
const char* partition_type_name(uint8_t type) {
    switch (type) {
        case PART_TYPE_UNUSED:    return "Unused";
        case PART_TYPE_FAT12:     return "FAT12";
        case PART_TYPE_FAT16:     return "FAT16";
        case PART_TYPE_EXTENDED:  return "Extended";
        case PART_TYPE_FAT32:     return "FAT32";
        case PART_TYPE_FAT32_LBA: return "FAT32 LBA";
        case PART_TYPE_LINUX:     return "Linux";
        case PART_TYPE_EXTENDED2: return "Extended LBA";
        default:                  return "Unknown";
    }
}

// read the mbr on a disk and detect partitions
int partition_detect(uint8_t disk) {
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[PARTITION] Detecting partitions on disk %d...\n", disk);
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
    
    // read mbr (sector 0)
    mbr_t mbr;
    uint8_t channel, drive;
    partition_disk_to_ata(disk, &channel, &drive);

    if (ata_read_sectors(channel, drive, 0, 1, &mbr) != 1) {
        printf("[PARTITION] Failed to read MBR\n");
        return -1;
    }
    
    // verify boot signature
    if (mbr.signature != 0xAA55) {
        printf("[PARTITION] Invalid MBR signature: 0x%04x\n", mbr.signature);
        return -1;
    }
    
    partition_count = 0;
    
    // parse the four primary partition entries
    for (int i = 0; i < 4; i++) {
        mbr_partition_t *part = &mbr.partitions[i];
        
        // skip empty partitions
        if (part->type == PART_TYPE_UNUSED) continue;
        
        partition_t *pinfo = &partitions[partition_count];
        pinfo->disk = disk;
        pinfo->partition_index = i;
        pinfo->type = part->type;
        pinfo->bootable = (part->boot_flag == 0x80) ? 1 : 0;
        pinfo->start_lba = part->start_lba;
        pinfo->size_sectors = part->size_sectors;
        
        if (debug_print_is_enabled()) {
            uint8_t prev = vga_get_color();
            vga_set_color(14, 0);
            printf("[PARTITION] Found partition %d:\n", i);
            printf("  Type: %s (0x%02x)\n", partition_type_name(part->type), part->type);
            printf("  Bootable: %s\n", pinfo->bootable ? "Yes" : "No");
            printf("  Start LBA: %d\n", pinfo->start_lba);
            printf("  Size: %d sectors (%d MB)\n", pinfo->size_sectors, (pinfo->size_sectors * 512) / 1024 / 1024);
            vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
        }
        
        partition_count++;
        
        if (partition_count >= MAX_PARTITIONS) {
            printf("[PARTITION] Maximum partitions reached\n");
            break;
        }
    }
    
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[PARTITION] Detection complete: %d partitions found\n", partition_count);
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
    return partition_count;
}

// get info about a specific partition by disk and index
int partition_get_info(uint8_t disk, uint8_t partition_index, partition_t *info) {
    if (partition_index >= partition_count) {
        return -1;
    }
    
    // find the matching partition entry
    for (int i = 0; i < partition_count; i++) {
        partition_t *p = &partitions[i];
        if (p->disk == disk && p->partition_index == partition_index) {
            memcpy(info, p, sizeof(partition_t));
            return 0;
        }
    }
    
    return -1;
}

// return how many partitions exist on a given disk
int partition_get_count(uint8_t disk) {
    int count = 0;
    for (int i = 0; i < partition_count; i++) {
        if (partitions[i].disk == disk) {
            count++;
        }
    }
    return count;
}
