#pragma once

#include <stdint.h>

// https://refspecs.linuxfoundation.org/elf/gabi4+/ch4.sheader.html
// https://refspecs.linuxfoundation.org/elf/gabi4+/ch4.eheader.html
// https://refspecs.linuxfoundation.org/elf/gabi4+/ch5.pheader.html

// ============================================================================
// ELF Types
// ============================================================================
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef uint64_t Elf64_Xword;

typedef uint64_t Elf64_Addr;

typedef uint64_t Elf64_Off;

// Number of identification bytes
#define EI_NIDENT 16

// ============================================================================
// ELF Header
// ============================================================================
typedef struct {
    unsigned char   e_ident[EI_NIDENT];
    Elf64_Half      e_type;
    Elf64_Half      e_machine;
    Elf64_Word      e_version;
    Elf64_Addr      e_entry;
    Elf64_Off       e_phoff;
    Elf64_Off       e_shoff;
    Elf64_Word      e_flags;
    Elf64_Half      e_ehsize;
    Elf64_Half      e_phentsize;
    Elf64_Half      e_phnum;
    Elf64_Half      e_shentsize;
    Elf64_Half      e_shnum;
    Elf64_Half      e_shstrndx;
} Elf64_EHeader;

// ELF magic number (0x7F followed by "ELF")
#define ELF_MAGIC     "\177ELF"
#define ELF_MAGIC_LEN 4

// Indices in e_ident
#define EI_CLASS   4 // ELF class
#define EI_DATA    5 // Endianness
#define EI_VERSION 6 // ELF version Index of version byte in e_ident
#define EI_OSABI   7 // OS ABI

// Valid values
#define ELFCLASS64    2 // 64-bit ELF
#define ELFDATA2LSB   1 // Little-endian
#define EV_CURRENT    1 // Current ELF version
#define ELFOSABI_SYSV 0 // System V ABI (no OS-specific extensions)

// ELF object type
#define ET_EXEC 2  // Executable file (fixed-address)

// Target architecture
#define EM_X86_64 62  // AMD64 / x86-64

// ============================================================================
// ELF Program Header
// ============================================================================
typedef struct {
    Elf64_Word	p_type;
    Elf64_Word	p_flags;
    Elf64_Off	p_offset;
    Elf64_Addr	p_vaddr;
    Elf64_Addr	p_paddr;
    Elf64_Xword	p_filesz;
    Elf64_Xword	p_memsz;
    Elf64_Xword	p_align;
} Elf64_PHeader;

// Program header types
#define PT_LOAD    1  // Loadable segment
#define PT_DYNAMIC 2  // Dynamic linking information
#define PT_INTERP  3  // Program interpreter

// Segment permissions
#define PF_X (1 << 0)  // Executable
#define PF_W (1 << 1)  // Writable
#define PF_R (1 << 2)  // Readable

_Static_assert(sizeof(Elf64_EHeader) == 64, "Invalid ELF64 header");
_Static_assert(sizeof(Elf64_PHeader) == 56, "Invalid ELF64 program header");