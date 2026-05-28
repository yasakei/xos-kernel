#!/bin/bash
# -------------------------------------------------------------------
# mit license
#
# copyright (c) 2026 xos
#
# permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation
# files (the "software"), to deal in the software without
# restriction, including without limitation the rights to use,
# copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the software, and to permit persons to whom the
# software is furnished to do so, subject to the following
# conditions:
#
# the above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the software.
#
# the software is provided "as is", without warranty of any kind,
# express or implied, including but not limited to the warranties
# of merchantability, fitness for a particular purpose and
# noninfringement. in no event shall the authors or copyright
# holders be liable for any claim, damages or other liability,
# whether in an action of contract, tort or otherwise, arising
# from, out of or in connection with the software or the use or
# other dealings in the software.
# -------------------------------------------------------------------

# qemu runner for xos
# usage:
#   ./run.sh          -> graphical window (ps/2 keyboard + vga)
#                          click the window to grab keyboard focus
#   ./run.sh serial   -> headless, serial <-> stdio (type in terminal)
#                          ctrl+c to quit

OS_IMAGE="build/os.img"
STORAGE_IMAGE="build/storage.img"
LOG_FILE="debug.log"

if [ ! -f "$OS_IMAGE" ]; then
    echo "error: os image not found. run 'make' first."
    exit 1
fi

if [ ! -f "$STORAGE_IMAGE" ]; then
    echo "error: storage image not found. run 'make' first."
    exit 1
fi

QEMU_COMMON=(
    -drive file="$OS_IMAGE",format=raw,if=ide,index=0,media=disk
    -drive file="$STORAGE_IMAGE",format=raw,if=ide,index=1,media=disk
    -boot order=c
    -m 256M
    -machine pc
    -device rtl8139
)

if [ "$1" = "serial" ]; then
    echo "booting xos (serial mode, stable/tcg) - direct com1 on stdio, ctrl+c to quit"
    echo ""
    # serial/headless mode is for low-level debugging and tracing.
    # keep it deterministic - no kvm acceleration here.
    # if running interactively, put the terminal in raw mode so control
    # characters (like ctrl+c) go to the guest's serial port instead
    # of being eaten by the host shell. save/restore terminal state.
    if [ -t 0 ]; then
        OLD_STTY=$(stty -g)
        stty raw -echo
        restore_stty() { stty "$OLD_STTY"; }
        trap restore_stty EXIT TERM
        trap '' INT
    fi

    qemu-system-x86_64 "${QEMU_COMMON[@]}" \
        -nographic -monitor none -serial stdio

    # restore terminal state if not already done
    if [ -n "${OLD_STTY:-}" ] ; then
        stty "$OLD_STTY" 2>/dev/null || true
        trap - EXIT TERM INT
    fi
else
    echo "booting xos (graphical) - click window for keyboard focus"
    # try with kvm first (faster), fall back to tcg
    qemu-system-x86_64 "${QEMU_COMMON[@]}" \
        -serial file:"$LOG_FILE" \
        -cpu host -enable-kvm 2>/dev/null || \
    qemu-system-x86_64 "${QEMU_COMMON[@]}" \
        -serial file:"$LOG_FILE"
fi
