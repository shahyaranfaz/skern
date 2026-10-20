#pragma once

#include <cstdint>

namespace skern::pmm {

void init();

bool allocate_frame(uint64_t &phys_addr);

void free_frame(uint64_t phys_addr);

} // namespace skern::pmm
