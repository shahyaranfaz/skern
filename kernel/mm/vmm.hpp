#pragma once

#include <skern/paging.h>

#include <cstdint>

namespace skern::vmm {

// clang-format off
enum : uint64_t {
    WRITABLE = SKERN_PAGE_WRITABLE,
    USER     = SKERN_PAGE_USER,
    NO_EXEC  = SKERN_PAGE_NO_EXEC
};
// clang-format on

bool init();

bool map(uint64_t virtual_addr, uint64_t phys_addr, uint64_t flags);

bool unmap(uint64_t virtual_addr);

} // namespace skern::vmm
