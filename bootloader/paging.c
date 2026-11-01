#include "paging.h"

#include <efilib.h>

static void clear_table(SkernPageTable *table) {
    for (int i = 0; i < 512; i++) {
        table->entries[i] = 0;
    }
}

// clang-format off
enum : uint64_t {
    CHILD_TABLE_FLAGS = SKERN_PAGE_PRESENT | SKERN_PAGE_WRITABLE,
};
// clang-format on

static SkernPageTable *root;

static SkernPageTable *allocate_table(uint64_t *phys_addr) {
    EFI_PHYSICAL_ADDRESS address = 0;
    EFI_STATUS status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, 1, &address);
    if (EFI_ERROR(status)) return NULL;

    SkernPageTable *table = (SkernPageTable *)address;
    clear_table(table);
    *phys_addr = address;
    return table;
}

static SkernPageTable *get_child(uint64_t entry) {
    if (entry & SKERN_PAGE_PRESENT) {
        uint64_t phys_addr = entry & SKERN_PAGE_ENTRY_ADDRESS_MASK;
        return (SkernPageTable *)phys_addr;
    }
    return NULL;
}

static SkernPageTable *get_or_create_child(uint64_t *entry) {
    SkernPageTable *table = get_child(*entry);
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
    if (!skern_is_canonical_address(virtual_addr)) return EFI_INVALID_PARAMETER;
    if (virtual_addr % SKERN_PAGE_SIZE != 0) return EFI_INVALID_PARAMETER;
    if (phys_addr % SKERN_PAGE_SIZE != 0) return EFI_INVALID_PARAMETER;
    if ((phys_addr & ~SKERN_PAGE_ENTRY_ADDRESS_MASK) != 0) return EFI_INVALID_PARAMETER;
    if (flags & ~(SKERN_PAGE_WRITABLE | SKERN_PAGE_NO_EXEC)) return EFI_INVALID_PARAMETER;

    SkernPageTable *pdpt = get_or_create_child(&root->entries[skern_pml4_index(virtual_addr)]);
    if (!pdpt) return EFI_OUT_OF_RESOURCES;

    SkernPageTable *pd = get_or_create_child(&pdpt->entries[skern_pdpt_index(virtual_addr)]);
    if (!pd) return EFI_OUT_OF_RESOURCES;

    SkernPageTable *pt = get_or_create_child(&pd->entries[skern_pd_index(virtual_addr)]);
    if (!pt) return EFI_OUT_OF_RESOURCES;

    uint64_t *entry = &pt->entries[skern_pt_index(virtual_addr)];
    if (*entry & SKERN_PAGE_PRESENT) return EFI_ALREADY_STARTED;
    *entry = phys_addr | SKERN_PAGE_PRESENT | flags;
    return EFI_SUCCESS;
}

EFI_STATUS paging_map_range(uint64_t virtual_start, uint64_t phys_start, uint64_t byte_size, uint64_t flags) {
    if (virtual_start % SKERN_PAGE_SIZE != phys_start % SKERN_PAGE_SIZE) return EFI_INVALID_PARAMETER;
    if (flags & ~(SKERN_PAGE_WRITABLE | SKERN_PAGE_NO_EXEC)) return EFI_INVALID_PARAMETER;
    if (byte_size == 0) return EFI_SUCCESS;

    uint64_t offset = virtual_start % SKERN_PAGE_SIZE;
    if (byte_size > UINT64_MAX - offset) return EFI_INVALID_PARAMETER;
    uint64_t virtual_page = virtual_start - offset;
    uint64_t phys_page = phys_start - offset;

    uint64_t range_size = offset + byte_size;
    uint64_t num_pages = range_size / SKERN_PAGE_SIZE + (range_size % SKERN_PAGE_SIZE != 0);

    for (uint64_t i = 0; i < num_pages; i++) {
        EFI_STATUS status = paging_map_page(virtual_page + i * SKERN_PAGE_SIZE, phys_page + i * SKERN_PAGE_SIZE, flags);
        if (EFI_ERROR(status)) return status;
    }
    return EFI_SUCCESS;
}
