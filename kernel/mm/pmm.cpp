#include "pmm.hpp"

#include "../panic.hpp"
#include "bitmap.hpp"
#include "memory_map.hpp"

#include <skern/memory_layout.h>
#include <skern/paging.h>

#include <cstddef>
#include <cstdint>

namespace {

using namespace skern;

enum : uint64_t {
    NO_FREE_FRAMES = UINT64_MAX,
};

uint64_t frame_count;
uint64_t free_frame_count;
uint64_t next_free_frame;

Bitmap allocations;
Bitmap protections;

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
        if (!allocations.get(frame)) {
            return frame;
        }
    }
    return NO_FREE_FRAMES;
}

uint64_t divide_round_up(uint64_t value, uint64_t divisor) {
    return value / divisor + (value % divisor != 0);
}

} // namespace

namespace skern::pmm {

void init() {
    size_t region_count = memory_map::num_regions();
    const memory_map::MemoryRegion *regions = memory_map::get_regions();
    if (regions == nullptr) panic("PMM: no memory map");
    if (region_count == 0) panic("PMM: empty memory map");

    frame_count = highest_address(regions, region_count) / SKERN_PAGE_SIZE;
    if (frame_count == 0) panic("PMM: no manageable RAM");

    uint64_t bitmap_bytes = divide_round_up(frame_count, 8);
    if (bitmap_bytes > UINT64_MAX / 2) panic("PMM: bitmap size overflow");
    uint64_t bitmap_storage_bytes = bitmap_bytes * 2;
    uint64_t bitmap_frames = divide_round_up(bitmap_storage_bytes, SKERN_PAGE_SIZE);
    uint64_t bitmap_size = bitmap_frames * SKERN_PAGE_SIZE;

    // find a usable region big enough for bitmap
    uint64_t bitmap_address = UINT64_MAX;
    for (size_t i = 0; i < region_count; i++) {
        if (regions[i].type != memory_map::MemoryType::Usable) continue;
        if (regions[i].end_exclusive - regions[i].start < bitmap_size) continue;

        bitmap_address = regions[i].start;
        break;
    }
    if (bitmap_address == UINT64_MAX) panic("PMM: no space for bitmap");

    uint8_t *bitmap_data = reinterpret_cast<uint8_t *>(SKERN_HHDM_BASE + bitmap_address);
    allocations = {bitmap_data, frame_count};
    protections = {bitmap_data + bitmap_bytes, frame_count};

    // start with every frame reserved
    for (uint64_t frame = 0; frame < frame_count; frame++) {
        allocations.set(frame);
    }
    protections.clear();

    // mark usable frames as free
    free_frame_count = 0;
    for (size_t i = 0; i < region_count; i++) {
        if (regions[i].type != memory_map::MemoryType::Usable) continue;
        release_range(regions[i].start, regions[i].end_exclusive);
    }

    // reserve the bitmap's own frames
    protect_range(bitmap_address, bitmap_address + bitmap_size);

    // initialize next_free_frame with the earliest free frame
    next_free_frame = find_free_frame(0);
}

bool allocate_frame(uint64_t &phys_addr) {
    if (free_frame_count == 0) return false;

    phys_addr = next_free_frame * SKERN_PAGE_SIZE;
    allocations.set(next_free_frame);
    free_frame_count--;

    if (free_frame_count == 0) {
        next_free_frame = NO_FREE_FRAMES;
    } else {
        next_free_frame = find_free_frame((next_free_frame + 1) % frame_count);
    }
    return true;
}

void free_frame(uint64_t phys_addr) {
    if (phys_addr % SKERN_PAGE_SIZE != 0) panic("PMM: freeing frame not aligned");

    uint64_t frame = phys_addr / SKERN_PAGE_SIZE;
    if (frame >= frame_count) panic("PMM: freeing frame out of range");
    if (protections.get(frame)) panic("PMM: freeing protected frame");
    if (!allocations.get(frame)) panic("PMM: double free");

    allocations.unset(frame);
    free_frame_count++;
    if (next_free_frame > frame) next_free_frame = frame;
}

void protect_range(uint64_t start, uint64_t end_exclusive) {
    if (start >= end_exclusive) panic("PMM: invalid protect range");
    uint64_t first_frame = start / SKERN_PAGE_SIZE;
    uint64_t end_frame = divide_round_up(end_exclusive, SKERN_PAGE_SIZE);

    if (first_frame >= frame_count || end_frame > frame_count) panic("PMM: protect range out of bounds");

    for (uint64_t frame = first_frame; frame < end_frame; frame++) {
        protections.set(frame);
        if (allocations.get(frame)) continue;
        allocations.set(frame);
        free_frame_count--;
    }

    if (first_frame <= next_free_frame && next_free_frame < end_frame) {
        next_free_frame = find_free_frame(next_free_frame);
    }
}

void unprotect_range(uint64_t start, uint64_t end_exclusive) {
    if (start >= end_exclusive) panic("PMM: invalid unprotect range");
    uint64_t first_frame = start / SKERN_PAGE_SIZE;
    uint64_t end_frame = divide_round_up(end_exclusive, SKERN_PAGE_SIZE);

    if (first_frame >= frame_count || end_frame > frame_count) panic("PMM: unprotect range out of bounds");

    for (uint64_t frame = first_frame; frame < end_frame; frame++) {
        protections.unset(frame);
    }
}

void release_range(uint64_t start, uint64_t end_exclusive) {
    if (start >= end_exclusive) panic("PMM: invalid release range");
    if (start % SKERN_PAGE_SIZE != 0 || end_exclusive % SKERN_PAGE_SIZE != 0) panic("PMM: releasing unaligned range");

    uint64_t first_frame = start / SKERN_PAGE_SIZE;
    uint64_t end_frame = end_exclusive / SKERN_PAGE_SIZE;

    if (first_frame >= frame_count || end_frame > frame_count) panic("PMM: release range out of bounds");

    uint64_t first_released = NO_FREE_FRAMES;
    for (uint64_t frame = first_frame; frame < end_frame; frame++) {
        if (protections.get(frame)) continue;
        if (!allocations.get(frame)) continue;
        allocations.unset(frame);
        free_frame_count++;
        if (first_released == NO_FREE_FRAMES) first_released = frame;
    }
    if (first_released == NO_FREE_FRAMES) return;

    if (next_free_frame == NO_FREE_FRAMES || first_released < next_free_frame) next_free_frame = first_released;
}

} // namespace skern::pmm
