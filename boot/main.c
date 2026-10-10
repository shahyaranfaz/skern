#include <efi.h>
#include <efilib.h>

EFI_STATUS EFIAPI efi_main(EFI_HANDLE img_handle, EFI_SYSTEM_TABLE *sys_table) {
    InitializeLib(img_handle, sys_table);

    Print(L"Hello from skern!\r\n");

    WaitForSingleEvent(sys_table->ConIn->WaitForKey, 0);

    return EFI_SUCCESS;
}
