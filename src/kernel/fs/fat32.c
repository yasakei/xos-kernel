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

#include "fat32.h"
#include "../drivers/storage/ata.h"
#include "partition.h"
#include "../lib/printf.h"
#include "../lib/debuglog.h"
#include "../drivers/display/vga.h"

// simple memcpy that doesn't need libc
static void* memcpy(void *dest, const void *src, int n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (int i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

// simple memset that doesn't need libc
static void* memset(void *dest, int value, int n) {
    uint8_t *d = (uint8_t *)dest;
    for (int i = 0; i < n; i++) {
        d[i] = (uint8_t)value;
    }
    return dest;
}

// simple memcmp that doesn't need libc
static int memcmp(const void *a, const void *b, int n) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    for (int i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return (int)pa[i] - (int)pb[i];
    }
    return 0;
}

// global fat32 state — one mounted filesystem at a time
static fat32_fs_t current_fs;
static int fs_mounted = 0;
static uint8_t root_dir_buffer[16384];
static fat32_dir_t root_dir_handle;

// cache one fat sector at a time for cluster chain traversal
static uint8_t fat_sector_cache[512];
static uint32_t fat_cache_sector = 0xFFFFFFFF;  // invalid sentinel value

// one open file slot — single file at a time for now
static fat32_file_t open_file;
static uint8_t file_cluster_buf[4096];  // max cluster size (8 * 512)

// forward declarations for internal helpers
static int fat32_load_directory_cluster(uint32_t cluster, uint8_t *buffer, size_t buffer_size);
static int fat32_find_in_directory(uint32_t dir_cluster, const char *name, fat32_dirent_t *out);
static int fat32_load_root_directory(void);
static int fat32_navigate_path(const char *path, uint32_t *parent_cluster, fat32_dirent_t *entry, char *filename);

// convert our disk number to ata channel/drive pair
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

// convert a cluster number to an absolute lba on disk
static uint32_t fat32_cluster_to_lba(uint32_t cluster) {
    return current_fs.data_start + ((cluster - 2) * current_fs.sectors_per_cluster);
}

// read the fat entry for a given cluster — returns the next cluster in the chain
static uint32_t fat32_next_cluster(uint32_t cluster) {
    uint8_t channel, drive;
    fat32_disk_to_ata(current_fs.disk, &channel, &drive);

    // each fat32 entry is 4 bytes; a 512-byte sector holds 128 entries
    uint32_t fat_offset  = cluster * 4;
    uint32_t fat_sector  = current_fs.fat_start + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;

    // only read from disk if this sector isn't already cached
    if (fat_sector != fat_cache_sector) {
        if (ata_read_sectors(channel, drive, fat_sector, 1, fat_sector_cache) != 1) {
            printf("[FAT32] Failed to read FAT sector %d\n", fat_sector);
            return 0;
        }
        fat_cache_sector = fat_sector;
    }

    uint32_t next = (uint32_t)fat_sector_cache[entry_offset]
                  | ((uint32_t)fat_sector_cache[entry_offset + 1] << 8)
                  | ((uint32_t)fat_sector_cache[entry_offset + 2] << 16)
                  | ((uint32_t)fat_sector_cache[entry_offset + 3] << 24);
    return next & 0x0FFFFFFF;
}

// check if a cluster value means end-of-chain
static int fat32_is_end_of_chain(uint32_t cluster) {
    return cluster >= 0x0FFFFFF8;
}

// check if a cluster value is a valid data cluster
static int fat32_is_valid_cluster(uint32_t cluster) {
    return cluster >= 2 && cluster < 0x0FFFFFF8;
}

// convert a string to uppercase in place
static void fat32_make_upper(char *s) {
    for (int i = 0; s[i]; i++) {
        if (s[i] >= 'a' && s[i] <= 'z') s[i] = (char)(s[i] - 32);
    }
}

// build an 8.3 format filename from a path string
static int fat32_make_name83(const char *path, uint8_t out[11]) {
    for (int i = 0; i < 11; i++) out[i] = ' ';

    const char *name = path;
    if (name[0] == '/') name++;
    if (!name[0]) return -1;

    char base[9];
    char ext[4];
    int bi = 0, ei = 0;
    int saw_dot = 0;

    for (int i = 0; name[i]; i++) {
        char c = name[i];
        if (c == '/') return -1;
        if (c == '.') {
            if (saw_dot) return -1;
            saw_dot = 1;
            continue;
        }

        if (!saw_dot) {
            if (bi >= 8) return -1;
            base[bi++] = c;
        } else {
            if (ei >= 3) return -1;
            ext[ei++] = c;
        }
    }

    base[bi] = '\0';
    ext[ei] = '\0';
    fat32_make_upper(base);
    fat32_make_upper(ext);

    for (int i = 0; i < bi; i++) out[i] = (uint8_t)base[i];
    for (int i = 0; i < ei; i++) out[8 + i] = (uint8_t)ext[i];
    return 0;
}

// write the root directory buffer back to disk
static int fat32_save_root_directory(void) {
    uint8_t channel, drive;
    fat32_disk_to_ata(current_fs.disk, &channel, &drive);
    uint32_t lba = fat32_cluster_to_lba(current_fs.root_dir_cluster);
    uint16_t sectors = current_fs.sectors_per_cluster;
    if ((uint32_t)sectors * 512 > sizeof(root_dir_buffer)) {
        sectors = sizeof(root_dir_buffer) / 512;
    }
    if (ata_write_sectors(channel, drive, lba, sectors, root_dir_buffer) != sectors) {
        printf("[FAT32] Failed to write root directory\n");
        return -1;
    }
    return 0;
}

// write a value into a fat entry (updates all fat copies and cache)
static int fat32_set_fat_entry(uint32_t cluster, uint32_t value) {
    uint8_t channel, drive;
    fat32_disk_to_ata(current_fs.disk, &channel, &drive);
    uint32_t fat_offset = cluster * 4;
    uint32_t sector_index = fat_offset / 512;
    uint32_t entry_offset = fat_offset % 512;
    uint8_t sector_buf[512];

    // update all fat copies (usually 2)
    for (uint8_t fat = 0; fat < current_fs.num_fats; fat++) {
        uint32_t abs_sector = current_fs.fat_start + ((uint32_t)fat * current_fs.sectors_per_fat) + sector_index;
        if (ata_read_sectors(channel, drive, abs_sector, 1, sector_buf) != 1) {
            printf("[FAT32] Failed to read FAT sector %d\n", abs_sector);
            return -1;
        }
        sector_buf[entry_offset + 0] = (uint8_t)(value & 0xFF);
        sector_buf[entry_offset + 1] = (uint8_t)((value >> 8) & 0xFF);
        sector_buf[entry_offset + 2] = (uint8_t)((value >> 16) & 0xFF);
        sector_buf[entry_offset + 3] = (uint8_t)((value >> 24) & 0xFF);
        if (ata_write_sectors(channel, drive, abs_sector, 1, sector_buf) != 1) {
            printf("[FAT32] Failed to write FAT sector %d\n", abs_sector);
            return -1;
        }
    }

    // keep the cache in sync if it was for this sector
    if (fat_cache_sector == current_fs.fat_start + sector_index) {
        fat_sector_cache[entry_offset + 0] = (uint8_t)(value & 0xFF);
        fat_sector_cache[entry_offset + 1] = (uint8_t)((value >> 8) & 0xFF);
        fat_sector_cache[entry_offset + 2] = (uint8_t)((value >> 16) & 0xFF);
        fat_sector_cache[entry_offset + 3] = (uint8_t)((value >> 24) & 0xFF);
    }

    return 0;
}

// scan the fat for a free cluster
static int fat32_find_free_cluster(uint32_t *cluster_out) {
    for (uint32_t cluster = 2; cluster < current_fs.total_clusters + 2; cluster++) {
        if (fat32_next_cluster(cluster) == 0) {
            *cluster_out = cluster;
            return 0;
        }
    }
    return -1;
}

// free an entire cluster chain starting from first_cluster
static int fat32_free_cluster_chain(uint32_t first_cluster) {
    uint32_t cluster = first_cluster;
    while (fat32_is_valid_cluster(cluster)) {
        uint32_t next = fat32_next_cluster(cluster);
        if (fat32_set_fat_entry(cluster, 0x00000000) != 0) {
            return -1;
        }
        if (fat32_is_end_of_chain(next) || next == 0) {
            break;
        }
        cluster = next;
    }
    return 0;
}

// allocate a chain of clusters (count consecutive entries in the fat)
static int fat32_allocate_cluster_chain(uint32_t count, uint32_t *first_cluster_out) {
    uint32_t first = 0;
    uint32_t prev = 0;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t cluster;
        if (fat32_find_free_cluster(&cluster) != 0) {
            return -1;
        }

        if (first == 0) first = cluster;
        if (prev != 0) {
            if (fat32_set_fat_entry(prev, cluster) != 0) return -1;
        }
        prev = cluster;
    }

    if (prev != 0) {
        if (fat32_set_fat_entry(prev, 0x0FFFFFFF) != 0) return -1;
    }

    *first_cluster_out = first;
    return 0;
}

// format an 8.3 directory entry name into a readable string like "file.txt"
static void fat32_format_name(const fat32_dirent_t *entry, char *out, size_t out_size) {
    size_t pos = 0;

    for (int i = 0; i < 8 && entry->name[i] != ' '; i++) {
        if (pos + 1 < out_size) {
            out[pos++] = (char)entry->name[i];
        }
    }

    int has_ext = 0;
    for (int i = 8; i < 11; i++) {
        if (entry->name[i] != ' ') {
            has_ext = 1;
            break;
        }
    }

    if (has_ext && pos + 1 < out_size) {
        out[pos++] = '.';
        for (int i = 8; i < 11 && entry->name[i] != ' '; i++) {
            if (pos + 1 < out_size) {
                out[pos++] = (char)entry->name[i];
            }
        }
    }

    if (pos < out_size) {
        out[pos] = '\0';
    } else if (out_size > 0) {
        out[out_size - 1] = '\0';
    }
}

// load the root directory cluster into our buffer
static int fat32_load_root_directory(void) {
    uint8_t channel, drive;
    fat32_disk_to_ata(current_fs.disk, &channel, &drive);

    uint32_t lba = fat32_cluster_to_lba(current_fs.root_dir_cluster);
    uint16_t sectors = current_fs.sectors_per_cluster;  // read the whole cluster

    if ((uint32_t)sectors * 512 > sizeof(root_dir_buffer)) {
        printf("[FAT32] Root directory cluster too large for buffer (%d sectors)\n", sectors);
        sectors = sizeof(root_dir_buffer) / 512;
    }

    if (ata_read_sectors(channel, drive, lba, sectors, root_dir_buffer) != sectors) {
        printf("[FAT32] Failed to read root directory cluster\n");
        return -1;
    }

    root_dir_handle.first_cluster = current_fs.root_dir_cluster;
    root_dir_handle.current_cluster = current_fs.root_dir_cluster;
    root_dir_handle.entry_index = 0;
    root_dir_handle.disk = current_fs.disk;
    root_dir_handle.partition = current_fs.partition;
    return 0;
}

// mount a fat32 partition on a given disk
int fat32_mount(uint8_t disk, uint8_t partition) {
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[FAT32] Mounting partition %d on disk %d...\n", partition, disk);
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
    
    // look up the partition info
    partition_t part_info;
    if (partition_get_info(disk, partition, &part_info) != 0) {
        printf("[FAT32] Partition not found\n");
        return -1;
    }
    
    // make sure it's actually fat32
    if (part_info.type != PART_TYPE_FAT32 && part_info.type != PART_TYPE_FAT32_LBA) {
        printf("[FAT32] Partition is not FAT32 (type: 0x%02x)\n", part_info.type);
        return -1;
    }
    
    // read the boot sector
    fat32_boot_sector_t boot;
    uint8_t channel, drive;
    fat32_disk_to_ata(disk, &channel, &drive);

    if (ata_read_sectors(channel, drive, part_info.start_lba, 1, &boot) != 1) {
        printf("[FAT32] Failed to read boot sector\n");
        return -1;
    }
    
    // check the boot signature (0xaa55)
    uint8_t *boot_raw = (uint8_t *)&boot;
    uint16_t boot_signature = (uint16_t)boot_raw[510] | ((uint16_t)boot_raw[511] << 8);
    if (boot_signature != 0xAA55) {
        printf("[FAT32] Invalid boot signature: 0x%x\n", boot_signature);
        return -1;
    }
    
    // fill in the filesystem structure from the boot sector
    current_fs.disk = disk;
    current_fs.partition = partition;
    current_fs.start_lba = part_info.start_lba;
    current_fs.bytes_per_sector = boot.bytes_per_sector;
    current_fs.sectors_per_cluster = boot.sectors_per_cluster;
    current_fs.fat_start = current_fs.start_lba + boot.reserved_sectors;
    current_fs.root_dir_cluster = boot.root_dir_first_cluster;
    current_fs.sectors_per_fat = boot.sectors_per_fat;
    current_fs.num_fats = boot.num_fats;
    
    // calculate where the data region starts
    uint32_t fat_sectors = boot.sectors_per_fat;
    uint32_t fat_area_size = boot.num_fats * fat_sectors;
    current_fs.data_start = current_fs.fat_start + fat_area_size;
    
    // figure out how many clusters there are
    uint32_t data_sectors = boot.total_sectors_32 - (boot.reserved_sectors + fat_area_size);
    current_fs.total_clusters = data_sectors / boot.sectors_per_cluster;
    
    // start in the root directory
    current_fs.current_dir_cluster = current_fs.root_dir_cluster;
    current_fs.current_path[0] = '/';
    current_fs.current_path[1] = '\0';
    
    fs_mounted = 1;

    // print some info about the mounted filesystem
    char fs_type[9];
    char volume_label[12];
    memcpy(fs_type, boot.file_system_type, 8);
    fs_type[8] = '\0';
    memcpy(volume_label, boot.volume_label, 11);
    volume_label[11] = '\0';
    
    if (debug_print_is_enabled()) {
        printf("[FAT32] Mount successful!\n");
        printf("[FAT32] Boot sector signature: 0x%x\n", boot_signature);
        printf("[FAT32] File system type: %s\n", fs_type);
        printf("[FAT32] Volume label: %s\n", volume_label);
        printf("[FAT32] Bytes per sector: %d\n", boot.bytes_per_sector);
        printf("[FAT32] Sectors per cluster: %d\n", boot.sectors_per_cluster);
        printf("[FAT32] Reserved sectors: %d\n", boot.reserved_sectors);
        printf("[FAT32] Total sectors: %d\n", boot.total_sectors_32);
        printf("[FAT32] Sectors per FAT: %d\n", fat_sectors);
        printf("[FAT32] Root directory cluster: %d\n", boot.root_dir_first_cluster);
        printf("[FAT32] Total clusters: %d\n", current_fs.total_clusters);
    }
    
    return 0;
}

// list the contents of a directory
int fat32_list_directory(const char *path) {
    if (!fs_mounted) {
        printf("[FAT32] File system not mounted\n");
        return -1;
    }

    // figure out which directory to list
    uint32_t dir_cluster;
    if (!path || path[0] == '\0') {
        // no path given — use current directory
        dir_cluster = current_fs.current_dir_cluster;
        // make sure we have a valid cluster
        if (dir_cluster == 0) {
            dir_cluster = current_fs.root_dir_cluster;
        }
    } else if (path[0] == '/' && path[1] == '\0') {
        // root directory
        dir_cluster = current_fs.root_dir_cluster;
    } else {
        // TODO: support listing arbitrary paths
        printf("[FAT32] Directory listing only supported for current dir and root\n");
        return -1;
    }

    // load the directory data from disk
    uint8_t dir_buffer[16384];
    if (fat32_load_directory_cluster(dir_cluster, dir_buffer, sizeof(dir_buffer)) != 0) {
        return -1;
    }

    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[FAT32] Listing directory (cluster=%d)\n", dir_cluster);
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }

    // iterate through entries and print them
    uint32_t max_entries = (current_fs.sectors_per_cluster * 512) / sizeof(fat32_dirent_t);
    for (uint32_t i = 0; i < max_entries; i++) {
        fat32_dirent_t *entry = (fat32_dirent_t *)(dir_buffer + (i * sizeof(fat32_dirent_t)));

        if (entry->name[0] == 0x00) {
            break;
        }
        if (entry->name[0] == 0xE5 || entry->attributes == ATTR_LFN) {
            continue;
        }

        char name[32];
        fat32_format_name(entry, name, sizeof(name));

        if (entry->attributes & ATTR_DIRECTORY) {
            printf("[DIR ] %s\n", name);
        } else {
            printf("[FILE] %s (%d bytes)\n", name, entry->file_size);
        }
    }

    return 0;
}

// convenience wrapper to list the root directory
int fat32_list_root(void) {
    return fat32_list_directory("/");
}

// check if a filename matches an 8.3 directory entry (case-insensitive)
static int fat32_name_match(const fat32_dirent_t *entry, const char *name) {
    // build the 8.3 name from the entry the same way fat32_format_name does
    char entry_name[13];
    fat32_format_name(entry, entry_name, sizeof(entry_name));

    // compare case-insensitively
    int i = 0;
    while (entry_name[i] && name[i]) {
        char ec = entry_name[i];
        char nc = name[i];
        if (ec >= 'a' && ec <= 'z') ec -= 32;
        if (nc >= 'a' && nc <= 'z') nc -= 32;
        if (ec != nc) return 0;
        i++;
    }
    return entry_name[i] == '\0' && name[i] == '\0';
}

// find a file in the root directory by name; fills out the dirent on success
static int fat32_find_in_root(const char *name, fat32_dirent_t *out) {
    if (fat32_load_root_directory() != 0) return -1;

    uint32_t max_entries = (current_fs.sectors_per_cluster * 512) / sizeof(fat32_dirent_t);
    for (uint32_t i = 0; i < max_entries; i++) {
        fat32_dirent_t *e = (fat32_dirent_t *)(root_dir_buffer + i * sizeof(fat32_dirent_t));
        if (e->name[0] == 0x00) break;
        if (e->name[0] == 0xE5 || e->attributes == ATTR_LFN) continue;
        if (fat32_name_match(e, name)) {
            memcpy(out, e, sizeof(fat32_dirent_t));
            return 0;
        }
    }
    return -1;
}

// open a file by path — returns a handle or null on failure
fat32_file_t* fat32_open(const char *path) {
    if (!fs_mounted) {
        printf("[FAT32] File system not mounted\n");
        return NULL;
    }

    // figure out which directory to search in
    uint32_t dir_cluster;
    const char *name = path;
    
    if (path[0] == '/') {
        // absolute path — search from root
        name++;
        dir_cluster = current_fs.root_dir_cluster;
    } else {
        // relative path — search from current directory
        dir_cluster = current_fs.current_dir_cluster;
        if (dir_cluster == 0 || dir_cluster < 2) {
            dir_cluster = current_fs.root_dir_cluster;
        }
    }

    // load the directory contents
    uint8_t dir_buffer[16384];
    uint8_t channel, drive;
    fat32_disk_to_ata(current_fs.disk, &channel, &drive);
    uint32_t lba = fat32_cluster_to_lba(dir_cluster);
    uint16_t sectors = current_fs.sectors_per_cluster;
    if ((uint32_t)sectors * 512 > sizeof(dir_buffer)) {
        sectors = sizeof(dir_buffer) / 512;
    }
    if (ata_read_sectors(channel, drive, lba, sectors, dir_buffer) != sectors) {
        printf("[FAT32] Failed to read directory cluster %d\n", dir_cluster);
        return NULL;
    }

    // look for the file by its 8.3 name
    uint8_t name83[11];
    if (fat32_make_name83(name, name83) != 0) {
        printf("[FAT32] Invalid 8.3 filename: %s\n", path);
        return NULL;
    }

    uint32_t max_entries = (current_fs.sectors_per_cluster * 512) / sizeof(fat32_dirent_t);
    for (uint32_t i = 0; i < max_entries; i++) {
        fat32_dirent_t *e = (fat32_dirent_t *)(dir_buffer + i * sizeof(fat32_dirent_t));
        if (e->name[0] == 0x00) break;
        if (e->name[0] == 0xE5 || e->attributes == ATTR_LFN) continue;
        if (memcmp(e->name, name83, 11) == 0) {
            if (e->attributes & ATTR_DIRECTORY) {
                printf("[FAT32] %s is a directory, not a file\n", path);
                return NULL;
            }

            uint32_t first_cluster = ((uint32_t)e->first_cluster_high << 16) | (uint32_t)e->first_cluster_low;

            open_file.first_cluster   = first_cluster;
            open_file.current_cluster = first_cluster;
            open_file.file_position   = 0;
            open_file.file_size       = e->file_size;
            open_file.sector_offset   = 0;
            open_file.disk            = current_fs.disk;
            open_file.partition       = current_fs.partition;
            open_file.attributes      = e->attributes;

            if (debug_print_is_enabled()) {
                uint8_t prev = vga_get_color();
                vga_set_color(14, 0);
                printf("[FAT32] Opened: %s (size=%d, cluster=%d)\n", path, e->file_size, first_cluster);
                vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
            }
            return &open_file;
        }
    }

    printf("[FAT32] File not found: %s\n", path);
    return NULL;
}

// read up to 'count' bytes from an open file into a buffer
int fat32_read(fat32_file_t *file, void *buffer, size_t count) {
    if (!fs_mounted) {
        printf("[FAT32] File system not mounted\n");
        return -1;
    }
    if (!file || !buffer) return -1;

    // don't read past the end of the file
    uint32_t remaining = file->file_size - file->file_position;
    if (count > remaining) count = remaining;
    if (count == 0) return 0;

    uint8_t channel, drive;
    fat32_disk_to_ata(file->disk, &channel, &drive);

    uint32_t cluster_size = current_fs.sectors_per_cluster * 512;
    uint8_t *out = (uint8_t *)buffer;
    uint32_t bytes_read = 0;

    while (bytes_read < (uint32_t)count) {
        if (!fat32_is_valid_cluster(file->current_cluster)) {
            printf("[FAT32] Invalid cluster %d during read\n", file->current_cluster);
            break;
        }

        // read the whole cluster into our temp buffer
        uint32_t lba = fat32_cluster_to_lba(file->current_cluster);
        if (ata_read_sectors(channel, drive, lba, current_fs.sectors_per_cluster, file_cluster_buf)
                != current_fs.sectors_per_cluster) {
            printf("[FAT32] Failed to read cluster %d\n", file->current_cluster);
            break;
        }

        // figure out our position within this cluster
        uint32_t cluster_offset = file->file_position % cluster_size;
        uint32_t available      = cluster_size - cluster_offset;
        uint32_t to_copy        = count - bytes_read;
        if (to_copy > available) to_copy = available;

        memcpy(out + bytes_read, file_cluster_buf + cluster_offset, (int)to_copy);
        bytes_read          += to_copy;
        file->file_position += to_copy;

        // if we finished this cluster, move to the next one in the chain
        if ((file->file_position % cluster_size) == 0) {
            uint32_t next = fat32_next_cluster(file->current_cluster);
            if (fat32_is_end_of_chain(next) || next == 0) break;
            file->current_cluster = next;
        }
    }

    return (int)bytes_read;
}

// write data to a file, creating or overwriting it
int fat32_write(const char *path, const void *buffer, size_t count) {
    if (!fs_mounted) {
        printf("[FAT32] File system not mounted\n");
        return -1;
    }
    if (!path || !buffer) return -1;
    // figure out the parent directory and filename
    uint32_t parent_cluster;
    fat32_dirent_t existing;
    char filename[256];
    int nav = fat32_navigate_path(path, &parent_cluster, &existing, filename);
    if (nav < 0) {
        printf("[FAT32] Path not found or invalid: %s\n", path);
        return -1;
    }

    const char *name = filename;
    if (!name || !name[0]) {
        // no filename provided
        printf("[FAT32] Invalid filename: %s\n", path);
        return -1;
    }

    uint8_t name83[11];
    if (fat32_make_name83(name, name83) != 0) {
        printf("[FAT32] Invalid 8.3 filename: %s\n", path);
        return -1;
    }

    // read the parent directory into a buffer
    uint8_t parent_buffer[16384];
    if (fat32_load_directory_cluster(parent_cluster, parent_buffer, sizeof(parent_buffer)) != 0) {
        return -1;
    }

    // look for an existing entry or a free slot
    uint32_t max_entries = (current_fs.sectors_per_cluster * 512) / sizeof(fat32_dirent_t);
    fat32_dirent_t *entry = NULL;
    fat32_dirent_t *free_entry = NULL;

    for (uint32_t i = 0; i < max_entries; i++) {
        fat32_dirent_t *e = (fat32_dirent_t *)(parent_buffer + i * sizeof(fat32_dirent_t));
        if (e->name[0] == 0x00 && !free_entry) {
            free_entry = e;
            break;
        }
        if (e->name[0] == 0xE5 || e->attributes == ATTR_LFN) {
            if (!free_entry) free_entry = e;
            continue;
        }
        if (memcmp(e->name, name83, 11) == 0) {
            entry = e;
            break;
        }
    }

    if (!entry) entry = free_entry;
    if (!entry) {
        printf("[FAT32] No free directory entries\n");
        return -1;
    }

    // free old cluster chain if this entry already had data
    uint32_t old_first_cluster = ((uint32_t)entry->first_cluster_high << 16) | (uint32_t)entry->first_cluster_low;
    if (old_first_cluster >= 2) {
        fat32_free_cluster_chain(old_first_cluster);
    }

    memset(entry, 0, sizeof(fat32_dirent_t));
    memcpy(entry->name, name83, 11);
    entry->attributes = ATTR_ARCHIVE;
    entry->first_cluster_high = 0;
    entry->first_cluster_low = 0;
    entry->file_size = 0;

    if (count == 0) {
        // write the parent directory back to disk
        uint8_t channel, drive;
        fat32_disk_to_ata(current_fs.disk, &channel, &drive);
        uint32_t lba = fat32_cluster_to_lba(parent_cluster);
        if (ata_write_sectors(channel, drive, lba, current_fs.sectors_per_cluster, parent_buffer) != current_fs.sectors_per_cluster) {
            printf("[FAT32] Failed to write parent directory cluster %d\n", parent_cluster);
            return -1;
        }
        // refresh root buffer if we modified root
        if (parent_cluster == current_fs.root_dir_cluster) fat32_load_root_directory();
        printf("[FAT32] Created empty file: %s\n", path);
        return 0;
    }

    // allocate clusters for the data
    uint32_t cluster_size = current_fs.sectors_per_cluster * 512;
    uint32_t clusters_needed = (count + cluster_size - 1) / cluster_size;
    uint32_t first_cluster = 0;
    if (fat32_allocate_cluster_chain(clusters_needed, &first_cluster) != 0) {
        printf("[FAT32] Failed to allocate cluster chain for %s\n", path);
        return -1;
    }

    uint8_t channel, drive;
    fat32_disk_to_ata(current_fs.disk, &channel, &drive);
    const uint8_t *src = (const uint8_t *)buffer;
    uint32_t remaining = (uint32_t)count;
    uint32_t cluster = first_cluster;

    // write data to each cluster in the chain
    while (remaining > 0 && fat32_is_valid_cluster(cluster)) {
        uint32_t chunk = remaining > cluster_size ? cluster_size : remaining;
        memset(file_cluster_buf, 0, cluster_size);
        memcpy(file_cluster_buf, src, (int)chunk);

        uint32_t lba = fat32_cluster_to_lba(cluster);
        if (ata_write_sectors(channel, drive, lba, current_fs.sectors_per_cluster, file_cluster_buf) != current_fs.sectors_per_cluster) {
            printf("[FAT32] Failed to write cluster %d\n", cluster);
            fat32_free_cluster_chain(first_cluster);
            return -1;
        }

        src += chunk;
        remaining -= chunk;

        uint32_t next = fat32_next_cluster(cluster);
        if (fat32_is_end_of_chain(next) || next == 0) break;
        cluster = next;
    }

    // update the directory entry with the new cluster and size
    entry->first_cluster_high = (uint16_t)((first_cluster >> 16) & 0xFFFF);
    entry->first_cluster_low  = (uint16_t)(first_cluster & 0xFFFF);
    entry->file_size = (uint32_t)count;

    // write the parent directory back to disk
    fat32_disk_to_ata(current_fs.disk, &channel, &drive);
    uint32_t lba = fat32_cluster_to_lba(parent_cluster);
    if (ata_write_sectors(channel, drive, lba, current_fs.sectors_per_cluster, parent_buffer) != current_fs.sectors_per_cluster) {
        printf("[FAT32] Failed to write parent directory cluster %d\n", parent_cluster);
        fat32_free_cluster_chain(first_cluster);
        return -1;
    }

    if (parent_cluster == current_fs.root_dir_cluster) {
        if (fat32_load_root_directory() != 0) {
            fat32_free_cluster_chain(first_cluster);
            return -1;
        }
    }

    printf("[FAT32] Wrote file: %s (%d bytes, first cluster=%d)\n", path, (int)count, first_cluster);
    return 0;
}

// create an empty file
int fat32_create(const char *path) {
    return fat32_write(path, "", 0);
}

// delete a file from the root directory
int fat32_delete(const char *path) {
    if (!fs_mounted) {
        printf("[FAT32] File system not mounted\n");
        return -1;
    }
    if (!path) return -1;

    if (fat32_load_root_directory() != 0) {
        return -1;
    }

    const char *name = path;
    if (name[0] == '/') name++;

    uint8_t name83[11];
    if (fat32_make_name83(name, name83) != 0) {
        printf("[FAT32] Invalid 8.3 filename: %s\n", path);
        return -1;
    }

    uint32_t max_entries = (current_fs.sectors_per_cluster * 512) / sizeof(fat32_dirent_t);
    for (uint32_t i = 0; i < max_entries; i++) {
        fat32_dirent_t *e = (fat32_dirent_t *)(root_dir_buffer + i * sizeof(fat32_dirent_t));
        if (e->name[0] == 0x00) break;
        if (e->name[0] == 0xE5 || e->attributes == ATTR_LFN) continue;
        if (memcmp(e->name, name83, 11) == 0) {
            uint32_t first_cluster = ((uint32_t)e->first_cluster_high << 16) | (uint32_t)e->first_cluster_low;
            if (first_cluster >= 2) {
                fat32_free_cluster_chain(first_cluster);
            }
            e->name[0] = 0xE5;
            e->attributes = 0;
            e->first_cluster_high = 0;
            e->first_cluster_low = 0;
            e->file_size = 0;
            if (fat32_save_root_directory() != 0) {
                return -1;
            }
            printf("[FAT32] Deleted file: %s\n", path);
            return 0;
        }
    }

    printf("[FAT32] File not found: %s\n", path);
    return -1;
}

// close an open file handle — just zeros it out
void fat32_close(fat32_file_t *file) {
    if (file) {
        file->first_cluster   = 0;
        file->current_cluster = 0;
        file->file_position   = 0;
        file->file_size       = 0;
    }
}

// open a directory for reading entries
fat32_dir_t* fat32_opendir(const char *path) {
    if (!fs_mounted) {
        printf("[FAT32] File system not mounted\n");
        return NULL;
    }
    
    printf("[FAT32] Opening directory: %s\n", path);
    if (!path || path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        if (fat32_load_root_directory() != 0) {
            return NULL;
        }
        return &root_dir_handle;
    }

    // TODO: implement subdirectory open
    return NULL;
}

// read the next entry from an open directory
int fat32_readdir(fat32_dir_t *dir, fat32_dirent_t *entry) {
    if (!fs_mounted) {
        printf("[FAT32] File system not mounted\n");
        return -1;
    }
    
    if (!dir || !entry) {
        return -1;
    }

    uint32_t max_entries = (current_fs.sectors_per_cluster * 512) / sizeof(fat32_dirent_t);
    while (dir->entry_index < max_entries) {
        fat32_dirent_t *raw = (fat32_dirent_t *)(root_dir_buffer + (dir->entry_index * sizeof(fat32_dirent_t)));
        dir->entry_index++;

        if (raw->name[0] == 0x00) {
            return 0;
        }
        if (raw->name[0] == 0xE5 || raw->attributes == ATTR_LFN) {
            continue;
        }

        memcpy(entry, raw, sizeof(fat32_dirent_t));
        return 1;
    }

    return 0;
}

// close a directory handle
void fat32_closedir(fat32_dir_t *dir) {
    if (dir) {
        // TODO: implement directory close
    }
}

// get file info (stub — not fully implemented yet)
int fat32_stat(const char *path, fat32_dirent_t *entry) {
    (void)entry;
    if (!fs_mounted) { printf("[FAT32] File system not mounted\n"); return -1; }
    printf("[FAT32] Stat: %s\n", path);
    return -1;
}

// helper: load a directory cluster into a buffer
static int fat32_load_directory_cluster(uint32_t cluster, uint8_t *buffer, size_t buffer_size) {
    uint8_t channel, drive;
    fat32_disk_to_ata(current_fs.disk, &channel, &drive);
    
    uint32_t lba = fat32_cluster_to_lba(cluster);
    uint16_t sectors = current_fs.sectors_per_cluster;
    
    if ((uint32_t)sectors * 512 > buffer_size) {
        sectors = buffer_size / 512;
    }
    
    if (ata_read_sectors(channel, drive, lba, sectors, buffer) != sectors) {
        printf("[FAT32] Failed to read directory cluster %d\n", cluster);
        return -1;
    }
    
    return 0;
}

// helper: search for a filename in a directory cluster
static int fat32_find_in_directory(uint32_t dir_cluster, const char *name, fat32_dirent_t *out) {
    uint8_t dir_buffer[16384];
    
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[FAT32] Searching for '%s' in cluster %d\n", name, dir_cluster);
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
    
    if (fat32_load_directory_cluster(dir_cluster, dir_buffer, sizeof(dir_buffer)) != 0) {
        return -1;
    }
    
    uint32_t max_entries = (current_fs.sectors_per_cluster * 512) / sizeof(fat32_dirent_t);
    for (uint32_t i = 0; i < max_entries; i++) {
        fat32_dirent_t *e = (fat32_dirent_t *)(dir_buffer + i * sizeof(fat32_dirent_t));
        if (e->name[0] == 0x00) break;
        if (e->name[0] == 0xE5 || e->attributes == ATTR_LFN) continue;
        
        if (debug_print_is_enabled()) {
            char entry_name[32];
            fat32_format_name(e, entry_name, sizeof(entry_name));
            uint8_t prev = vga_get_color();
            vga_set_color(14, 0);
            printf("[FAT32]   Entry: '%s' (attr=0x%02x)\n", entry_name, e->attributes);
            vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
        }
        
        if (fat32_name_match(e, name)) {
            if (debug_print_is_enabled()) {
                uint8_t prev = vga_get_color();
                vga_set_color(14, 0);
                printf("[FAT32]   Found match!\n");
                vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
            }
            memcpy(out, e, sizeof(fat32_dirent_t));
            return 0;
        }
    }
    
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[FAT32]   Not found\n");
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
    
    return -1;
}

// helper: navigate a path string and find the parent directory and filename
// returns the parent's cluster, optionally fills out the entry if found
static int fat32_navigate_path(const char *path, uint32_t *parent_cluster, fat32_dirent_t *entry, char *filename) {
    // start from root or current directory depending on whether the path is absolute
    uint32_t cluster = (path[0] == '/') ? current_fs.root_dir_cluster : current_fs.current_dir_cluster;
    
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[FAT32] Navigating path: '%s' (start cluster=%d)\n", path, cluster);
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
    
    // skip the leading slash
    const char *p = path;
    if (*p == '/') p++;
    
    // walk through each path component
    char component[256];
    int comp_idx = 0;
    
    while (*p) {
        if (*p == '/') {
            if (comp_idx > 0) {
                component[comp_idx] = '\0';
                
                if (debug_print_is_enabled()) {
                    uint8_t prev = vga_get_color();
                    vga_set_color(14, 0);
                    printf("[FAT32]   Component: '%s'\n", component);
                    vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
                }
                
                // look up this path component in the current directory
                fat32_dirent_t dir_entry;
                if (fat32_find_in_directory(cluster, component, &dir_entry) != 0) {
                    return -1;  // component not found
                }
                
                if (!(dir_entry.attributes & ATTR_DIRECTORY)) {
                    return -1;  // not a directory
                }
                
                // descend into this directory
                cluster = ((uint32_t)dir_entry.first_cluster_high << 16) | (uint32_t)dir_entry.first_cluster_low;
                comp_idx = 0;
            }
            p++;
        } else {
            if (comp_idx < 255) {
                component[comp_idx++] = *p;
            }
            p++;
        }
    }
    
    // the last component is the filename we're looking for
    if (comp_idx > 0) {
        component[comp_idx] = '\0';
        
        if (debug_print_is_enabled()) {
            uint8_t prev = vga_get_color();
            vga_set_color(14, 0);
            printf("[FAT32]   Final component: '%s'\n", component);
            vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
        }
        
        if (filename) {
            int i = 0;
            while (component[i] && i < 255) {
                filename[i] = component[i];
                i++;
            }
            filename[i] = '\0';
        }
        
        // see if we can find it
        if (entry && fat32_find_in_directory(cluster, component, entry) == 0) {
            *parent_cluster = cluster;
            return 1;  // found
        }
    }
    
    *parent_cluster = cluster;
    return 0;  // not found (but parent exists)
}

// create a new directory
int fat32_mkdir(const char *path) {
    if (!fs_mounted) {
        printf("[FAT32] File system not mounted\n");
        return -1;
    }
    
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[FAT32] Creating directory: %s\n", path);
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
    
    uint32_t parent_cluster;
    fat32_dirent_t existing;
    char dirname[256];
    
    int result = fat32_navigate_path(path, &parent_cluster, &existing, dirname);
    if (result == 1) {
        printf("[FAT32] Directory already exists: %s\n", path);
        return -1;
    }
    
    // allocate a cluster for the new directory
    uint32_t new_cluster;
    if (fat32_allocate_cluster_chain(1, &new_cluster) != 0) {
        printf("[FAT32] Failed to allocate cluster for directory\n");
        return -1;
    }
    
    // set up the standard . and .. entries
    uint8_t dir_buffer[16384];
    memset(dir_buffer, 0, sizeof(dir_buffer));
    
    fat32_dirent_t *dot = (fat32_dirent_t *)dir_buffer;
    memcpy(dot->name, ".          ", 11);
    dot->attributes = ATTR_DIRECTORY;
    dot->first_cluster_high = (uint16_t)((new_cluster >> 16) & 0xFFFF);
    dot->first_cluster_low = (uint16_t)(new_cluster & 0xFFFF);
    
    fat32_dirent_t *dotdot = (fat32_dirent_t *)(dir_buffer + sizeof(fat32_dirent_t));
    memcpy(dotdot->name, "..         ", 11);
    dotdot->attributes = ATTR_DIRECTORY;
    dotdot->first_cluster_high = (uint16_t)((parent_cluster >> 16) & 0xFFFF);
    dotdot->first_cluster_low = (uint16_t)(parent_cluster & 0xFFFF);
    
    // write the new directory cluster to disk
    uint8_t channel, drive;
    fat32_disk_to_ata(current_fs.disk, &channel, &drive);
    uint32_t lba = fat32_cluster_to_lba(new_cluster);
    if (ata_write_sectors(channel, drive, lba, current_fs.sectors_per_cluster, dir_buffer) != current_fs.sectors_per_cluster) {
        printf("[FAT32] Failed to write directory cluster\n");
        fat32_free_cluster_chain(new_cluster);
        return -1;
    }
    
    // add an entry for this new directory in the parent
    uint8_t parent_buffer[16384];
    if (fat32_load_directory_cluster(parent_cluster, parent_buffer, sizeof(parent_buffer)) != 0) {
        fat32_free_cluster_chain(new_cluster);
        return -1;
    }
    
    uint8_t name83[11];
    if (fat32_make_name83(dirname, name83) != 0) {
        printf("[FAT32] Invalid directory name: %s\n", dirname);
        fat32_free_cluster_chain(new_cluster);
        return -1;
    }
    
    // find a free slot in the parent directory
    uint32_t max_entries = (current_fs.sectors_per_cluster * 512) / sizeof(fat32_dirent_t);
    fat32_dirent_t *free_entry = NULL;
    for (uint32_t i = 0; i < max_entries; i++) {
        fat32_dirent_t *e = (fat32_dirent_t *)(parent_buffer + i * sizeof(fat32_dirent_t));
        if (e->name[0] == 0x00 || e->name[0] == 0xE5) {
            free_entry = e;
            break;
        }
    }
    
    if (!free_entry) {
        printf("[FAT32] No free entries in parent directory\n");
        fat32_free_cluster_chain(new_cluster);
        return -1;
    }
    
    memset(free_entry, 0, sizeof(fat32_dirent_t));
    memcpy(free_entry->name, name83, 11);
    free_entry->attributes = ATTR_DIRECTORY;
    free_entry->first_cluster_high = (uint16_t)((new_cluster >> 16) & 0xFFFF);
    free_entry->first_cluster_low = (uint16_t)(new_cluster & 0xFFFF);
    
    // write the parent directory back
    lba = fat32_cluster_to_lba(parent_cluster);
    if (ata_write_sectors(channel, drive, lba, current_fs.sectors_per_cluster, parent_buffer) != current_fs.sectors_per_cluster) {
        printf("[FAT32] Failed to write parent directory\n");
        fat32_free_cluster_chain(new_cluster);
        return -1;
    }
    
    // reload root directory if we modified it
    if (parent_cluster == current_fs.root_dir_cluster) {
        fat32_load_root_directory();
    }
    
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[FAT32] Created directory: %s (cluster=%d)\n", path, new_cluster);
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
    return 0;
}

// change the current working directory
int fat32_chdir(const char *path) {
    if (!fs_mounted) {
        printf("[FAT32] File system not mounted\n");
        return -1;
    }
    
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[FAT32] Changing directory to: %s\n", path);
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
    
    uint32_t parent_cluster;
    fat32_dirent_t entry;
    char dirname[256];
    
    int result = fat32_navigate_path(path, &parent_cluster, &entry, dirname);
    if (result != 1) {
        printf("[FAT32] Directory not found: %s\n", path);
        return -1;
    }
    
    if (!(entry.attributes & ATTR_DIRECTORY)) {
        printf("[FAT32] Not a directory: %s\n", path);
        return -1;
    }
    
    uint32_t new_cluster = ((uint32_t)entry.first_cluster_high << 16) | (uint32_t)entry.first_cluster_low;
    current_fs.current_dir_cluster = new_cluster;
    
    // update the tracked current path
    if (path[0] == '/') {
        // absolute path — use it as-is
        int i = 0;
        while (path[i] && i < 255) {
            current_fs.current_path[i] = path[i];
            i++;
        }
        current_fs.current_path[i] = '\0';
    } else {
        // relative path — append to current
        // first, handle special cases like . and ..
        if (dirname[0] == '.' && dirname[1] == '\0') {
            // stay in current directory
            return 0;
        } else if (dirname[0] == '.' && dirname[1] == '.' && dirname[2] == '\0') {
            // go up one level
            int len = 0;
            while (current_fs.current_path[len]) len++;
            // remove trailing slash if present
            if (len > 1 && current_fs.current_path[len-1] == '/') {
                len--;
            }
            // find the last slash
            while (len > 0 && current_fs.current_path[len] != '/') {
                len--;
            }
            if (len == 0) len = 1;  // keep at least root /
            current_fs.current_path[len] = '\0';
        } else {
            // append the directory name
            int len = 0;
            while (current_fs.current_path[len]) len++;
            
            // add slash if not at root
            if (len > 1 || (len == 1 && current_fs.current_path[0] != '/')) {
                if (current_fs.current_path[len-1] != '/') {
                    current_fs.current_path[len++] = '/';
                }
            } else if (len == 0) {
                current_fs.current_path[len++] = '/';
            }
            
            // append dirname
            int i = 0;
            while (dirname[i] && len < 255) {
                current_fs.current_path[len++] = dirname[i++];
            }
            current_fs.current_path[len] = '\0';
        }
    }
    
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[FAT32] Changed directory to: %s (cluster=%d)\n", current_fs.current_path, new_cluster);
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
    
    return 0;
}

// get the current working directory path
const char* fat32_getcwd(void) {
    if (!fs_mounted) return NULL;
    if (debug_print_is_enabled()) {
        uint8_t prev = vga_get_color();
        vga_set_color(14, 0);
        printf("[FAT32] getcwd: path='%s' cluster=%d\n", current_fs.current_path, current_fs.current_dir_cluster);
        vga_set_color(prev & 0x0F, (prev >> 4) & 0x0F);
    }
    return current_fs.current_path[0] ? current_fs.current_path : "/";
}
