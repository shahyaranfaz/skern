#include "mm/memory_map.hpp"

#include <skern/boot_info.h>
#include <skern/memory_layout.h>
#include <skern/paging.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

struct EfiMemoryDescriptor {
    uint32_t type;
    uint32_t padding;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attributes;
};

static_assert(sizeof(EfiMemoryDescriptor) == 40);

bool expect(bool condition, const char *message) {
    if (condition) return true;
    std::fprintf(stderr, "%s\n", message);
    return false;
}

MemoryMapInfo make_map(EfiMemoryDescriptor *descriptors, size_t count) {
    MemoryMapInfo map{};
    map.descriptors = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(descriptors)) - SKERN_HHDM_BASE;
    map.byte_size = count * sizeof(EfiMemoryDescriptor);
    map.descriptor_size = sizeof(EfiMemoryDescriptor);
    map.descriptor_version = 1;
    return map;
}

bool sorts_and_merges_regions() {
    EfiMemoryDescriptor descriptors[] = {
        {7, 0, 0x3000, 0, 1, 0},
        {7, 0, 0x1000, 0, 1, 0},
        {7, 0, 0x2000, 0, 1, 0},
        {8, 0, 0x4000, 0, 1, 0},
    };

    MemoryMapInfo map = make_map(descriptors, 4);
    if (!expect(skern::memory_map::normalize(map), "valid unsorted map was rejected")) return false;
    if (!expect(skern::memory_map::num_regions() == 2, "adjacent compatible regions were not merged")) return false;

    const auto *regions = skern::memory_map::get_regions();
    if (!expect(regions[0].start == 0x1000 && regions[0].end_exclusive == 0x4000,
                "usable regions were not sorted and merged")) {
        return false;
    }
    if (!expect(regions[0].type == skern::memory_map::MemoryType::Usable, "usable type was converted incorrectly")) {
        return false;
    }
    return expect(regions[1].start == 0x4000 && regions[1].type == skern::memory_map::MemoryType::Unusable,
                  "unusable region was converted incorrectly");
}

bool converts_types_and_runtime_override() {
    EfiMemoryDescriptor descriptors[] = {
        {2, 0, 0x1000, 0, 1, 0},
        {4, 0, 0x2000, 0, 1, 0},
        {7, 0, 0x3000, 0, 1, UINT64_C(1) << 63},
        {9, 0, 0x4000, 0, 1, 0},
        {11, 0, 0x5000, 0, 1, 0},
    };

    MemoryMapInfo map = make_map(descriptors, 5);
    if (!expect(skern::memory_map::normalize(map), "type conversion map was rejected")) return false;

    const auto *regions = skern::memory_map::get_regions();
    const skern::memory_map::MemoryType expected[] = {
        skern::memory_map::MemoryType::ReclaimableBootloader,
        skern::memory_map::MemoryType::ReclaimableBootServices,
        skern::memory_map::MemoryType::Runtime,
        skern::memory_map::MemoryType::ReclaimableACPI,
        skern::memory_map::MemoryType::MMIO,
    };
    for (size_t i = 0; i < 5; i++) {
        if (!expect(regions[i].type == expected[i], "descriptor type was converted incorrectly")) return false;
    }
    return true;
}

bool rejects_overlapping_regions() {
    EfiMemoryDescriptor descriptors[] = {
        {7, 0, 0x1000, 0, 2, 0},
        {7, 0, 0x2000, 0, 1, 0},
    };
    MemoryMapInfo map = make_map(descriptors, 2);
    return expect(!skern::memory_map::normalize(map), "overlapping regions were accepted");
}

bool rejects_malformed_maps() {
    EfiMemoryDescriptor descriptors[] = {{7, 0, 0x1000, 0, 1, 0}};
    MemoryMapInfo map = make_map(descriptors, 1);

    map.descriptor_version = 2;
    if (!expect(!skern::memory_map::normalize(map), "unsupported descriptor version was accepted")) return false;

    map = make_map(descriptors, 1);
    map.descriptor_size = sizeof(EfiMemoryDescriptor) - 8;
    if (!expect(!skern::memory_map::normalize(map), "short descriptor was accepted")) return false;

    map = make_map(descriptors, 1);
    map.byte_size--;
    if (!expect(!skern::memory_map::normalize(map), "partial descriptor was accepted")) return false;

    descriptors[0].number_of_pages = 0;
    map = make_map(descriptors, 1);
    if (!expect(!skern::memory_map::normalize(map), "zero-page descriptor was accepted")) return false;

    descriptors[0].number_of_pages = 1;
    descriptors[0].physical_start = 1;
    map = make_map(descriptors, 1);
    return expect(!skern::memory_map::normalize(map), "unaligned physical address was accepted");
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    if (std::strcmp(argv[1], "sort-merge") == 0) return sorts_and_merges_regions() ? 0 : 1;
    if (std::strcmp(argv[1], "types") == 0) return converts_types_and_runtime_override() ? 0 : 1;
    if (std::strcmp(argv[1], "overlap") == 0) return rejects_overlapping_regions() ? 0 : 1;
    if (std::strcmp(argv[1], "malformed") == 0) return rejects_malformed_maps() ? 0 : 1;
    return 2;
}
