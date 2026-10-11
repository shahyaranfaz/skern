#include "arch.h"
#include "elf.h"

#include <efi.h>
#include <efilib.h>

// x86-64 to exec kernel
__attribute__((sysv_abi, noreturn)) void kernel_handoff(Elf64_Addr entry_addr, UINTN stack_top);

EFI_STATUS EFIAPI efi_main(EFI_HANDLE img_handle, EFI_SYSTEM_TABLE *sys_table) {
    InitializeLib(img_handle, sys_table);

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

    // allocate kernel stack
    void *stack = AllocatePool(KERNEL_STACK_SIZE);
    if (stack == NULL) return EFI_OUT_OF_RESOURCES;

    // get memory map from UEFI
    UINTN map_size = 0;
    UINTN map_key, descriptor_size;
    UINT32 descriptor_version;
    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &map_size, NULL, &map_key, &descriptor_size,
                               &descriptor_version);

    // should fail as we gave NULL buffer, map_size updated by call
    if (status != EFI_BUFFER_TOO_SMALL) return EFI_LOAD_ERROR;
    map_size += 8 * descriptor_size;
    UINTN map_capacity = map_size;

    // allocate map_size bytes
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

    UINTN stack_top = (UINTN)stack + KERNEL_STACK_SIZE;
    stack_top &= ~(UINTN)0xF;


    kernel_handoff(entry_addr, stack_top);
    // never returns
}
