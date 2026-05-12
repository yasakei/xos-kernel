#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stddef.h>

// FAT32 boot sector structure
typedef struct {
    uint8_t  jump[3];               // Jump instruction
    uint8_t  oem_id[8];             // OEM ID
    uint16_t bytes_per_sector;      // Usually 512
    uint8_t  sectors_per_cluster;   // Cluster size
    uint16_t reserved_sectors;      // Reserved sector count
    uint8_t  num_fats;              // Number of FATs
    uint16_t root_entries;          // Root entries (0 for FAT32)
    uint16_t total_sectors_16;      // Total sectors (0 for FAT32)
    uint8_t  media_descriptor;      // Media type
    uint16_t sectors_per_fat_16;    // Sectors per FAT (0 for FAT32)
    uint16_t sectors_per_track;     // Sectors per track
    uint16_t num_heads;             // Number of heads
    uint32_t hidden_sectors;        // Hidden sectors
    uint32_t total_sectors_32;      // Total sectors (FAT32)
    
    // FAT32 specific
    uint32_t sectors_per_fat;       // Sectors per FAT
    uint16_t flags;                 // Flags
    uint16_t version;               // Version
    uint32_t root_dir_first_cluster; // First cluster of root directory
    uint16_t fsinfo_sector;         // FSInfo sector
    uint16_t boot_backup_sector;    // Boot backup sector
    uint8_t  reserved[12];          // Reserved
    
    uint8_t  drive_number;          // Drive number
    uint8_t  reserved_nt;           // Reserved (NT)
    uint8_t  boot_signature;        // Boot signature
    uint32_t volume_id;             // Volume ID
    uint8_t  volume_label[11];      // Volume label
    uint8_t  file_system_type[8];   // File system type
    uint8_t  reserved2[420];        // Padding to sector signature
    uint16_t sector_signature;      // 0xAA55 boot sector signature
} __attribute__((packed)) fat32_boot_sector_t;

// FAT32 directory entry (32 bytes)
typedef struct {
    uint8_t  name[11];              // File name (8.3 format)
    uint8_t  attributes;            // File attributes        (+11)
    uint8_t  nt_reserved;           // Reserved (NTRes)       (+12)
    uint8_t  creation_time_tenth;   // Creation time (tenths) (+13)
    uint16_t creation_time;         // Creation time          (+14)
    uint16_t creation_date;         // Creation date          (+16)
    uint16_t last_access_date;      // Last access date       (+18)
    uint16_t first_cluster_high;    // First cluster high     (+20)
    uint16_t write_time;            // Write time             (+22)
    uint16_t write_date;            // Write date             (+24)
    uint16_t first_cluster_low;     // First cluster low      (+26)
    uint32_t file_size;             // File size              (+28)
} __attribute__((packed)) fat32_dirent_t;

// File attributes
#define ATTR_READ_ONLY   0x01
#define ATTR_HIDDEN      0x02
#define ATTR_SYSTEM      0x04
#define ATTR_VOLUME_ID   0x08
#define ATTR_DIRECTORY   0x10
#define ATTR_ARCHIVE     0x20
#define ATTR_LFN         0x0F   // Long file name

// FAT32 file handle
typedef struct {
    uint32_t first_cluster;         // First cluster
    uint32_t current_cluster;       // Current cluster
    uint32_t file_position;         // Current position in file
    uint32_t file_size;             // File size
    uint32_t sector_offset;         // Offset within current sector
    uint32_t disk;                  // Disk number
    uint8_t  partition;             // Partition number
    uint8_t  attributes;            // File attributes
} fat32_file_t;

// FAT32 directory handle
typedef struct {
    uint32_t first_cluster;         // First directory cluster
    uint32_t current_cluster;       // Current cluster
    uint32_t entry_index;           // Current entry index
    uint32_t disk;                  // Disk number
    uint8_t  partition;             // Partition number
} fat32_dir_t;

// FAT32 file system handle
typedef struct {
    uint8_t  disk;                  // Disk number
    uint8_t  partition;             // Partition number
    uint32_t start_lba;             // Start LBA
    uint32_t fat_start;             // FAT start sector
    uint32_t data_start;            // Data start sector
    uint32_t root_dir_cluster;      // Root directory cluster
    uint32_t sectors_per_fat;       // Sectors per FAT
    uint8_t  num_fats;              // Number of FAT copies
    uint16_t bytes_per_sector;      // Bytes per sector
    uint8_t  sectors_per_cluster;   // Sectors per cluster
    uint32_t total_clusters;        // Total clusters
} fat32_fs_t;

// Mount FAT32 file system
int fat32_mount(uint8_t disk, uint8_t partition);

// List files in the root directory
int fat32_list_root(void);

// List files in a directory path
int fat32_list_directory(const char *path);

// Open file
fat32_file_t* fat32_open(const char *path);

// Read from file
int fat32_read(fat32_file_t *file, void *buffer, size_t count);

// Write file contents in the root directory (creates or overwrites)
int fat32_write(const char *path, const void *buffer, size_t count);

// Create an empty file in the root directory
int fat32_create(const char *path);

// Delete a file from the root directory
int fat32_delete(const char *path);

// Close file
void fat32_close(fat32_file_t *file);

// Open directory
fat32_dir_t* fat32_opendir(const char *path);

// Read directory entry
int fat32_readdir(fat32_dir_t *dir, fat32_dirent_t *entry);

// Close directory
void fat32_closedir(fat32_dir_t *dir);

// Get file info
int fat32_stat(const char *path, fat32_dirent_t *entry);

#endif
