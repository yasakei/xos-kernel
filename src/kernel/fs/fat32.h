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

#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stddef.h>

// fat32 boot sector structure
typedef struct {
    uint8_t  jump[3];               // jump instruction
    uint8_t  oem_id[8];             // oem id
    uint16_t bytes_per_sector;      // usually 512
    uint8_t  sectors_per_cluster;   // cluster size
    uint16_t reserved_sectors;      // reserved sector count
    uint8_t  num_fats;              // number of fats
    uint16_t root_entries;          // root entries (0 for fat32)
    uint16_t total_sectors_16;      // total sectors (0 for fat32)
    uint8_t  media_descriptor;      // media type
    uint16_t sectors_per_fat_16;    // sectors per fat (0 for fat32)
    uint16_t sectors_per_track;     // sectors per track
    uint16_t num_heads;             // number of heads
    uint32_t hidden_sectors;        // hidden sectors
    uint32_t total_sectors_32;      // total sectors (fat32)

    // fat32 specific
    uint32_t sectors_per_fat;       // sectors per fat
    uint16_t flags;                 // flags
    uint16_t version;               // version
    uint32_t root_dir_first_cluster; // first cluster of root directory
    uint16_t fsinfo_sector;         // fsinfo sector
    uint16_t boot_backup_sector;    // boot backup sector
    uint8_t  reserved[12];          // reserved

    uint8_t  drive_number;          // drive number
    uint8_t  reserved_nt;           // reserved (nt)
    uint8_t  boot_signature;        // boot signature
    uint32_t volume_id;             // volume id
    uint8_t  volume_label[11];      // volume label
    uint8_t  file_system_type[8];   // file system type
    uint8_t  reserved2[420];        // padding to sector signature
    uint16_t sector_signature;      // 0xaa55 boot sector signature
} __attribute__((packed)) fat32_boot_sector_t;

// fat32 directory entry (32 bytes)
typedef struct {
    uint8_t  name[11];              // file name (8.3 format)
    uint8_t  attributes;            // file attributes        (+11)
    uint8_t  nt_reserved;           // reserved (ntres)       (+12)
    uint8_t  creation_time_tenth;   // creation time (tenths) (+13)
    uint16_t creation_time;         // creation time          (+14)
    uint16_t creation_date;         // creation date          (+16)
    uint16_t last_access_date;      // last access date       (+18)
    uint16_t first_cluster_high;    // first cluster high     (+20)
    uint16_t write_time;            // write time             (+22)
    uint16_t write_date;            // write date             (+24)
    uint16_t first_cluster_low;     // first cluster low      (+26)
    uint32_t file_size;             // file size              (+28)
} __attribute__((packed)) fat32_dirent_t;

// file attributes
#define ATTR_READ_ONLY   0x01
#define ATTR_HIDDEN      0x02
#define ATTR_SYSTEM      0x04
#define ATTR_VOLUME_ID   0x08
#define ATTR_DIRECTORY   0x10
#define ATTR_ARCHIVE     0x20
#define ATTR_LFN         0x0f   // long file name

// fat32 file handle
typedef struct {
    uint32_t first_cluster;         // first cluster
    uint32_t current_cluster;       // current cluster
    uint32_t file_position;         // current position in file
    uint32_t file_size;             // file size
    uint32_t sector_offset;         // offset within current sector
    uint32_t disk;                  // disk number
    uint8_t  partition;             // partition number
    uint8_t  attributes;            // file attributes
} fat32_file_t;

// fat32 directory handle
typedef struct {
    uint32_t first_cluster;         // first directory cluster
    uint32_t current_cluster;       // current cluster
    uint32_t entry_index;           // current entry index
    uint32_t disk;                  // disk number
    uint8_t  partition;             // partition number
} fat32_dir_t;

// fat32 file system handle
typedef struct {
    uint8_t  disk;                  // disk number
    uint8_t  partition;             // partition number
    uint32_t start_lba;             // start lba
    uint32_t fat_start;             // fat start sector
    uint32_t data_start;            // data start sector
    uint32_t root_dir_cluster;      // root directory cluster
    uint32_t sectors_per_fat;       // sectors per fat
    uint8_t  num_fats;              // number of fat copies
    uint16_t bytes_per_sector;      // bytes per sector
    uint8_t  sectors_per_cluster;   // sectors per cluster
    uint32_t total_clusters;        // total clusters
    uint32_t current_dir_cluster;   // current working directory cluster
    char     current_path[256];     // current working directory path
} fat32_fs_t;

// mount fat32 file system
int fat32_mount(uint8_t disk, uint8_t partition);

// list files in the root directory
int fat32_list_root(void);

// list files in a directory path
int fat32_list_directory(const char *path);

// open file
fat32_file_t* fat32_open(const char *path);

// read from file
int fat32_read(fat32_file_t *file, void *buffer, size_t count);

// write file contents in the root directory (creates or overwrites)
int fat32_write(const char *path, const void *buffer, size_t count);

// create an empty file in the root directory
int fat32_create(const char *path);

// delete a file from the root directory
int fat32_delete(const char *path);

// close file
void fat32_close(fat32_file_t *file);

// open directory
fat32_dir_t* fat32_opendir(const char *path);

// read directory entry
int fat32_readdir(fat32_dir_t *dir, fat32_dirent_t *entry);

// close directory
void fat32_closedir(fat32_dir_t *dir);

// get file info
int fat32_stat(const char *path, fat32_dirent_t *entry);

// create directory
int fat32_mkdir(const char *path);

// change directory
int fat32_chdir(const char *path);

// get current working directory
const char* fat32_getcwd(void);

#endif
