# xos

xos - a minimal 64-bit x86-64 hobby os. the kernel is called phobos. features preemptive multitasking, a fat32 filesystem, pci enumeration, an interactive shell, and basic network drivers.

## run

```sh
make          # build the kernel and disk images
./run.sh      # graphical mode with vga/keyboard
./run.sh serial  # headless mode with serial terminal
```

dependencies: `gcc`, `ld`, `nasm`, `python3`, `qemu-system-x86_64`.
