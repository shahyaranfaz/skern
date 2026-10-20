#include "memory_map.hpp"

#include "page.hpp"

#include <skern/boot_info.h>

#include <cstddef>
#include <cstdint>

namespace {
using namespace skern;

memory_map::MemoryRegion *regions;
size_t region_count = 0;

struct EfiMemoryDescriptor {
    uint32_t type;
    // 4 bytes of padding added by compiler
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attributes;
};

static_assert(sizeof(EfiMemoryDescriptor) == 40);
static_assert(alignof(EfiMemoryDescriptor) == 8);
static_assert(offsetof(EfiMemoryDescriptor, physical_start) == 8);

// clang-format off
enum EfiMemoryType : uint32_t {
    EfiReservedMemoryType      = 0,
    EfiLoaderCode              = 1,
    EfiLoaderData              = 2,
    EfiBootServicesCode        = 3,
    EfiBootServicesData        = 4,
    EfiRuntimeServicesCode     = 5,
    EfiRuntimeServicesData     = 6,
    EfiConventionalMemory      = 7,
    EfiUnusableMemory          = 8,
    EfiACPIReclaimMemory       = 9,
    EfiACPIMemoryNVS           = 10,
    EfiMemoryMappedIO          = 11,
    EfiMemoryMappedIOPortSpace = 12,
    EfiPalCode                 = 13,
    EfiPersistentMemory        = 14,
    EfiUnacceptedMemoryType    = 15,
    EfiMaxMemoryType           = 16,

    MEMORY_TYPE_OEM_RESERVED_MIN = 0x70000000,
    MEMORY_TYPE_OEM_RESERVED_MAX = 0x7FFFFFFF,
    MEMORY_TYPE_OS_RESERVED_MIN  = 0x80000000,
    MEMORY_TYPE_OS_RESERVED_MAX  = 0xFFFFFFFF
};
// clang-format on

enum : uint32_t {
    EFI_MEMORY_DESCRIPTOR_VERSION = 1,
};

enum : uint64_t {
    EFI_MEMORY_RUNTIME = 1ULL << 63,
};

bool validate_descriptor(const EfiMemoryDescriptor &descriptor) {
    if (descriptor.physical_start % page::SIZE) return false;
    if (descriptor.number_of_pages == 0) return false;

    if (descriptor.number_of_pages > UINT64_MAX / page::SIZE) return false;
    uint64_t size = descriptor.number_of_pages * page::SIZE;
    if (descriptor.physical_start > UINT64_MAX - size) return false;

    if (descriptor.type < EfiMaxMemoryType) return true;

    if (descriptor.type >= MEMORY_TYPE_OEM_RESERVED_MIN && descriptor.type <= MEMORY_TYPE_OEM_RESERVED_MAX) {
        return true;
    }

    if (descriptor.type >= MEMORY_TYPE_OS_RESERVED_MIN && descriptor.type <= MEMORY_TYPE_OS_RESERVED_MAX) {
        return true;
    }

    return false;
}

bool validate(const MemoryMapInfo &raw) {
    if (raw.descriptor_version != EFI_MEMORY_DESCRIPTOR_VERSION) return false;

    if (raw.descriptor_size < sizeof(EfiMemoryDescriptor)) return false;
    if (raw.descriptor_size % alignof(EfiMemoryDescriptor) != 0) return false;

    if (raw.descriptors == 0) return false;
    if (raw.descriptors % alignof(EfiMemoryDescriptor) != 0) return false;
    if (raw.descriptors > UINT64_MAX - raw.byte_size) return false;

    if (raw.byte_size == 0) return false;
    if (raw.byte_size % raw.descriptor_size != 0) return false;

    const uint8_t *base = reinterpret_cast<const uint8_t *>(raw.descriptors);

    for (uint64_t i = 0; i < raw.byte_size; i += raw.descriptor_size) {
        const EfiMemoryDescriptor *descriptor = reinterpret_cast<const EfiMemoryDescriptor *>(base + i);
        if (!validate_descriptor(*descriptor)) return false;
    }
    return true;
}

memory_map::MemoryType convert_type(const EfiMemoryDescriptor &descriptor) {
    if ((descriptor.attributes & EFI_MEMORY_RUNTIME) != 0) return memory_map::MemoryType::Runtime;

    switch (descriptor.type) {
        case EfiLoaderCode:
        case EfiLoaderData:
            return memory_map::MemoryType::ReclaimableBootloader;

        case EfiBootServicesCode:
        case EfiBootServicesData:
            return memory_map::MemoryType::ReclaimableBootServices;

        case EfiConventionalMemory:
            return memory_map::MemoryType::Usable;

        case EfiACPIReclaimMemory:
            return memory_map::MemoryType::ReclaimableACPI;

        case EfiMemoryMappedIO:
        case EfiMemoryMappedIOPortSpace:
            return memory_map::MemoryType::MMIO;

        case EfiUnusableMemory:
            return memory_map::MemoryType::Unusable;

        case EfiPersistentMemory:
            return memory_map::MemoryType::Persistent;

        case EfiUnacceptedMemoryType:
            return memory_map::MemoryType::Unaccepted;

        default:
            return memory_map::MemoryType::Reserved;
    }
}

void parse(const MemoryMapInfo &raw) {
    // Compact the raw map in place. Copy each descriptor before its slot is overwritten.
    // 32-byte output regions never reach unread input descriptors (40+ bytes per).
    regions = reinterpret_cast<memory_map::MemoryRegion *>(raw.descriptors);
    region_count = 0;

    const uint8_t *base = reinterpret_cast<const uint8_t *>(raw.descriptors);

    for (uint64_t i = 0; i < raw.byte_size; i += raw.descriptor_size) {
        EfiMemoryDescriptor descriptor = *reinterpret_cast<const EfiMemoryDescriptor *>(base + i);

        auto &region = regions[region_count++];
        region.start = descriptor.physical_start;
        region.end_exclusive = descriptor.physical_start + descriptor.number_of_pages * page::SIZE;
        region.attributes = descriptor.attributes;
        region.type = convert_type(descriptor);
    }
}

void sort() {
    for (size_t i = 1; i < region_count; i++) {
        auto curr = regions[i];
        size_t j = i;

        while (j > 0 && regions[j - 1].start > curr.start) {
            regions[j] = regions[j - 1];
            j--;
        }

        regions[j] = curr;
    }
}

bool check_overlaps() {
    for (size_t i = 1; i < region_count; i++) {
        if (regions[i].start < regions[i - 1].end_exclusive) {
            return false;
        }
    }
    return true;
}

void merge() {
    if (region_count < 2) return;

    size_t write_index = 0;
    for (size_t i = 1; i < region_count; i++) {
        auto &a = regions[write_index];
        auto &b = regions[i];

        if (a.end_exclusive == b.start && a.type == b.type && a.attributes == b.attributes) {
            a.end_exclusive = b.end_exclusive;
        } else {
            regions[++write_index] = b;
        }
    }
    region_count = write_index + 1;
}

} // namespace

namespace skern::memory_map {

bool normalize(const MemoryMapInfo &raw) {
    if (!validate(raw)) return false;
    parse(raw);
    sort();

    if (check_overlaps()) {
        merge();
        return true;
    }

    regions = nullptr;
    region_count = 0;
    return false;
}

const MemoryRegion *get_regions() {
    return regions;
}

size_t num_regions() {
    return region_count;
}

} // namespace skern::memory_map
