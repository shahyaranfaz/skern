#include "elf_validate.h"

#include "elf_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static void copy_bytes(void *destination, const uint8_t *source, size_t count) {
    uint8_t *destination_bytes = destination;

    for (size_t i = 0; i < count; i++)
        destination_bytes[i] = source[i];
}

static bool power_of_two(uint64_t value) {
    return value > 0 && (value & (value - 1)) == 0;
}

static bool elf_validate_eheader(const Elf64_EHeader *header) {
    for (size_t i = 0; i < ELF_MAGIC_LEN; i++) {
        if (header->e_ident[i] != (unsigned char)ELF_MAGIC[i]) return false;
    }

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

static bool elf_validate_pheaders(const uint8_t *bytes, size_t file_size,
                                  const Elf64_EHeader *elf_header) {
    bool entry_is_executable = false;

    for (size_t i = 0; i < elf_header->e_phnum; i++) {
        Elf64_PHeader program_header;
        copy_bytes(&program_header, bytes + elf_header->e_phoff + i * sizeof(Elf64_PHeader),
                   sizeof(program_header));

        if (program_header.p_type == PT_INTERP || program_header.p_type == PT_DYNAMIC) return false;
        if (program_header.p_type != PT_LOAD || program_header.p_memsz == 0) continue;

        if (program_header.p_filesz > program_header.p_memsz) return false;
        if (program_header.p_offset > file_size) return false;
        if (program_header.p_filesz > file_size - program_header.p_offset) return false;
        if (program_header.p_memsz > UINT64_MAX - program_header.p_vaddr) return false;
        if (program_header.p_memsz > UINT64_MAX - program_header.p_paddr) return false;

        if (program_header.p_align > 1) {
            if (!power_of_two(program_header.p_align)) return false;
            if (program_header.p_vaddr % program_header.p_align !=
                program_header.p_offset % program_header.p_align)
                return false;
        }

        if ((program_header.p_flags & PF_X) && program_header.p_vaddr <= elf_header->e_entry &&
            elf_header->e_entry < program_header.p_vaddr + program_header.p_memsz) {
            entry_is_executable = true;
        }
    }

    return entry_is_executable;
}

bool elf_validate(const void *file, size_t file_size) {
    if (file == NULL || file_size < sizeof(Elf64_EHeader)) return false;

    Elf64_EHeader elf_header;
    copy_bytes(&elf_header, file, sizeof(elf_header));

    if (!elf_validate_eheader(&elf_header)) return false;
    if (elf_header.e_phoff > file_size) return false;
    if ((size_t)elf_header.e_phnum * sizeof(Elf64_PHeader) > file_size - elf_header.e_phoff)
        return false;

    return elf_validate_pheaders(file, file_size, &elf_header);
}
