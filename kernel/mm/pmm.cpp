#include "pmm.hpp"

#include "../panic.hpp"
#include "memory_map.hpp"
#include "page.hpp"

#include <cstddef>
#include <cstdint>

namespace {

using namespace skern;

uint8_t *bitmap;
uint64_t frame_count;
uint64_t free_frame_count;
uint64_t next_free_frame;

enum : uint64_t {
    NO_FREE_FRAMES = UINT64_MAX,
};

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

} // namespace

namespace skern::pmm {

void init() {
    size_t region_count = memory_map::num_regions();
    const memory_map::MemoryRegion *regions = memory_map::get_regions();
    if (regions == nullptr) panic("PMM: no memory map");
    if (region_count == 0) panic("PMM: empty memory map");

    frame_count = highest_address(regions, region_count) / page::SIZE;
    if (frame_count == 0) panic("PMM: no manageable RAM");

    uint64_t bitmap_bytes = frame_count / 8 + (frame_count % 8 != 0);
    uint64_t bitmap_frames = bitmap_bytes / page::SIZE + (bitmap_bytes % page::SIZE != 0);
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

    // mark usable frames as free
    free_frame_count = 0;
    for (size_t i = 0; i < region_count; i++) {
        if (regions[i].type != memory_map::MemoryType::Usable) continue;

        uint64_t first_frame = regions[i].start / page::SIZE;
        uint64_t end_frame = regions[i].end_exclusive / page::SIZE;
        for (uint64_t frame = first_frame; frame < end_frame; frame++) {
            mark_free(frame);
            free_frame_count++;
        }
    }

    // reserve the bitmap's own frames
    uint64_t bitmap_first_frame = bitmap_address / page::SIZE;
    for (uint64_t frame = bitmap_first_frame; frame < bitmap_first_frame + bitmap_frames; frame++) {
        if (is_allocated(frame)) continue;
        mark_allocated(frame);
        free_frame_count--;
    }

    // initialize next_free_frame with the earliest free frame
    next_free_frame = NO_FREE_FRAMES;
    for (uint64_t frame = 0; frame < frame_count; frame++) {
        if (!is_allocated(frame)) {
            next_free_frame = frame;
            break;
        }
    }
}

bool allocate_frame(uint64_t &phys_addr) {
    if (free_frame_count == 0) return false;

    phys_addr = next_free_frame * page::SIZE;
    mark_allocated(next_free_frame);
    free_frame_count--;

    if (free_frame_count == 0) {
        next_free_frame = NO_FREE_FRAMES;
    } else {
        for (uint64_t step = 1; step < frame_count; ++step) {
            uint64_t i = (next_free_frame + step) % frame_count;
            if (!is_allocated(i)) {
                next_free_frame = i;
                break;
            }
        }
    }
    return true;
}

void free_frame(uint64_t phys_addr) {
    if (phys_addr % page::SIZE != 0) panic("PMM: freeing frame not aligned");

    uint64_t frame = phys_addr / page::SIZE;
    if (frame >= frame_count) panic("PMM: freeing frame out of range");
    if (!is_allocated(frame)) panic("PMM: double free");

    mark_free(frame);
    free_frame_count++;
    if (next_free_frame > frame) next_free_frame = frame;
}

} // namespace skern::pmm
