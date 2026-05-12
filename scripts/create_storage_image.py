#!/usr/bin/env python3
import os
import struct
import sys

SECTOR_SIZE = 512
IMAGE_SIZE = 64 * 1024 * 1024
PARTITION_START = 2048
PARTITION_SIZE_SECTORS = (IMAGE_SIZE // SECTOR_SIZE) - PARTITION_START
SECTORS_PER_CLUSTER = 8
RESERVED_SECTORS = 32
NUM_FATS = 2
SECTORS_PER_FAT = 1024
ROOT_CLUSTER = 2
FAT_START = PARTITION_START + RESERVED_SECTORS
DATA_START = FAT_START + (NUM_FATS * SECTORS_PER_FAT)


def lba_of_cluster(cluster):
    return DATA_START + ((cluster - 2) * SECTORS_PER_CLUSTER)


def write_u16(buf, offset, value):
    struct.pack_into('<H', buf, offset, value)


def write_u32(buf, offset, value):
    struct.pack_into('<I', buf, offset, value)


def clusters_needed(size_bytes):
    cluster_bytes = SECTORS_PER_CLUSTER * SECTOR_SIZE
    if size_bytes == 0:
        return 1
    return (size_bytes + cluster_bytes - 1) // cluster_bytes


def make_mbr(image):
    mbr = bytearray(SECTOR_SIZE)
    # Partition entry at offset 446
    entry = 446
    mbr[entry + 0] = 0x00  # boot flag
    mbr[entry + 1] = 0x00  # start CHS (unused)
    mbr[entry + 2] = 0x02
    mbr[entry + 3] = 0x00
    mbr[entry + 4] = 0x0C  # FAT32 LBA
    mbr[entry + 5] = 0x00  # end CHS (unused)
    mbr[entry + 6] = 0x00
    mbr[entry + 7] = 0x00
    write_u32(mbr, entry + 8, PARTITION_START)
    write_u32(mbr, entry + 12, PARTITION_SIZE_SECTORS)
    write_u16(mbr, 510, 0xAA55)
    image[0:SECTOR_SIZE] = mbr


def make_fat32_boot_sector(image):
    bs = bytearray(SECTOR_SIZE)
    bs[0:3] = b'\xEB\x58\x90'
    bs[3:11] = b'XOSBOOT '
    write_u16(bs, 11, SECTOR_SIZE)
    bs[13] = SECTORS_PER_CLUSTER
    write_u16(bs, 14, RESERVED_SECTORS)
    bs[16] = NUM_FATS
    write_u16(bs, 17, 0)
    write_u16(bs, 19, 0)
    bs[21] = 0xF8
    write_u16(bs, 22, 0)
    write_u16(bs, 24, 63)
    write_u16(bs, 26, 255)
    write_u32(bs, 28, PARTITION_START)
    write_u32(bs, 32, PARTITION_SIZE_SECTORS)
    write_u32(bs, 36, SECTORS_PER_FAT)
    write_u16(bs, 40, 0)
    write_u16(bs, 42, 0)
    write_u32(bs, 44, ROOT_CLUSTER)
    write_u16(bs, 48, 1)
    write_u16(bs, 50, 6)
    bs[52:64] = b'\x00' * 12
    bs[64] = 0x80
    bs[65] = 0
    bs[66] = 0x29
    write_u32(bs, 67, 0x12345678)
    bs[71:82] = b'XOS STORAGE'
    bs[82:90] = b'FAT32   '
    write_u16(bs, 510, 0xAA55)
    image[PARTITION_START * SECTOR_SIZE:(PARTITION_START + 1) * SECTOR_SIZE] = bs


def make_fat_tables(image, files):
    fat = bytearray(SECTORS_PER_FAT * SECTOR_SIZE)

    def set_fat_entry(cluster, value):
        offset = cluster * 4
        struct.pack_into('<I', fat, offset, value & 0x0FFFFFFF)

    # FAT reserved entries
    set_fat_entry(0, 0x0FFFFFF8)
    set_fat_entry(1, 0x0FFFFFFF)
    set_fat_entry(ROOT_CLUSTER, 0x0FFFFFFF)

    for f in files:
        first = f['first_cluster']
        count = f['cluster_count']
        for i in range(count):
            c = first + i
            if i == count - 1:
                set_fat_entry(c, 0x0FFFFFFF)
            else:
                set_fat_entry(c, c + 1)

    fat1_start = FAT_START * SECTOR_SIZE
    fat2_start = (FAT_START + SECTORS_PER_FAT) * SECTOR_SIZE
    image[fat1_start:fat1_start + len(fat)] = fat
    image[fat2_start:fat2_start + len(fat)] = fat


def make_root_directory(image, files):
    root = bytearray(SECTORS_PER_CLUSTER * SECTOR_SIZE)

    def put_name(entry, name, ext):
        n = (name[:8].ljust(8) + ext[:3].ljust(3)).encode('ascii')
        entry[0:11] = n

    def write_entry(offset, name, ext, attr, first_cluster, size):
        entry = bytearray(32)
        put_name(entry, name, ext)
        entry[11] = attr
        struct.pack_into('<H', entry, 20, (first_cluster >> 16) & 0xFFFF)
        struct.pack_into('<H', entry, 26, first_cluster & 0xFFFF)
        struct.pack_into('<I', entry, 28, size)
        root[offset:offset + 32] = entry

    for i, f in enumerate(files):
        write_entry(i * 32, f['name'], f['ext'], 0x20, f['first_cluster'], len(f['data']))
    root[len(files) * 32] = 0x00  # end marker

    root_start = lba_of_cluster(ROOT_CLUSTER) * SECTOR_SIZE
    image[root_start:root_start + len(root)] = root


def make_file_data(image, files):
    def write_cluster(cluster, data):
        start = lba_of_cluster(cluster) * SECTOR_SIZE
        buf = bytearray(SECTORS_PER_CLUSTER * SECTOR_SIZE)
        buf[:len(data)] = data
        image[start:start + len(buf)] = buf

    cluster_bytes = SECTORS_PER_CLUSTER * SECTOR_SIZE
    for f in files:
        data = f['data']
        for i in range(f['cluster_count']):
            chunk = data[i * cluster_bytes:(i + 1) * cluster_bytes]
            write_cluster(f['first_cluster'] + i, chunk)


def build_file_table(user_elf_path=None):
    files = [
        {
            'name': 'HELLO',
            'ext': 'TXT',
            'data': b'Hello from XOS!\n',
        },
        {
            'name': 'README',
            'ext': 'TXT',
            'data': b'FAT32 listing demo\n',
        },
    ]

    if user_elf_path and os.path.exists(user_elf_path):
        with open(user_elf_path, 'rb') as ef:
            elf_data = ef.read()
        files.append({
            'name': 'HELLO',
            'ext': 'ELF',
            'data': elf_data,
        })

    next_cluster = 3
    for f in files:
        f['cluster_count'] = clusters_needed(len(f['data']))
        f['first_cluster'] = next_cluster
        next_cluster += f['cluster_count']

    return files


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else 'build/storage.img'
    user_elf_path = sys.argv[2] if len(sys.argv) > 2 else None
    os.makedirs(os.path.dirname(out), exist_ok=True)
    image = bytearray(IMAGE_SIZE)
    files = build_file_table(user_elf_path)
    make_mbr(image)
    make_fat32_boot_sector(image)
    make_fat_tables(image, files)
    make_root_directory(image, files)
    make_file_data(image, files)
    with open(out, 'wb') as f:
        f.write(image)
    print(f'Created {out} ({IMAGE_SIZE // (1024 * 1024)} MiB) with {len(files)} file(s)')


if __name__ == '__main__':
    main()
