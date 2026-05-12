#!/bin/bash

# QEMU runner script for XOS
#
# Usage:
#   ./run.sh          — graphical window (PS/2 keyboard + VGA)
#                       click the window to grab keyboard focus
#   ./run.sh serial   — headless, serial ↔ stdio (type in terminal)
#                       Ctrl+A then X to quit

OS_IMAGE="build/os.img"
STORAGE_IMAGE="build/storage.img"
LOG_FILE="debug.log"

if [ ! -f "$OS_IMAGE" ]; then
    echo "Error: OS image not found. Run 'make' first."
    exit 1
fi

if [ ! -f "$STORAGE_IMAGE" ]; then
    echo "Error: Storage image not found. Run 'make' first."
    exit 1
fi

QEMU_COMMON=(
    -drive file="$OS_IMAGE",format=raw,if=ide,index=0,media=disk
    -drive file="$STORAGE_IMAGE",format=raw,if=ide,index=1,media=disk
    -boot order=c
    -m 256M
    -machine pc
)

if [ "$1" = "serial" ]; then
    echo "Booting XOS (serial mode) — type here, Ctrl+A X to quit"
    echo ""
    qemu-system-x86_64 "${QEMU_COMMON[@]}" \
        -nographic -serial mon:stdio \
        -cpu host -enable-kvm 2>/dev/null || \
    qemu-system-x86_64 "${QEMU_COMMON[@]}" \
        -nographic -serial mon:stdio
else
    echo "Booting XOS (graphical) — click window for keyboard focus"
    qemu-system-x86_64 "${QEMU_COMMON[@]}" \
        -serial file:"$LOG_FILE" \
        -cpu host -enable-kvm 2>/dev/null || \
    qemu-system-x86_64 "${QEMU_COMMON[@]}" \
        -serial file:"$LOG_FILE"
fi
