#include "elf.h"

#include <efi.h>
#include <efilib.h>
#include <stdbool.h>
#include <stddef.h>

static bool elf_validate_header(const Elf64_EHeader *header) {
    if (CompareMem(header->e_ident, ELF_MAGIC, ELF_MAGIC_LEN) != 0);
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

static bool elf_validate_segments(const void *file, size_t file_size) {


}

EFI_STATUS elf_load(EFI_FILE_HANDLE file, Elf64_Addr *entry) {

}