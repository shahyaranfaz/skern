#include "pmm.hpp"

#include "../panic.hpp"
#include "memory_map.hpp"
#include "page.hpp"

#include <cstddef>
#include <cstdint>

namespace {

using namespace skern;

struct ProtectedRange {
    uint64_t start;
    uint64_t end_exclusive;
};

enum : size_t {
    MAX_PROTECTED_RANGES = 16,
};

enum : uint64_t {
    NO_FREE_FRAMES = UINT64_MAX,
};

uint8_t *bitmap;
uint64_t frame_count;
uint64_t free_frame_count;
uint64_t next_free_frame;

ProtectedRange protected_ranges[MAX_PROTECTED_RANGES];
size_t protected_range_count;

bool is_allocated(uint64_t frame) {
    return (bitmap[frame / 8] & (1U << (frame % 8))) != 0;
}

void mark_allocated(uint64_t frame) {
    bitmap[frame / 8] |= 1U << (frame % 8);
}

void mark_free(uint64_t frame) {
    bitmap[frame / 8] &= ~(1U << (frame % 8));
}

bool is_managed_ram(memory_map::MemoryType type) {
    switch (type) {
        case memory_map::MemoryType::Usable:
        case memory_map::MemoryType::ReclaimableBootloader:
        case memory_map::MemoryType::ReclaimableBootServices:
        case memory_map::MemoryType::ReclaimableACPI:
            return true;
        default:
            return false;
    }
}

uint64_t highest_address(const memory_map::MemoryRegion *regions, size_t count) {
    uint64_t highest_addr = 0;
    for (size_t i = 0; i < count; i++) {
        if (!is_managed_ram(regions[i].type)) continue;
        if (regions[i].end_exclusive > highest_addr) highest_addr = regions[i].end_exclusive;
    }
    return highest_addr;
}

uint64_t find_free_frame(uint64_t start) {
    for (uint64_t step = 0; step < frame_count; step++) {
        uint64_t frame = (start + step) % frame_count;
        if (!is_allocated(frame)) {
            return frame;
        }
    }
    return NO_FREE_FRAMES;
}

uint64_t divide_round_up(uint64_t value, uint64_t divisor) {
    return value / divisor + (value % divisor != 0);
}

bool is_protected(uint64_t frame) {
    uint64_t address = frame * page::SIZE;
    for (size_t i = 0; i < protected_range_count; i++) {
        if (address >= protected_ranges[i].start && address < protected_ranges[i].end_exclusive) return true;
    }
    return false;
}

} // namespace

namespace skern::pmm {

void init() {
    size_t region_count = memory_map::num_regions();
    const memory_map::MemoryRegion *regions = memory_map::get_regions();
    if (regions == nullptr) panic("PMM: no memory map");
    if (region_count == 0) panic("PMM: empty memory map");

    frame_count = highest_address(regions, region_count) / page::SIZE;
    if (frame_count == 0) panic("PMM: no manageable RAM");

    uint64_t bitmap_bytes = divide_round_up(frame_count, 8);
    uint64_t bitmap_frames = divide_round_up(bitmap_bytes, page::SIZE);
    uint64_t bitmap_size = bitmap_frames * page::SIZE;

    // find a usable region big enough for bitmap
    uint64_t bitmap_address = UINT64_MAX;
    for (size_t i = 0; i < region_count; i++) {
        if (regions[i].type != memory_map::MemoryType::Usable) continue;
        if (regions[i].end_exclusive - regions[i].start < bitmap_size) continue;

        bitmap_address = regions[i].start;
        break;
    }
    if (bitmap_address == UINT64_MAX) panic("PMM: no space for bitmap");

    // start with every frame reserved
    bitmap = reinterpret_cast<uint8_t *>(bitmap_address);
    for (uint64_t i = 0; i < bitmap_bytes; i++) {
        bitmap[i] = 0xFF;
    }

    // start with no protected regions
    protected_range_count = 0;

    // mark usable frames as free
    free_frame_count = 0;
    for (size_t i = 0; i < region_count; i++) {
        if (regions[i].type != memory_map::MemoryType::Usable) continue;
        release_range(regions[i].start, regions[i].end_exclusive);
    }

    // reserve the bitmap's own frames
    reserve_range(bitmap_address, bitmap_address + bitmap_size);

    // initialize next_free_frame with the earliest free frame
    next_free_frame = find_free_frame(0);
}

bool allocate_frame(uint64_t &phys_addr) {
    if (free_frame_count == 0) return false;

    phys_addr = next_free_frame * page::SIZE;
    mark_allocated(next_free_frame);
    free_frame_count--;

    if (free_frame_count == 0) {
        next_free_frame = NO_FREE_FRAMES;
    } else {
        next_free_frame = find_free_frame((next_free_frame + 1) % frame_count);
    }
    return true;
}

void free_frame(uint64_t phys_addr) {
    if (phys_addr % page::SIZE != 0) panic("PMM: freeing frame not aligned");

    uint64_t frame = phys_addr / page::SIZE;
    if (frame >= frame_count) panic("PMM: freeing frame out of range");
    if (is_protected(frame)) panic("PMM: freeing protected frame");
    if (!is_allocated(frame)) panic("PMM: double free");

    mark_free(frame);
    free_frame_count++;
    if (next_free_frame > frame) next_free_frame = frame;
}

void reserve_range(uint64_t start, uint64_t end_exclusive) {
    if (start >= end_exclusive) panic("PMM: invalid reserve range");
    uint64_t first_frame = start / page::SIZE;
    uint64_t end_frame = divide_round_up(end_exclusive, page::SIZE);

    if (first_frame >= frame_count || end_frame > frame_count) panic("PMM: reserve range out of bounds");

    // mark as reserved
    if (protected_range_count >= MAX_PROTECTED_RANGES) panic("PMM: too many protected ranges");
    protected_ranges[protected_range_count].start = first_frame * page::SIZE;
    protected_ranges[protected_range_count].end_exclusive = end_frame * page::SIZE;
    protected_range_count++;

    for (uint64_t frame = first_frame; frame < end_frame; frame++) {
        if (is_allocated(frame)) continue;
        mark_allocated(frame);
        free_frame_count--;
    }

    if (first_frame <= next_free_frame && next_free_frame < end_frame) {
        next_free_frame = find_free_frame(next_free_frame);
    }
}

void release_range(uint64_t start, uint64_t end_exclusive) {
    if (start >= end_exclusive) panic("PMM: invalid release range");
    if (start % page::SIZE != 0 || end_exclusive % page::SIZE != 0) panic("PMM: releasing unaligned range");

    uint64_t first_frame = start / page::SIZE;
    uint64_t end_frame = end_exclusive / page::SIZE;

    if (first_frame >= frame_count || end_frame > frame_count) panic("PMM: release range out of bounds");

    uint64_t first_released = NO_FREE_FRAMES;
    for (uint64_t frame = first_frame; frame < end_frame; frame++) {
        if (is_protected(frame)) continue;
        if (!is_allocated(frame)) continue;
        mark_free(frame);
        free_frame_count++;
        if (first_released == NO_FREE_FRAMES) first_released = frame;
    }
    if (first_released == NO_FREE_FRAMES) return;

    if (next_free_frame == NO_FREE_FRAMES || first_released < next_free_frame) next_free_frame = first_released;
}

} // namespace skern::pmm
