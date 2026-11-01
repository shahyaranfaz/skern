#include "elf.h"
#include "elf_types.h"
#include "paging.h"

#include <skern/boot_info.h>
#include <skern/memory_layout.h>

#include <efi.h>
#include <efilib.h>
#include <stddef.h>

enum : UINTN {
    BOOTSTRAP_STACK_SIZE = 16 * 1024,
};

static EFI_STATUS load_kernel_to_ram(EFI_HANDLE img_handle, Elf64_Addr *entry) {
    // read kernel.elf from root/skern/kernel.elf
    EFI_FILE_HANDLE kernel_file;
    EFI_LOADED_IMAGE *loaded_image;
    EFI_STATUS status = uefi_call_wrapper(BS->HandleProtocol, 3, img_handle, &LoadedImageProtocol,
                                          (void **)&loaded_image);
    if (EFI_ERROR(status)) {
        Print(L"Failed to locate loaded image\r\n");
        return status;
    }

    EFI_FILE_HANDLE root = LibOpenRoot(loaded_image->DeviceHandle);
    if (root == NULL) {
        Print(L"Failed to open filesystem\r\n");
        return EFI_NOT_FOUND;
    }

    status = uefi_call_wrapper(root->Open, 5, root, &kernel_file, L"\\skern\\kernel.elf", EFI_FILE_MODE_READ, 0);

    uefi_call_wrapper(root->Close, 1, root);

    if (EFI_ERROR(status)) {
        Print(L"Failed to open kernel.elf\r\n");
        return status;
    }

    // load kernel elf to ram
    Elf64_Addr entry_addr;
    status = elf_load(kernel_file, &entry_addr);

    uefi_call_wrapper(kernel_file->Close, 1, kernel_file);

    if (EFI_ERROR(status)) {
        Print(L"Failed to load kernel\r\n");
        return status;
    }
    *entry = entry_addr;
    return EFI_SUCCESS;
}

static bool is_bootstrap_memory(UINT32 type) {
    switch (type) {
        case EfiLoaderCode:
        case EfiLoaderData:
        case EfiBootServicesCode:
        case EfiBootServicesData:
        case EfiConventionalMemory:
        case EfiACPIReclaimMemory:
        case EfiACPIMemoryNVS:
            return true;
        default:
            return false;
    }
}

static EFI_STATUS map_bootstrap_range(uint64_t phys_start, uint64_t byte_size) {
    if (phys_start > UINT64_MAX - byte_size) return EFI_LOAD_ERROR;

    uint64_t phys_end = phys_start + byte_size;
    uint64_t hhdm_capacity = SKERN_VIRT_BASE - SKERN_HHDM_BASE;
    if (phys_end > hhdm_capacity) return EFI_UNSUPPORTED;

    EFI_STATUS status = paging_map_range(phys_start, phys_start, byte_size, PAGING_WRITABLE);
    if (EFI_ERROR(status)) return status;

    return paging_map_range(SKERN_HHDM_BASE + phys_start, phys_start, byte_size, PAGING_WRITABLE | PAGING_NO_EXEC);
}

static EFI_STATUS build_bootstrap_mappings(const EFI_MEMORY_DESCRIPTOR *map, UINTN map_size, UINTN descriptor_size,
                                           const FramebufferInfo *framebuffer) {
    uint8_t *bytes = (uint8_t *)map;

    for (UINTN offset = 0; offset < map_size; offset += descriptor_size) {
        EFI_MEMORY_DESCRIPTOR *descriptor = (EFI_MEMORY_DESCRIPTOR *)(bytes + offset);
        if (!is_bootstrap_memory(descriptor->Type)) continue;
        if (descriptor->NumberOfPages > UINT64_MAX / EFI_PAGE_SIZE) return EFI_LOAD_ERROR;

        uint64_t byte_size = descriptor->NumberOfPages * EFI_PAGE_SIZE;
        EFI_STATUS status = map_bootstrap_range(descriptor->PhysicalStart, byte_size);
        if (EFI_ERROR(status)) return status;
    }

    if (framebuffer->byte_size == 0) return EFI_INVALID_PARAMETER;
    return map_bootstrap_range(framebuffer->address, framebuffer->byte_size);
}

static EFI_STATUS get_memory_map(EFI_MEMORY_DESCRIPTOR **map, UINTN *map_size, UINTN *map_capacity, UINTN *map_key,
                                 UINTN *descriptor_size, UINT32 *descriptor_version) {
    while (1) {
        *map_size = *map_capacity;
        EFI_STATUS status = uefi_call_wrapper(BS->GetMemoryMap, 5, map_size, *map, map_key, descriptor_size,
                                              descriptor_version);
        if (status != EFI_BUFFER_TOO_SMALL) return status;

        if (*descriptor_size > (UINTN)-1 / 8) return EFI_OUT_OF_RESOURCES;
        UINTN headroom = 8 * *descriptor_size;
        if (*map_size > (UINTN)-1 - headroom) return EFI_OUT_OF_RESOURCES;
        UINTN new_capacity = *map_size + headroom;

        EFI_MEMORY_DESCRIPTOR *new_map = AllocatePool(new_capacity);
        if (new_map == NULL) return EFI_OUT_OF_RESOURCES;

        if (*map != NULL) FreePool(*map);
        *map = new_map;
        *map_capacity = new_capacity;
    }
}

static EFI_STATUS setup_memory_map(MemoryMapInfo *mem_info, EFI_HANDLE img_handle, FramebufferInfo *framebuffer) {
    EFI_MEMORY_DESCRIPTOR *map = NULL;
    UINTN map_size = 0, map_capacity = 0;
    UINTN map_key, descriptor_size;
    UINT32 descriptor_version;

    EFI_STATUS status = get_memory_map(&map, &map_size, &map_capacity, &map_key, &descriptor_size, &descriptor_version);
    if (EFI_ERROR(status)) {
        Print(L"Failed to load memory map\r\n");
        return status;
    }

    // allocate page table frames (invalidates map_key)
    status = build_bootstrap_mappings(map, map_size, descriptor_size, framebuffer);
    if (EFI_ERROR(status)) {
        Print(L"Failed to build bootstrap mappings\r\n");
        return status;
    }

    // get final map/key
    status = get_memory_map(&map, &map_size, &map_capacity, &map_key, &descriptor_size, &descriptor_version);
    if (EFI_ERROR(status)) {
        Print(L"Failed to load memory map\r\n");
        return status;
    }

    // exit uefi boot services (bounded loop to retry on fail)
    for (int attempt = 0; attempt < 5; attempt++) {
        status = uefi_call_wrapper(BS->ExitBootServices, 2, img_handle, map_key);

        // break if no error, retry only if invalid parameter error
        if (!EFI_ERROR(status)) break;
        if (status != EFI_INVALID_PARAMETER) return status;

        // Refresh the memory map and its key (on first 4 attempts)
        if (attempt == 4) return status;
        status = get_memory_map(&map, &map_size, &map_capacity, &map_key, &descriptor_size, &descriptor_version);
        if (EFI_ERROR(status)) return status;
    }

    mem_info->descriptors = (uint64_t)map;
    mem_info->byte_size = map_size;
    mem_info->descriptor_size = descriptor_size;
    mem_info->descriptor_version = descriptor_version;
    mem_info->reserved = 0;
    return EFI_SUCCESS;
}

static EFI_STATUS setup_framebuffer(FramebufferInfo *fb_info) {
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

    EFI_STATUS status = uefi_call_wrapper(BS->LocateProtocol, 3, &gop_guid, NULL, (void **)&gop);
    if (EFI_ERROR(status)) {
        Print(L"Failed to locate GOP\r\n");
        return status;
    }

    fb_info->address = gop->Mode->FrameBufferBase;
    fb_info->byte_size = gop->Mode->FrameBufferSize;
    fb_info->width = gop->Mode->Info->HorizontalResolution;
    fb_info->height = gop->Mode->Info->VerticalResolution;
    fb_info->pixels_per_scanline = gop->Mode->Info->PixelsPerScanLine;
    fb_info->pixel_format = gop->Mode->Info->PixelFormat;
    return EFI_SUCCESS;
}

static EFI_STATUS load_boot_info(BootInfo *boot_info) {
    boot_info->magic = SKERN_BOOT_INFO_MAGIC;
    boot_info->version = SKERN_BOOT_INFO_VERSION;
    boot_info->size = sizeof(BootInfo);

    // setup bootstrap stack
    void *stack = AllocatePool(BOOTSTRAP_STACK_SIZE);
    if (stack == NULL) return EFI_OUT_OF_RESOURCES;
    boot_info->bootstrap_stack.base = (uint64_t)stack;
    boot_info->bootstrap_stack.size = BOOTSTRAP_STACK_SIZE;
    return EFI_SUCCESS;
}

// x86-64 to exec kernel
__attribute__((sysv_abi)) void skern_bootloader_handoff(Elf64_Addr entry_addr, UINTN stack_top,
                                                        const BootInfo *boot_info, uint64_t root_phys_addr);

EFI_STATUS EFIAPI efi_main(EFI_HANDLE img_handle, EFI_SYSTEM_TABLE *sys_table) {
    InitializeLib(img_handle, sys_table);

    uint64_t root_phys_addr;
    EFI_STATUS status;
    Elf64_Addr entry_addr;
    BootInfo *boot_info = AllocatePool(sizeof(BootInfo));
    if (boot_info == NULL) return EFI_OUT_OF_RESOURCES;
    SetMem(boot_info, sizeof(BootInfo), 0);

    status = paging_init(&root_phys_addr);
    if (EFI_ERROR(status)) return status;

    status = load_kernel_to_ram(img_handle, &entry_addr);
    if (EFI_ERROR(status)) return status;

    status = load_boot_info(boot_info);
    if (EFI_ERROR(status)) return status;

    status = setup_framebuffer(&boot_info->framebuffer);
    if (EFI_ERROR(status)) return status;

    status = setup_memory_map(&boot_info->memory_map, img_handle, &boot_info->framebuffer);
    if (EFI_ERROR(status)) return status;

    UINTN stack_top = boot_info->bootstrap_stack.base + boot_info->bootstrap_stack.size;
    stack_top &= ~(UINTN)0xF;

    skern_bootloader_handoff(entry_addr, stack_top, boot_info, root_phys_addr);
    return EFI_ABORTED;
}
