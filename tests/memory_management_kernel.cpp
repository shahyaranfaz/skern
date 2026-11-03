#include "cpu/gdt.hpp"
#include "cpu/idt.hpp"
#include "cpu/tss.hpp"
#include "drivers/serial.hpp"
#include "mm/heap.hpp"
#include "mm/memory_map.hpp"
#include "mm/pmm.hpp"
#include "mm/vmm.hpp"
#include "panic.hpp"

#include <skern/boot_info.h>
#include <skern/memory_layout.h>
#include <skern/paging.h>

#include <cstddef>
#include <cstdint>

extern "C" uint8_t __kernel_phys_start[];
extern "C" uint8_t __kernel_phys_end[];

namespace {

using namespace skern;

void fail(const char *message) {
    serial::write("HEAP TEST FAILED: ");
    serial::write(message);
    serial::write("\n");
    while (true)
        asm volatile("cli; hlt");
}

void initialize_memory(const BootInfo *boot_info) {
    uint64_t boot_info_start = reinterpret_cast<uint64_t>(boot_info);
    boot_info = reinterpret_cast<const BootInfo *>(SKERN_HHDM_BASE + boot_info_start);

    if (boot_info->magic != SKERN_BOOT_INFO_MAGIC || boot_info->version != SKERN_BOOT_INFO_VERSION ||
        boot_info->size != sizeof(BootInfo)) {
        fail("invalid boot info");
    }
    if (!memory_map::normalize(boot_info->memory_map)) fail("invalid memory map");

    pmm::init();
    pmm::protect_range(boot_info_start, boot_info_start + sizeof(BootInfo));
    pmm::protect_range(boot_info->bootstrap_stack.base,
                       boot_info->bootstrap_stack.base + boot_info->bootstrap_stack.size);
    pmm::protect_range(reinterpret_cast<uint64_t>(__kernel_phys_start),
                       reinterpret_cast<uint64_t>(__kernel_phys_end));
    pmm::protect_range(boot_info->memory_map.descriptors,
                       boot_info->memory_map.descriptors + boot_info->memory_map.byte_size);

    if (!vmm::init()) fail("VMM initialization failed");

    auto &tss_state = tss::init();
    gdt::init(tss_state);
    idt::init();
    pmm::reclaim_bootloader_memory();
}

void test_pmm_allocation() {
    uint64_t first;
    uint64_t second;
    if (!pmm::allocate_frame(first) || !pmm::allocate_frame(second)) fail("PMM allocation failed");
    if (first == second || first % SKERN_PAGE_SIZE != 0 || second % SKERN_PAGE_SIZE != 0) {
        fail("PMM returned invalid frames");
    }

    pmm::free_frame(first);
    uint64_t reused;
    if (!pmm::allocate_frame(reused) || reused != first) fail("PMM did not reuse a released frame");
    pmm::free_frame(reused);
    pmm::free_frame(second);
    serial::write("PMM ALLOCATION PASS\n");
}

uint64_t page_entry(uint64_t virtual_addr) {
    uint64_t root_phys;
    asm volatile("mov %%cr3, %0" : "=r"(root_phys));
    root_phys &= SKERN_PAGE_ENTRY_ADDRESS_MASK;

    auto *pml4 = reinterpret_cast<SkernPageTable *>(SKERN_HHDM_BASE + root_phys);
    uint64_t pml4e = pml4->entries[skern_pml4_index(virtual_addr)];
    if (!(pml4e & SKERN_PAGE_PRESENT)) return 0;

    auto *pdpt = reinterpret_cast<SkernPageTable *>(SKERN_HHDM_BASE + (pml4e & SKERN_PAGE_ENTRY_ADDRESS_MASK));
    uint64_t pdpte = pdpt->entries[skern_pdpt_index(virtual_addr)];
    if (!(pdpte & SKERN_PAGE_PRESENT)) return 0;

    auto *pd = reinterpret_cast<SkernPageTable *>(SKERN_HHDM_BASE + (pdpte & SKERN_PAGE_ENTRY_ADDRESS_MASK));
    uint64_t pde = pd->entries[skern_pd_index(virtual_addr)];
    if (!(pde & SKERN_PAGE_PRESENT)) return 0;

    auto *pt = reinterpret_cast<SkernPageTable *>(SKERN_HHDM_BASE + (pde & SKERN_PAGE_ENTRY_ADDRESS_MASK));
    return pt->entries[skern_pt_index(virtual_addr)];
}

void test_vmm_mapping() {
    enum : uint64_t {
        TEST_VIRTUAL_ADDRESS = HEAP_LIMIT,
    };

    uint64_t first_phys;
    uint64_t second_phys;
    if (!pmm::allocate_frame(first_phys) || !pmm::allocate_frame(second_phys)) fail("VMM frame allocation failed");

    *reinterpret_cast<uint64_t *>(SKERN_HHDM_BASE + first_phys) = UINT64_C(0x1111111111111111);
    *reinterpret_cast<uint64_t *>(SKERN_HHDM_BASE + second_phys) = UINT64_C(0x2222222222222222);

    if (!vmm::map(TEST_VIRTUAL_ADDRESS, first_phys, vmm::WRITABLE | vmm::NO_EXEC)) fail("VMM map failed");
    uint64_t entry = page_entry(TEST_VIRTUAL_ADDRESS);
    if (!(entry & SKERN_PAGE_PRESENT) || !(entry & SKERN_PAGE_WRITABLE) || !(entry & SKERN_PAGE_NO_EXEC) ||
        entry & SKERN_PAGE_USER) {
        fail("VMM installed incorrect permissions");
    }
    if (*reinterpret_cast<uint64_t *>(TEST_VIRTUAL_ADDRESS) != UINT64_C(0x1111111111111111)) {
        fail("VMM mapped the wrong frame");
    }

    if (!vmm::unmap(TEST_VIRTUAL_ADDRESS)) fail("VMM unmap failed");
    if (!vmm::map(TEST_VIRTUAL_ADDRESS, second_phys, vmm::WRITABLE)) fail("VMM remap failed");
    if (*reinterpret_cast<uint64_t *>(TEST_VIRTUAL_ADDRESS) != UINT64_C(0x2222222222222222)) {
        fail("VMM remap retained a stale TLB entry");
    }
    if (page_entry(TEST_VIRTUAL_ADDRESS) & SKERN_PAGE_NO_EXEC) fail("VMM retained stale permissions");
    if (!vmm::unmap(TEST_VIRTUAL_ADDRESS) || vmm::unmap(TEST_VIRTUAL_ADDRESS)) fail("VMM unmap state is invalid");

    pmm::free_frame(first_phys);
    pmm::free_frame(second_phys);
    serial::write("VMM MAPPING PASS\n");
    serial::write("VMM TLB PASS\n");
}

void test_zero_size() {
    if (heap::request(0) != nullptr) fail("zero-size request returned memory");
    serial::write("HEAP ZERO PASS\n");
}

void test_multi_page_allocation() {
    auto *allocation = static_cast<uint8_t *>(heap::request(SKERN_PAGE_SIZE * 2));
    if (allocation == nullptr) fail("multi-page allocation failed");
    if (reinterpret_cast<uintptr_t>(allocation) % 16 != 0) fail("multi-page allocation is misaligned");

    allocation[0] = 0x11;
    allocation[SKERN_PAGE_SIZE - 1] = 0x22;
    allocation[SKERN_PAGE_SIZE] = 0x33;
    allocation[SKERN_PAGE_SIZE * 2 - 1] = 0x44;
    if (allocation[0] != 0x11 || allocation[SKERN_PAGE_SIZE - 1] != 0x22 ||
        allocation[SKERN_PAGE_SIZE] != 0x33 || allocation[SKERN_PAGE_SIZE * 2 - 1] != 0x44) {
        fail("multi-page allocation did not retain data");
    }
    serial::write("HEAP GROWTH PASS\n");
}

void test_alignment_and_separation() {
    auto *first = static_cast<uint8_t *>(heap::request(1));
    auto *second = static_cast<uint8_t *>(heap::request(31));
    if (first == nullptr || second == nullptr) fail("small allocation failed");
    if (reinterpret_cast<uintptr_t>(first) % 16 != 0 || reinterpret_cast<uintptr_t>(second) % 16 != 0) {
        fail("small allocation is misaligned");
    }

    first[0] = 0x55;
    for (size_t i = 0; i < 31; i++) second[i] = 0xAA;
    if (first[0] != 0x55) fail("allocations overlap");
    serial::write("HEAP ALIGNMENT PASS\n");
}

void test_reuse() {
    void *original = heap::request(128);
    if (original == nullptr) fail("reuse setup allocation failed");
    heap::release(original);

    void *replacement = heap::request(64);
    if (replacement != original) fail("released block was not reused");
    heap::release(replacement);
    serial::write("HEAP REUSE PASS\n");
}

void test_coalescing() {
    void *left = heap::request(256);
    void *right = heap::request(256);
    void *guard = heap::request(64);
    if (left == nullptr || right == nullptr || guard == nullptr) fail("coalescing setup allocation failed");

    heap::release(left);
    heap::release(right);

    void *combined = heap::request(512);
    if (combined != left) fail("adjacent free blocks were not coalesced");
    serial::write("HEAP COALESCE PASS\n");
}

void test_pmm_protection_and_exhaustion() {
    uint64_t protected_frame;
    if (!pmm::allocate_frame(protected_frame)) fail("protected-range setup allocation failed");
    pmm::free_frame(protected_frame);
    pmm::protect_range(protected_frame, protected_frame + SKERN_PAGE_SIZE);

    uint64_t allocated_count = 0;
    uint64_t frame;
    while (pmm::allocate_frame(frame)) {
        if (frame == protected_frame) fail("PMM allocated a protected frame");
        allocated_count++;
    }
    if (allocated_count == 0 || pmm::allocate_frame(frame)) fail("PMM exhaustion was not stable");

    pmm::unprotect_range(protected_frame, protected_frame + SKERN_PAGE_SIZE);
    pmm::release_range(protected_frame, protected_frame + SKERN_PAGE_SIZE);
    if (!pmm::allocate_frame(frame) || frame != protected_frame) fail("released protected frame was not recovered");

    serial::write("PMM PROTECTION PASS\n");
    serial::write("PMM EXHAUSTION PASS\n");
}

} // namespace

extern "C" void kernel_main(const BootInfo *boot_info) {
    skern::serial::init();
    if (boot_info == nullptr) fail("missing boot info");

    initialize_memory(boot_info);
    test_pmm_allocation();
    test_vmm_mapping();
    heap::init();
    test_zero_size();
    test_multi_page_allocation();
    test_alignment_and_separation();
    test_reuse();
    test_coalescing();
    test_pmm_protection_and_exhaustion();
    skern::serial::write("HEAP TESTS PASSED\n");

    while (true)
        asm volatile("cli; hlt");
}
