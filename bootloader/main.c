#include "elf.h"
#include "elf_types.h"

#include <skern/boot_info.h>

#include <efi.h>
#include <efilib.h>
#include <stddef.h>

#define BOOTSTRAP_STACK_SIZE (16 * 1024)

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

    status = uefi_call_wrapper(root->Open, 5, root, &kernel_file, L"\\skern\\kernel.elf",
                               EFI_FILE_MODE_READ, 0);

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

static EFI_STATUS setup_memory_map(MemoryMapInfo *mem_info, EFI_HANDLE img_handle) {
    UINTN map_size = 0;
    UINTN map_key, descriptor_size;
    UINT32 descriptor_version;
    EFI_STATUS status = uefi_call_wrapper(BS->GetMemoryMap, 5, &map_size, NULL, &map_key, &descriptor_size,
                                     &descriptor_version);

    // should fail as we gave NULL buffer, map_size updated by call
    if (status != EFI_BUFFER_TOO_SMALL) return EFI_LOAD_ERROR;
    map_size += 8 * descriptor_size;
    UINTN map_capacity = map_size;

    EFI_MEMORY_DESCRIPTOR *map = AllocatePool(map_capacity);
    if (map == NULL) return EFI_OUT_OF_RESOURCES;

    // now actually get the memory map
    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &map_size, map, &map_key, &descriptor_size,
                               &descriptor_version);
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
        map_size = map_capacity;
        status = uefi_call_wrapper(BS->GetMemoryMap, 5, &map_size, map, &map_key, &descriptor_size,
                                   &descriptor_version);
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

    EFI_STATUS status = uefi_call_wrapper(BS->LocateProtocol, 3, &gop_guid, NULL, (void**)&gop);
    if (EFI_ERROR(status)) {
        Print(L"Failed to locate GOP\r\n");
        return status;
    }

    fb_info->address = gop->Mode->FrameBufferBase;
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
                                                        const BootInfo *boot_info);

EFI_STATUS EFIAPI efi_main(EFI_HANDLE img_handle, EFI_SYSTEM_TABLE *sys_table) {
    InitializeLib(img_handle, sys_table);

    EFI_STATUS status;
    Elf64_Addr entry_addr;
    BootInfo *boot_info = AllocatePool(sizeof(BootInfo));
    if (boot_info == NULL) return EFI_OUT_OF_RESOURCES;
    SetMem(boot_info, sizeof(BootInfo), 0);

    status = load_kernel_to_ram(img_handle, &entry_addr);
    if (EFI_ERROR(status)) return status;

    status = load_boot_info(boot_info);
    if (EFI_ERROR(status)) return status;

    status = setup_framebuffer(&boot_info->framebuffer);
    if (EFI_ERROR(status)) return status;

    status = setup_memory_map(&boot_info->memory_map, img_handle);
    if (EFI_ERROR(status)) return status;

    UINTN stack_top = boot_info->bootstrap_stack.base + boot_info->bootstrap_stack.size;
    stack_top &= ~(UINTN)0xF;

    skern_bootloader_handoff(entry_addr, stack_top, boot_info);
    return EFI_ABORTED;
}
