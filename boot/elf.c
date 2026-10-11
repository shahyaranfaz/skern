#include "elf.h"

#include "arch.h"

#include <efi.h>
#include <efilib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


static bool power_of_two(uint64_t n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}

static bool elf_validate_eheader(const Elf64_EHeader *header) {
    if (CompareMem(header->e_ident, ELF_MAGIC, ELF_MAGIC_LEN) != 0) return false;
    // Use EFI function cuz we may not have string.h

    if (header->e_ident[EI_CLASS] != ELFCLASS64) return false;
    if (header->e_ident[EI_DATA] != ELFDATA2LSB) return false;
    if (header->e_ident[EI_VERSION] != EV_CURRENT) return false;
    if (header->e_ident[EI_OSABI] != ELFOSABI_SYSV) return false;
    if (header->e_type != ET_EXEC) return false;
    if (header->e_machine != EM_X86_64) return false;
    if (header->e_version != EV_CURRENT) return false;
    if (header->e_ehsize != sizeof(Elf64_EHeader)) return false;
    if (header->e_phentsize != sizeof(Elf64_PHeader)) return false;
    if (header->e_phnum == 0) return false;
    return true;
}

static bool elf_validate_pheaders(const uint8_t *bytes, const size_t file_size,
                                  const Elf64_EHeader *elf_header) {
    bool header_executable = false;

    for (size_t i = 0; i < elf_header->e_phnum; i++) {
        const uint8_t *ptr = bytes + elf_header->e_phoff + i * sizeof(Elf64_PHeader);
        Elf64_PHeader p_header;
        CopyMem(&p_header, (void *)ptr, sizeof(p_header));

        if (p_header.p_type == PT_INTERP || p_header.p_type == PT_DYNAMIC) return false;
        if (p_header.p_type != PT_LOAD) continue;
        if (p_header.p_memsz == 0) continue;

        if (p_header.p_filesz > p_header.p_memsz) return false;
        if (p_header.p_offset > file_size) return false;
        if (p_header.p_filesz > file_size - p_header.p_offset) return false;
        if (p_header.p_memsz > UINT64_MAX - p_header.p_vaddr) return false;
        if (p_header.p_memsz > UINT64_MAX - p_header.p_paddr) return false;

        if (p_header.p_align > 1) {
            if (!power_of_two(p_header.p_align)) return false;

            if (p_header.p_vaddr % p_header.p_align != p_header.p_offset % p_header.p_align)
                return false;
        }

        if ((p_header.p_flags & PF_X) && p_header.p_vaddr <= elf_header->e_entry &&
            elf_header->e_entry < p_header.p_vaddr + p_header.p_memsz) {
            header_executable = true;
        }
    }
    return header_executable;
}

static bool elf_validate(const void *file, size_t file_size) {
    if (file_size < sizeof(Elf64_EHeader)) return false;

    const Elf64_EHeader *elf_header = file;
    if (!elf_validate_eheader(elf_header)) return false;
    if (elf_header->e_phoff > file_size) return false;
    if ((size_t)elf_header->e_phnum * sizeof(Elf64_PHeader) > file_size - elf_header->e_phoff)
        return false;

    const uint8_t *bytes = file;
    return elf_validate_pheaders(bytes, file_size, elf_header);
}

typedef struct {
    EFI_PHYSICAL_ADDRESS address;
    UINTN pages;
} SegmentAllocation;

static EFI_STATUS get_file_size(EFI_FILE_HANDLE file, UINT64 *file_size) {
    EFI_FILE_INFO *file_info = LibFileInfo(file);
    if (file_info == NULL) return EFI_DEVICE_ERROR;

    *file_size = file_info->FileSize;
    FreePool(file_info);
    return EFI_SUCCESS;
}

static void free_allocations(const SegmentAllocation *allocations, UINTN count) {
    while (count > 0) {
        count--;
        uefi_call_wrapper(BS->FreePages, 2, allocations[count].address, allocations[count].pages);
    }
}

EFI_STATUS elf_load(EFI_FILE_HANDLE file, Elf64_Addr *entry) {
    if (file == NULL || entry == NULL) return EFI_INVALID_PARAMETER;

    // get file size
    UINT64 file_size;
    EFI_STATUS status = get_file_size(file, &file_size);
    if (EFI_ERROR(status)) return status;
    if (file_size < sizeof(Elf64_EHeader)) return EFI_LOAD_ERROR;
    if (file_size > (UINTN)-1) return EFI_OUT_OF_RESOURCES;

    // allocate buffer for file
    void *buffer = AllocatePool((UINTN)file_size);
    if (buffer == NULL) return EFI_OUT_OF_RESOURCES;

    status = uefi_call_wrapper(file->SetPosition, 2, file, 0);
    if (EFI_ERROR(status)) {
        FreePool(buffer);
        return status;
    }

    // read file to buffer
    UINTN bytes_read = file_size;
    status = uefi_call_wrapper(file->Read, 3, file, &bytes_read, buffer);
    if (EFI_ERROR(status) || bytes_read != file_size) {
        FreePool(buffer);
        return EFI_ERROR(status) ? status : EFI_LOAD_ERROR;
    }

    // validate elf
    if (!elf_validate(buffer, file_size)) {
        FreePool(buffer);
        return EFI_LOAD_ERROR;
    }

    // Load elf segments into kernel memory
    const Elf64_EHeader *elf_header = buffer;
    const uint8_t *bytes = buffer;
    SegmentAllocation *allocations =
        AllocatePool((UINTN)elf_header->e_phnum * sizeof(SegmentAllocation));
    if (allocations == NULL) {
        FreePool(buffer);
        return EFI_OUT_OF_RESOURCES;
    }
    UINTN allocation_count = 0;

    for (size_t i = 0; i < elf_header->e_phnum; i++) {
        Elf64_PHeader header;
        CopyMem(&header, (void *)(bytes + elf_header->e_phoff + i * sizeof(header)),
                sizeof(header));
        if (header.p_type != PT_LOAD) continue;
        if (header.p_memsz == 0) continue;

        uint64_t offset = header.p_paddr % PAGE_SIZE;
        uint64_t start = header.p_paddr - offset;

        uint64_t pages = (offset + header.p_memsz) / PAGE_SIZE;
        if ((offset + header.p_memsz) % PAGE_SIZE != 0) pages++;

        EFI_PHYSICAL_ADDRESS address = start;

        status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData, pages,
                                   &address);

        if (EFI_ERROR(status)) {
            free_allocations(allocations, allocation_count);
            FreePool(allocations);
            FreePool(buffer);
            return status;
        }

        allocations[allocation_count].address = address;
        allocations[allocation_count].pages = (UINTN)pages;
        allocation_count++;

        const uint8_t *source = (const uint8_t *)buffer + header.p_offset;
        void *destination = (void *)(UINTN)header.p_paddr;
        CopyMem(destination, (void *)source, (UINTN)header.p_filesz);
        SetMem((uint8_t *)destination + header.p_filesz, (UINTN)(header.p_memsz - header.p_filesz),
               0);
    }
    *entry = elf_header->e_entry; // addr to execute
    FreePool(allocations);
    FreePool(buffer);
    return EFI_SUCCESS;
}
