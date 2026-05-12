#ifndef ELF_H
#define ELF_H

#include <stdint.h>

// Load an ELF64 user program from FAT32 into memory.
// On success, writes the entry address to entry_out and returns 0.
int elf_load_user_program(const char *path, uint64_t *entry_out);

#endif
