#!/bin/bash

# QEMU runner script for XOS

OS_IMAGE="build/os.img"
HDD_IMAGE="build/os_hdd.img"
LOG_FILE="debug.log"

if [ ! -f "$OS_IMAGE" ]; then
    echo "Error: OS image not found at $OS_IMAGE"
    echo "Please run 'make' first to build the kernel"
    exit 1
fi

echo "Booting XOS kernel with QEMU..."
echo "========================================="
echo ""

rm -f "$LOG_FILE"
cp "$OS_IMAGE" "$HDD_IMAGE"

# Unbuffered output for debugging
qemu-system-x86_64 \
    -drive file="$OS_IMAGE",format=raw,if=floppy \
    -drive file="$HDD_IMAGE",format=raw,if=ide,index=0,media=disk \
    -boot order=a \
    -m 256M \
    -serial file:"$LOG_FILE" \
    -machine pc \
    -cpu host \
    -enable-kvm 2>/dev/null || \
    qemu-system-x86_64 \
    -drive file="$OS_IMAGE",format=raw,if=floppy \
    -drive file="$HDD_IMAGE",format=raw,if=ide,index=0,media=disk \
    -boot order=a \
    -m 256M \
    -serial file:"$LOG_FILE" \
    -machine pc

echo ""
echo "========================================="
echo "QEMU exited"
echo "Serial log saved to $LOG_FILE"
