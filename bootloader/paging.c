#include "paging.h"

#include <efilib.h>

enum : uint16_t {
    PAGE_SIZE = 4096,
};

typedef struct {
    alignas(PAGE_SIZE) uint64_t entries[512];
} PageTableNode;

static_assert(sizeof(PageTableNode) == PAGE_SIZE);
static_assert(alignof(PageTableNode) == PAGE_SIZE);

static void clear_ptn(PageTableNode *table) {
    for (int i = 0; i < 512; i++) {
        table->entries[i] = 0;
    }
}

// clang-format off
enum : uint64_t {
    PRESENT            = 1ULL << 0,
    WRITABLE           = 1ULL << 1,
    USER               = 1ULL << 2,
    PWT                = 1ULL << 3,
    PCD                = 1ULL << 4,
    ACCESSED           = 1ULL << 5,
    HUGE               = 1ULL << 7,
    NO_EXEC            = 1ULL << 63,
    ENTRY_ADDRESS_MASK = 0x000FFFFFFFFFF000ULL,
    CHILD_TABLE_FLAGS  = PRESENT | WRITABLE,
};
// clang-format on

static PageTableNode *root;

static uint16_t pml4_index(uint64_t virtual_addr) {
    return (virtual_addr >> 39) & 0x1FF;
}

static uint16_t pdpt_index(uint64_t virtual_addr) {
    return (virtual_addr >> 30) & 0x1FF;
}

static uint16_t pd_index(uint64_t virtual_addr) {
    return (virtual_addr >> 21) & 0x1FF;
}

static uint16_t pt_index(uint64_t virtual_addr) {
    return (virtual_addr >> 12) & 0x1FF;
}

static bool is_canonical(uint64_t virtual_addr) {
    uint64_t upper = virtual_addr >> 48;
    return upper == ((virtual_addr & (1ULL << 47)) != 0 ? 0xFFFF : 0);
}

static PageTableNode *allocate_table(uint64_t *phys_addr) {
    EFI_PHYSICAL_ADDRESS address = 0;
    EFI_STATUS status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, 1, &address);
    if (EFI_ERROR(status)) return NULL;

    PageTableNode *table = (PageTableNode *)address;
    clear_ptn(table);
    *phys_addr = address;
    return table;
}

static PageTableNode *get_child(uint64_t entry) {
    if (entry & PRESENT) {
        uint64_t phys_addr = entry & ENTRY_ADDRESS_MASK;
        return (PageTableNode *)phys_addr;
    }
    return NULL;
}

static PageTableNode *get_or_create_child(uint64_t *entry) {
    PageTableNode *table = get_child(*entry);
    if (table != NULL) return table;

    uint64_t phys_addr;
    table = allocate_table(&phys_addr);
    if (table == NULL) return NULL;

    *entry = phys_addr | CHILD_TABLE_FLAGS;
    return table;
}

EFI_STATUS paging_init(uint64_t *root_phys_addr) {
    if (root_phys_addr == NULL) return EFI_INVALID_PARAMETER;
    if (root != NULL) return EFI_ALREADY_STARTED;

    root = allocate_table(root_phys_addr);
    if (root == NULL) return EFI_OUT_OF_RESOURCES;
    return EFI_SUCCESS;
}

EFI_STATUS paging_map_page(uint64_t virtual_addr, uint64_t phys_addr, uint64_t flags) {
    if (root == NULL) return EFI_NOT_READY;
    if (!is_canonical(virtual_addr)) return EFI_INVALID_PARAMETER;
    if (virtual_addr % PAGE_SIZE != 0) return EFI_INVALID_PARAMETER;
    if (phys_addr % PAGE_SIZE != 0) return EFI_INVALID_PARAMETER;
    if ((phys_addr & ~ENTRY_ADDRESS_MASK) != 0) return EFI_INVALID_PARAMETER;
    if (flags & ~(WRITABLE | NO_EXEC)) return EFI_INVALID_PARAMETER;

    PageTableNode *pdpt = get_or_create_child(&root->entries[pml4_index(virtual_addr)]);
    if (!pdpt) return EFI_OUT_OF_RESOURCES;

    PageTableNode *pd = get_or_create_child(&pdpt->entries[pdpt_index(virtual_addr)]);
    if (!pd) return EFI_OUT_OF_RESOURCES;

    PageTableNode *pt = get_or_create_child(&pd->entries[pd_index(virtual_addr)]);
    if (!pt) return EFI_OUT_OF_RESOURCES;

    uint64_t *entry = &pt->entries[pt_index(virtual_addr)];
    if (*entry & PRESENT) return EFI_ALREADY_STARTED;
    *entry = phys_addr | PRESENT | flags;
    return EFI_SUCCESS;
}

EFI_STATUS paging_map_range(uint64_t virtual_start, uint64_t phys_start, uint64_t byte_size, uint64_t flags) {
    if (virtual_start % PAGE_SIZE != phys_start % PAGE_SIZE) return EFI_INVALID_PARAMETER;
    if (flags & ~(WRITABLE | NO_EXEC)) return EFI_INVALID_PARAMETER;
    if (byte_size == 0) return EFI_SUCCESS;

    uint64_t offset = virtual_start % PAGE_SIZE;
    if (byte_size > UINT64_MAX - offset) return EFI_INVALID_PARAMETER;
    uint64_t virtual_page = virtual_start - offset;
    uint64_t phys_page = phys_start - offset;

    uint64_t range_size = offset + byte_size;
    uint64_t num_pages = range_size / PAGE_SIZE + (range_size % PAGE_SIZE != 0);

    for (uint64_t i = 0; i < num_pages; i++) {
        EFI_STATUS status = paging_map_page(virtual_page + i * PAGE_SIZE, phys_page + i * PAGE_SIZE, flags);
        if (EFI_ERROR(status)) return status;
    }
    return EFI_SUCCESS;
}
