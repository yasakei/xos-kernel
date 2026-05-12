#include "elf.h"
#include "../fs/fat32.h"
#include "../lib/printf.h"
#include "../lib/debuglog.h"
#include <stdint.h>
#include <stddef.h>

#define EI_NIDENT 16
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define ET_EXEC 2
#define EM_X86_64 62
#define PT_LOAD 1

#define ELF_MAX_PHDRS 32
#define ELF_SCRATCH_CHUNK 128

// Current kernel identity-map covers first 256MB.
#define USER_LOAD_MIN 0x00200000ULL
#define USER_LOAD_MAX 0x10000000ULL

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint64_t      e_entry;
    uint64_t      e_phoff;
    uint64_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
} __attribute__((packed)) elf64_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) elf64_phdr_t;

static void *memset_local(void *dst, int value, size_t count) {
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < count; i++) d[i] = (uint8_t)value;
    return dst;
}

static int read_exact(fat32_file_t *f, void *dst, size_t count) {
    uint8_t *out = (uint8_t *)dst;
    size_t total = 0;
    while (total < count) {
        int n = fat32_read(f, out + total, count - total);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return 0;
}

static int skip_bytes(fat32_file_t *f, uint64_t count) {
    uint8_t scratch[ELF_SCRATCH_CHUNK];
    while (count > 0) {
        size_t chunk = (count > ELF_SCRATCH_CHUNK) ? ELF_SCRATCH_CHUNK : (size_t)count;
        int n = fat32_read(f, scratch, chunk);
        if (n <= 0) return -1;
        count -= (uint64_t)n;
    }
    return 0;
}

int elf_load_user_program(const char *path, uint64_t *entry_out) {
    if (!path || !entry_out) {
        return -1;
    }

    fat32_file_t *f = fat32_open(path);
    if (!f) {
        printf("[ELF] open failed: %s\n", path);
        return -1;
    }

    elf64_ehdr_t eh;
    if (read_exact(f, &eh, sizeof(eh)) != 0) {
        printf("[ELF] failed to read ELF header\n");
        fat32_close(f);
        return -1;
    }

    if (!(eh.e_ident[0] == 0x7F && eh.e_ident[1] == 'E' && eh.e_ident[2] == 'L' && eh.e_ident[3] == 'F')) {
        printf("[ELF] invalid magic\n");
        fat32_close(f);
        return -1;
    }
    if (eh.e_ident[4] != ELFCLASS64 || eh.e_ident[5] != ELFDATA2LSB) {
        printf("[ELF] unsupported class/endianness\n");
        fat32_close(f);
        return -1;
    }
    if (eh.e_type != ET_EXEC || eh.e_machine != EM_X86_64) {
        printf("[ELF] unsupported type/machine\n");
        fat32_close(f);
        return -1;
    }
    if (eh.e_phentsize != sizeof(elf64_phdr_t) || eh.e_phnum == 0 || eh.e_phnum > ELF_MAX_PHDRS) {
        printf("[ELF] invalid program header table\n");
        fat32_close(f);
        return -1;
    }

    if (eh.e_entry < USER_LOAD_MIN || eh.e_entry >= USER_LOAD_MAX) {
        printf("[ELF] entry out of supported range: %p\n", (void *)eh.e_entry);
        fat32_close(f);
        return -1;
    }

    if ((uint64_t)f->file_position > eh.e_phoff) {
        printf("[ELF] invalid e_phoff\n");
        fat32_close(f);
        return -1;
    }

    if (skip_bytes(f, eh.e_phoff - (uint64_t)f->file_position) != 0) {
        printf("[ELF] failed to seek to program headers\n");
        fat32_close(f);
        return -1;
    }

    elf64_phdr_t phdrs[ELF_MAX_PHDRS];
    for (uint16_t i = 0; i < eh.e_phnum; i++) {
        if (read_exact(f, &phdrs[i], sizeof(elf64_phdr_t)) != 0) {
            printf("[ELF] failed to read phdr %d\n", i);
            fat32_close(f);
            return -1;
        }
    }
    fat32_close(f);

    for (uint16_t i = 0; i < eh.e_phnum; i++) {
        elf64_phdr_t *p = &phdrs[i];
        if (p->p_type != PT_LOAD) continue;

        if (p->p_memsz < p->p_filesz) {
            printf("[ELF] invalid PT_LOAD sizes\n");
            return -1;
        }
        if (p->p_vaddr < USER_LOAD_MIN || (p->p_vaddr + p->p_memsz) >= USER_LOAD_MAX) {
            printf("[ELF] PT_LOAD out of supported range\n");
            return -1;
        }

        fat32_file_t *segf = fat32_open(path);
        if (!segf) {
            printf("[ELF] reopen failed for segment\n");
            return -1;
        }

        if (skip_bytes(segf, p->p_offset) != 0) {
            printf("[ELF] failed to seek segment offset\n");
            fat32_close(segf);
            return -1;
        }

        if (read_exact(segf, (void *)(uintptr_t)p->p_vaddr, (size_t)p->p_filesz) != 0) {
            printf("[ELF] failed to load segment data\n");
            fat32_close(segf);
            return -1;
        }
        fat32_close(segf);

        if (p->p_memsz > p->p_filesz) {
            memset_local((void *)(uintptr_t)(p->p_vaddr + p->p_filesz), 0, (size_t)(p->p_memsz - p->p_filesz));
        }
    }

    *entry_out = eh.e_entry;
    if (debug_print_is_enabled()) {
        printf("[ELF] loaded %s (entry=%p)\n", path, (void *)eh.e_entry);
    }
    return 0;
}
