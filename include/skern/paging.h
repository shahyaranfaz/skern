#pragma once

#include <stdint.h>

enum : uint64_t {
    SKERN_PAGE_SIZE = UINT64_C(4096),
    SKERN_PAGE_PRESENT = UINT64_C(1) << 0,
    SKERN_PAGE_WRITABLE = UINT64_C(1) << 1,
    SKERN_PAGE_USER = UINT64_C(1) << 2,
    SKERN_PAGE_WRITE_THROUGH = UINT64_C(1) << 3,
    SKERN_PAGE_CACHE_DISABLE = UINT64_C(1) << 4,
    SKERN_PAGE_ACCESSED = UINT64_C(1) << 5,
    SKERN_PAGE_HUGE = UINT64_C(1) << 7,
    SKERN_PAGE_NO_EXEC = UINT64_C(1) << 63,
    SKERN_PAGE_ENTRY_ADDRESS_MASK = UINT64_C(0x000FFFFFFFFFF000),
};

typedef struct {
    alignas(SKERN_PAGE_SIZE) uint64_t entries[512];
} SkernPageTable;

static_assert(sizeof(SkernPageTable) == SKERN_PAGE_SIZE);
static_assert(alignof(SkernPageTable) == SKERN_PAGE_SIZE);

static inline uint16_t skern_pml4_index(uint64_t virtual_addr) {
    return (virtual_addr >> 39) & UINT64_C(0x1FF);
}

static inline uint16_t skern_pdpt_index(uint64_t virtual_addr) {
    return (virtual_addr >> 30) & UINT64_C(0x1FF);
}

static inline uint16_t skern_pd_index(uint64_t virtual_addr) {
    return (virtual_addr >> 21) & UINT64_C(0x1FF);
}

static inline uint16_t skern_pt_index(uint64_t virtual_addr) {
    return (virtual_addr >> 12) & UINT64_C(0x1FF);
}

static inline bool skern_is_canonical_address(uint64_t virtual_addr) {
    uint64_t upper = virtual_addr >> 48;
    return upper == ((virtual_addr & (UINT64_C(1) << 47)) != 0 ? UINT64_C(0xFFFF) : 0);
}
