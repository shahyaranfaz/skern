#include "vmm.hpp"

#include "page.hpp"
#include "pmm.hpp"

#include <cstdint>

namespace {

struct alignas(4096) PageTableBase {
    uint64_t entries[512];

    void clear() {
        for (uint64_t &entry : entries) {
            entry = 0;
        }
    }
};

struct PML4 : PageTableBase {};
struct PDPT : PageTableBase {};
struct PageDirectory : PageTableBase {};
struct PageTable : PageTableBase {};

static_assert(sizeof(PageTableBase) == 4096);
static_assert(sizeof(PML4) == 4096);
static_assert(alignof(PML4) == 4096);
static_assert(sizeof(PDPT) == 4096);
static_assert(alignof(PDPT) == 4096);
static_assert(sizeof(PageDirectory) == 4096);
static_assert(alignof(PageDirectory) == 4096);
static_assert(sizeof(PageTable) == 4096);
static_assert(alignof(PageTable) == 4096);

// clang-format off
enum : uint64_t {
    PRESENT            = 1ULL << 0,
    PWT                = 1ULL << 3,
    PCD                = 1ULL << 4,
    ACCESSED           = 1ULL << 5,
    HUGE               = 1ULL << 7,

    ENTRY_ADDRESS_MASK = 0x000FFFFFFFFFF000ULL,
    CHILD_TABLE_FLAGS  = PRESENT | skern::vmm::WRITABLE,
};
// clang-format on

uint64_t root_phys_addr;
PML4 *root;

uint16_t pml4_index(uint64_t virtual_addr) {
    return (virtual_addr >> 39) & 0x1FF;
}

uint16_t pdpt_index(uint64_t virtual_addr) {
    return (virtual_addr >> 30) & 0x1FF;
}

uint16_t pd_index(uint64_t virtual_addr) {
    return (virtual_addr >> 21) & 0x1FF;
}

uint16_t pt_index(uint64_t virtual_addr) {
    return (virtual_addr >> 12) & 0x1FF;
}

bool is_canonical(uint64_t virtual_addr) {
    uint64_t upper = virtual_addr >> 48;
    return upper == ((virtual_addr & (1ULL << 47)) != 0 ? 0xFFFF : 0);
}

PageTableBase *allocate_table(uint64_t &phys_addr) {
    if (!skern::pmm::allocate_frame(phys_addr)) return nullptr;

    auto *table = reinterpret_cast<PageTableBase *>(phys_addr);
    table->clear();
    return table;
}

PageTableBase *get_child(uint64_t &entry) {
    if (entry & PRESENT) {
        uint64_t phys_addr = entry & ENTRY_ADDRESS_MASK;
        return reinterpret_cast<PageTableBase *>(phys_addr);
    }
    return nullptr;
}

PageTableBase *get_or_create_child(uint64_t &entry) {
    PageTableBase *table = get_child(entry);
    if (table != nullptr) return table;

    uint64_t phys_addr;
    table = allocate_table(phys_addr);
    if (table == nullptr) return nullptr;

    entry = phys_addr | CHILD_TABLE_FLAGS;
    return table;
}

} // namespace

namespace skern::vmm {

bool init() {
    root = static_cast<PML4 *>(allocate_table(root_phys_addr));
    return root != nullptr;
}

bool map(uint64_t virtual_addr, uint64_t phys_addr, uint64_t flags) {
    if (!is_canonical(virtual_addr)) return false;
    if (virtual_addr % page::SIZE != 0) return false;
    if (phys_addr % page::SIZE != 0) return false;
    if ((phys_addr & ~ENTRY_ADDRESS_MASK) != 0) return false;
    if (flags & ~(WRITABLE | NO_EXEC)) return false;

    PageTableBase *pdpt = get_or_create_child(root->entries[pml4_index(virtual_addr)]);
    if (!pdpt) return false;

    PageTableBase *pd = get_or_create_child(pdpt->entries[pdpt_index(virtual_addr)]);
    if (!pd) return false;

    PageTableBase *pt = get_or_create_child(pd->entries[pd_index(virtual_addr)]);
    if (!pt) return false;

    uint64_t &entry = pt->entries[pt_index(virtual_addr)];
    if (entry & PRESENT) return false;

    entry = phys_addr | PRESENT | flags;
    return true;
}

bool unmap(uint64_t virtual_addr) {
    if (!is_canonical(virtual_addr)) return false;
    if (virtual_addr % page::SIZE != 0) return false;

    PageTableBase *pdpt = get_child(root->entries[pml4_index(virtual_addr)]);
    if (!pdpt) return false;

    PageTableBase *pd = get_child(pdpt->entries[pdpt_index(virtual_addr)]);
    if (!pd) return false;

    PageTableBase *pt = get_child(pd->entries[pd_index(virtual_addr)]);
    if (!pt) return false;

    uint64_t &entry = pt->entries[pt_index(virtual_addr)];
    if (!(entry & PRESENT)) return false;

    entry = 0;
    asm volatile("invlpg (%0)" : : "r"(virtual_addr) : "memory");
    return true;
}

} // namespace skern::vmm
