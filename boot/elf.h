#pragma once

#include "elf_types.h"

#include <efi.h>
#include <efilib.h>

EFI_STATUS elf_load(EFI_FILE_HANDLE file, Elf64_Addr *entry);
