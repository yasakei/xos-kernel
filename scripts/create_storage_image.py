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


def write_u16(buf, offset, value):
    struct.pack_into('<H', buf, offset, value)


def write_u32(buf, offset, value):
    struct.pack_into('<I', buf, offset, value)


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


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else 'build/storage.img'
    os.makedirs(os.path.dirname(out), exist_ok=True)
    image = bytearray(IMAGE_SIZE)
    make_mbr(image)
    make_fat32_boot_sector(image)
    with open(out, 'wb') as f:
        f.write(image)
    print(f'Created {out} ({IMAGE_SIZE // (1024 * 1024)} MiB)')


if __name__ == '__main__':
    main()
