#include "elf.h"

#include <efi.h>
#include <efilib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool power_of_two(uint64_t n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}

static bool elf_validate_header(const Elf64_EHeader *header) {
    if (CompareMem(header->e_ident, ELF_MAGIC, ELF_MAGIC_LEN) != 0) return false;
    // Use EFI function cuz we may not have string.h

    if (header->e_ident[EI_CLASS] != ELFCLASS64) return false;
    if (header->e_ident[EI_DATA] != ELFDATA2LSB) return false;
    if (header->e_ident[EI_VERSION] != EV_CURRENT) return false;
    if (header->e_ident[EI_OSABI] != ELFOSABI_SYSV) return false;
    if (header->e_type != ET_EXEC) return false;
    if (header->e_machine != EM_X86_64) return false;
    if (header->e_version != EV_CURRENT) return false;
    if (header->e_ehsize != sizeof(Elf64_EHeader)) return false;
    if (header->e_phentsize != sizeof(Elf64_PHeader)) return false;
    if (header->e_phnum == 0) return false;
    return true;
}

static bool elf_validate(const void *file, size_t file_size) {
    if (file_size < sizeof(Elf64_EHeader)) return false;

    const Elf64_EHeader *elf_header = file;
    if (!elf_validate_header(elf_header)) return false;
    if (elf_header->e_phoff > file_size) return false;
    if ((size_t)elf_header->e_phnum * sizeof(Elf64_PHeader) > file_size - elf_header->e_phoff) return false;

    bool header_executable = false;

    const uint8_t *bytes = file;
    for (size_t i = 0; i < elf_header->e_phnum; i++) {
        const uint8_t *ptr = bytes + elf_header->e_phoff + i * sizeof(Elf64_PHeader);
        Elf64_PHeader p_header;
        CopyMem(&p_header, (void *)ptr, sizeof(p_header));

        switch (p_header.p_type) {
            case PT_LOAD:
                break;
            case PT_INTERP:
            case PT_DYNAMIC:
                return false;
            default:
                continue;
        }

        if (p_header.p_filesz > p_header.p_memsz) return false;
        if (p_header.p_offset > file_size) return false;
        if (p_header.p_filesz > file_size - p_header.p_offset) return false;
        if (p_header.p_memsz > UINT64_MAX - p_header.p_vaddr) return false;

        if (p_header.p_align > 1) {
            if (!power_of_two(p_header.p_align)) return false;

            if (p_header.p_vaddr % p_header.p_align != p_header.p_offset % p_header.p_align)
                return false;
        }

        if ((p_header.p_flags & PF_X) &&
            p_header.p_vaddr <= elf_header->e_entry &&
            elf_header->e_entry < p_header.p_vaddr + p_header.p_memsz) {
            header_executable = true;
        }
    }
    return header_executable;
}

EFI_STATUS elf_load(EFI_FILE_HANDLE file, Elf64_Addr *entry) {

}