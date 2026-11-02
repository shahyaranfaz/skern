#include "vmm.hpp"

#include "pmm.hpp"

#include <skern/memory_layout.h>

#include <cstdint>

namespace {
// clang-format off
enum : uint64_t {
    CHILD_TABLE_FLAGS = SKERN_PAGE_PRESENT | SKERN_PAGE_WRITABLE,
};
// clang-format on

uint64_t root_phys_addr;
SkernPageTable *root;

SkernPageTable *table_from_phys(uint64_t phys_addr) {
    return reinterpret_cast<SkernPageTable *>(SKERN_HHDM_BASE + phys_addr);
}

SkernPageTable *allocate_table(uint64_t &phys_addr) {
    if (!skern::pmm::allocate_frame(phys_addr)) return nullptr;
    skern::pmm::protect_range(phys_addr, phys_addr + SKERN_PAGE_SIZE);

    SkernPageTable *table = table_from_phys(phys_addr);
    for (uint64_t &entry : table->entries) {
        entry = 0;
    }
    return table;
}

SkernPageTable *get_child(uint64_t &entry) {
    if (entry & SKERN_PAGE_PRESENT) {
        uint64_t phys_addr = entry & SKERN_PAGE_ENTRY_ADDRESS_MASK;
        return table_from_phys(phys_addr);
    }
    return nullptr;
}

SkernPageTable *get_or_create_child(uint64_t &entry) {
    SkernPageTable *table = get_child(entry);
    if (table != nullptr) return table;

    uint64_t phys_addr;
    table = allocate_table(phys_addr);
    if (table == nullptr) return nullptr;

    entry = phys_addr | CHILD_TABLE_FLAGS;
    return table;
}

void protect_table(uint64_t phys_addr, uint64_t level) {
    skern::pmm::protect_range(phys_addr, phys_addr + SKERN_PAGE_SIZE);

    if (level == 1) return;

    SkernPageTable *table = table_from_phys(phys_addr);
    for (uint64_t entry : table->entries) {
        if (!(entry & SKERN_PAGE_PRESENT)) continue;
        if (level != 4 && entry & SKERN_PAGE_HUGE) continue;
        protect_table(entry & SKERN_PAGE_ENTRY_ADDRESS_MASK, level - 1);
    }
}

void free_table(uint64_t phys_addr, uint64_t level) {
    SkernPageTable *table = table_from_phys(phys_addr);

    if (level > 1) {
        for (uint64_t entry : table->entries) {
            if (!(entry & SKERN_PAGE_PRESENT) || entry & SKERN_PAGE_HUGE) continue;

            uint64_t child_phys = entry & SKERN_PAGE_ENTRY_ADDRESS_MASK;
            free_table(child_phys, level - 1);
        }
    }

    skern::pmm::unprotect_range(phys_addr, phys_addr + SKERN_PAGE_SIZE);
    skern::pmm::release_range(phys_addr, phys_addr + SKERN_PAGE_SIZE);
}

void remove_identity_mappings() {
    for (uint64_t i = 0; i < 256; i++) {
        if (!(root->entries[i] & SKERN_PAGE_PRESENT)) continue;
        uint64_t entry = root->entries[i];
        root->entries[i] = 0;

        uint64_t child_phys = entry & SKERN_PAGE_ENTRY_ADDRESS_MASK;
        free_table(child_phys, 3);
    }

    asm volatile("mov %0, %%cr3" : : "r"(root_phys_addr) : "memory");
}

} // namespace

namespace skern::vmm {

bool init() {
    asm volatile("mov %%cr3, %0" : "=r"(root_phys_addr));
    root_phys_addr &= SKERN_PAGE_ENTRY_ADDRESS_MASK;

    root = table_from_phys(root_phys_addr);
    protect_table(root_phys_addr, 4);
    remove_identity_mappings();
    return root != nullptr;
}

bool map(uint64_t virtual_addr, uint64_t phys_addr, uint64_t flags) {
    if (!skern_is_canonical_address(virtual_addr)) return false;
    if (virtual_addr % SKERN_PAGE_SIZE != 0) return false;
    if (phys_addr % SKERN_PAGE_SIZE != 0) return false;
    if ((phys_addr & ~SKERN_PAGE_ENTRY_ADDRESS_MASK) != 0) return false;
    if (flags & ~(WRITABLE | NO_EXEC)) return false;

    SkernPageTable *pdpt = get_or_create_child(root->entries[skern_pml4_index(virtual_addr)]);
    if (!pdpt) return false;

    SkernPageTable *pd = get_or_create_child(pdpt->entries[skern_pdpt_index(virtual_addr)]);
    if (!pd) return false;

    SkernPageTable *pt = get_or_create_child(pd->entries[skern_pd_index(virtual_addr)]);
    if (!pt) return false;

    uint64_t &entry = pt->entries[skern_pt_index(virtual_addr)];
    if (entry & SKERN_PAGE_PRESENT) return false;

    entry = phys_addr | SKERN_PAGE_PRESENT | flags;
    return true;
}

bool unmap(uint64_t virtual_addr) {
    if (!skern_is_canonical_address(virtual_addr)) return false;
    if (virtual_addr % SKERN_PAGE_SIZE != 0) return false;

    SkernPageTable *pdpt = get_child(root->entries[skern_pml4_index(virtual_addr)]);
    if (!pdpt) return false;

    SkernPageTable *pd = get_child(pdpt->entries[skern_pdpt_index(virtual_addr)]);
    if (!pd) return false;

    SkernPageTable *pt = get_child(pd->entries[skern_pd_index(virtual_addr)]);
    if (!pt) return false;

    uint64_t &entry = pt->entries[skern_pt_index(virtual_addr)];
    if (!(entry & SKERN_PAGE_PRESENT)) return false;

    entry = 0;
    asm volatile("invlpg (%0)" : : "r"(virtual_addr) : "memory");
    return true;
}

} // namespace skern::vmm
