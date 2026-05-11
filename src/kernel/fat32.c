#include "fat32.h"
#include "ata.h"
#include "partition.h"
#include "printf.h"
#include <string.h>

static fat32_fs_t current_fs;
static int fs_mounted = 0;

static void fat32_disk_to_ata(uint8_t disk, uint8_t *channel, uint8_t *drive) {
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

int fat32_mount(uint8_t disk, uint8_t partition) {
    printf("[FAT32] Mounting partition %d on disk %d...\n", partition, disk);
    
    // Get partition info
    partition_t part_info;
    if (partition_get_info(disk, partition, &part_info) != 0) {
        printf("[FAT32] Partition not found\n");
        return -1;
    }
    
    // Verify it's FAT32
    if (part_info.type != PART_TYPE_FAT32 && part_info.type != PART_TYPE_FAT32_LBA) {
        printf("[FAT32] Partition is not FAT32 (type: 0x%02x)\n", part_info.type);
        return -1;
    }
    
    // Read boot sector
    fat32_boot_sector_t boot;
    uint8_t channel, drive;
    fat32_disk_to_ata(disk, &channel, &drive);

    if (ata_read_sectors(channel, drive, part_info.start_lba, 1, &boot) != 1) {
        printf("[FAT32] Failed to read boot sector\n");
        return -1;
    }
    
    // Verify FAT32 signature from the raw sector bytes
    uint8_t *boot_raw = (uint8_t *)&boot;
    uint16_t boot_signature = (uint16_t)boot_raw[510] | ((uint16_t)boot_raw[511] << 8);
    if (boot_signature != 0xAA55) {
        printf("[FAT32] Invalid boot signature: 0x%x\n", boot_signature);
        return -1;
    }
    
    // Initialize file system structure
    current_fs.disk = disk;
    current_fs.partition = partition;
    current_fs.start_lba = part_info.start_lba;
    current_fs.bytes_per_sector = boot.bytes_per_sector;
    current_fs.sectors_per_cluster = boot.sectors_per_cluster;
    current_fs.fat_start = current_fs.start_lba + boot.reserved_sectors;
    current_fs.root_dir_cluster = boot.root_dir_first_cluster;
    
    // Calculate data start
    uint32_t fat_sectors = boot.sectors_per_fat;
    uint32_t fat_area_size = boot.num_fats * fat_sectors;
    current_fs.data_start = current_fs.fat_start + fat_area_size;
    
    // Calculate total clusters
    uint32_t data_sectors = boot.total_sectors_32 - (boot.reserved_sectors + fat_area_size);
    current_fs.total_clusters = data_sectors / boot.sectors_per_cluster;
    
    fs_mounted = 1;
    
    printf("[FAT32] Mount successful!\n");
    printf("[FAT32] Boot sector signature: 0x%x\n", boot_signature);
    printf("[FAT32] File system type: %.8s\n", boot.file_system_type);
    printf("[FAT32] Volume label: %.11s\n", boot.volume_label);
    printf("[FAT32] Bytes per sector: %d\n", boot.bytes_per_sector);
    printf("[FAT32] Sectors per cluster: %d\n", boot.sectors_per_cluster);
    printf("[FAT32] Reserved sectors: %d\n", boot.reserved_sectors);
    printf("[FAT32] Total sectors: %d\n", boot.total_sectors_32);
    printf("[FAT32] Sectors per FAT: %d\n", fat_sectors);
    printf("[FAT32] Root directory cluster: %d\n", boot.root_dir_first_cluster);
    printf("[FAT32] Total clusters: %d\n", current_fs.total_clusters);
    
    return 0;
}

fat32_file_t* fat32_open(const char *path) {
    if (!fs_mounted) {
        printf("[FAT32] File system not mounted\n");
        return NULL;
    }
    
    printf("[FAT32] Opening file: %s\n", path);
    // TODO: Implement file open
    return NULL;
}

int fat32_read(fat32_file_t *file, void *buffer, size_t count) {
    if (!fs_mounted) {
        printf("[FAT32] File system not mounted\n");
        return -1;
    }
    
    printf("[FAT32] Reading %d bytes\n", count);
    // TODO: Implement file read
    return -1;
}

void fat32_close(fat32_file_t *file) {
    if (file) {
        // TODO: Implement file close
    }
}

fat32_dir_t* fat32_opendir(const char *path) {
    if (!fs_mounted) {
        printf("[FAT32] File system not mounted\n");
        return NULL;
    }
    
    printf("[FAT32] Opening directory: %s\n", path);
    // TODO: Implement directory open
    return NULL;
}

int fat32_readdir(fat32_dir_t *dir, fat32_dirent_t *entry) {
    if (!fs_mounted) {
        printf("[FAT32] File system not mounted\n");
        return -1;
    }
    
    // TODO: Implement directory read
    return -1;
}

void fat32_closedir(fat32_dir_t *dir) {
    if (dir) {
        // TODO: Implement directory close
    }
}

int fat32_stat(const char *path, fat32_dirent_t *entry) {
    if (!fs_mounted) {
        printf("[FAT32] File system not mounted\n");
        return -1;
    }
    
    printf("[FAT32] Stat: %s\n", path);
    // TODO: Implement stat
    return -1;
}
