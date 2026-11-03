#include "heap.hpp"

#include "../panic.hpp"
#include "pmm.hpp"
#include "vmm.hpp"

#include <skern/memory_layout.h>
#include <skern/paging.h>

#include <cstddef>
#include <cstdint>

namespace {

using namespace skern;

struct HeapBlock {
    uint8_t flags;
    size_t size;
    HeapBlock *prev, *next;
};

enum : uint8_t {
    ALLOCATED = 1 << 0,

    HEAP_ALIGNMENT = 16,
};

uint64_t heap_end;
HeapBlock *heap_head;
HeapBlock *heap_tail;

HeapBlock *find_free_block(size_t bytes) {
    for (HeapBlock *b = heap_head; b != nullptr; b = b->next) {
        if ((b->flags & ALLOCATED) == 0 && b->size >= bytes) return b;
    }
    return nullptr;
}

void split_block(HeapBlock *block, size_t bytes) {
    if (bytes >= block->size) return;
    if (block->size - bytes < sizeof(HeapBlock) + HEAP_ALIGNMENT) return;

    auto *payload = reinterpret_cast<uint8_t *>(block + 1);
    auto *new_block = reinterpret_cast<HeapBlock *>(payload + bytes);

    new_block->flags = 0;
    new_block->size = block->size - bytes - sizeof(HeapBlock);
    new_block->prev = block;
    new_block->next = block->next;

    if (block->next) block->next->prev = new_block;
    block->next = new_block;
    block->size = bytes;
    if (heap_tail == block) heap_tail = new_block;
}

void join_blocks() {
    if (heap_head == nullptr) return;
    HeapBlock *prev = heap_head;
    HeapBlock *curr = heap_head->next;

    while (curr != nullptr) {
        if (!(prev->flags & ALLOCATED) && !(curr->flags & ALLOCATED)) {
            prev->size += curr->size + sizeof(HeapBlock);
            prev->next = curr->next;
            if (prev->next != nullptr) {
                prev->next->prev = prev;
            } else {
                heap_tail = prev;
            }
            curr = prev->next;
            continue;
        }
        prev = curr;
        curr = curr->next;
    }
}

void grow_heap() {
    if (heap_end >= HEAP_LIMIT) panic("Heap: out of memory");

    uint64_t phys_addr;
    if (!pmm::allocate_frame(phys_addr)) panic("Heap: failed to allocate new frame");
    if (!vmm::map(heap_end, phys_addr, vmm::WRITABLE | vmm::NO_EXEC)) {
        pmm::free_frame(phys_addr);
        panic("Heap: failed to map new frame");
    }

    auto *new_block = reinterpret_cast<HeapBlock *>(heap_end);
    new_block->flags = 0;
    new_block->size = SKERN_PAGE_SIZE - sizeof(HeapBlock);

    new_block->prev = heap_tail;
    new_block->prev->next = new_block;
    new_block->next = nullptr;

    heap_tail = new_block;
    heap_end += static_cast<uint64_t>(SKERN_PAGE_SIZE);
}

} // namespace

namespace skern::heap {

void init() {
    uint64_t phys_addr;
    if (!pmm::allocate_frame(phys_addr)) panic("Heap: failed to allocate initial frame");
    if (!vmm::map(HEAP_BASE, phys_addr, vmm::WRITABLE | vmm::NO_EXEC)) {
        pmm::free_frame(phys_addr);
        panic("Heap: failed to map initial frame");
    }

    auto *head = reinterpret_cast<HeapBlock *>(HEAP_BASE);
    head->flags = 0;
    head->size = SKERN_PAGE_SIZE - sizeof(HeapBlock);
    head->prev = nullptr;
    head->next = nullptr;

    heap_head = head;
    heap_tail = head;

    heap_end = HEAP_BASE + static_cast<uint64_t>(SKERN_PAGE_SIZE);
}

void *request(size_t bytes) {
    if (bytes == 0) return nullptr;
    if (bytes > SIZE_MAX - (HEAP_ALIGNMENT - 1)) return nullptr;

    bytes = (bytes + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);

    while (true) {
        if (HeapBlock *block = find_free_block(bytes)) {
            split_block(block, bytes);
            block->flags |= ALLOCATED;
            return block + 1;
        }
        grow_heap();
        join_blocks();
    }
}

void release(void *ptr) {
    if (ptr == nullptr) return;
    auto *block = static_cast<HeapBlock *>(ptr) - 1;

    if (!(block->flags & ALLOCATED)) panic("Heap: invalid or double release");
    block->flags &= ~ALLOCATED;
    join_blocks();
}

} // namespace skern::heap
