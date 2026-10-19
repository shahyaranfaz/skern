#pragma once

#include <skern/boot_info.h>

#include <cstddef>
#include <cstdint>

namespace skern::memory_map {

enum MemoryType : uint16_t {
    Usable = 0,
    ReclaimableBootloader,
    ReclaimableBootServices,
    ReclaimableACPI,
    Runtime,
    Reserved,
    MMIO,
    Unusable,
    Persistent,
    Unaccepted
};

struct MemoryRegion {
    uint64_t start;
    uint64_t end_exclusive;
    uint64_t attributes;
    MemoryType type;
};

static_assert(sizeof(MemoryRegion) == 32);

bool normalize(const MemoryMapInfo &mem_map);

const MemoryRegion *get_regions();

size_t num_regions();

} // namespace skern::memory_map
