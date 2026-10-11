#pragma once

#include <efi.h>
#include <efilib.h>

#include "elf_types.h"

EFI_STATUS elf_load(EFI_FILE_HANDLE file, Elf64_Addr *entry);
