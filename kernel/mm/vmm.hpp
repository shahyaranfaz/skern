#pragma once

#include <cstdint>

namespace skern::vmm {

// clang-format off
enum : uint64_t {
    WRITABLE = 1ULL <<  1,
    USER     = 1ULL <<  2,
    NO_EXEC  = 1ULL << 63
};
// clang-format on

bool init();

bool map(uint64_t virtual_addr, uint64_t phys_addr, uint64_t flags);

bool unmap(uint64_t virtual_addr);

} // namespace skern::vmm
