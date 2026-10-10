#include "elf.h"

#include <efi.h>
#include <efilib.h>
#include <stdbool.h>
#include <stddef.h>

static bool elf_validate(const Elf64_EHeader *header) {

}

static bool elf_validate_segments(const void *file, size_t file_size) {

}

EFI_STATUS elf_load(EFI_FILE_HANDLE file, Elf64_Addr *entry) {

}