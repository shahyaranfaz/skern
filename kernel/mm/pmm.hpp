#pragma once

#include <cstdint>

namespace skern::pmm {

void init();

bool allocate_frame(uint64_t &phys_addr);

void free_frame(uint64_t phys_addr);

void reserve_range(uint64_t start, uint64_t end_exclusive);

void release_range(uint64_t start, uint64_t end_exclusive);

} // namespace skern::pmm
