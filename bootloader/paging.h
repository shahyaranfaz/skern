#pragma once

#include <efi.h>
#include <stdint.h>

enum : uint64_t {
    PAGING_WRITABLE = 1ULL << 1,
    PAGING_NO_EXEC  = 1ULL << 63,
};

EFI_STATUS paging_init(uint64_t *root_phys_addr);

EFI_STATUS paging_map_page(uint64_t virtual_addr, uint64_t phys_addr, uint64_t flags);

EFI_STATUS paging_map_range(uint64_t virtual_start, uint64_t phys_start, uint64_t byte_size, uint64_t flags);