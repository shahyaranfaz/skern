// clang-format off
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
enum : uint8_t {
    EI_NIDENT = 16,
};

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
#define ELF_MAGIC "\177ELF"

enum : uint8_t {
    ELF_MAGIC_LEN = 4,
};

// Indices in e_ident
enum : uint8_t {
    EI_CLASS   = 4, // ELF class
    EI_DATA    = 5, // Endianness
    EI_VERSION = 6, // ELF version index of version byte in e_ident
    EI_OSABI   = 7, // OS ABI
};

// Valid values
enum : uint8_t {
    ELFCLASS64    = 2, // 64-bit ELF
    ELFDATA2LSB   = 1, // Little-endian
    ELFOSABI_SYSV = 0, // System V ABI (no OS-specific extensions)
};

enum : uint32_t {
    EV_CURRENT = 1, // Current ELF version
};

// ELF object type
enum : uint16_t {
    ET_EXEC = 2, // Executable file (fixed-address)
};

// Target architecture
enum : uint16_t {
    EM_X86_64 = 62, // AMD64 / x86-64
};

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
enum : uint32_t {
    PT_LOAD    = 1, // Loadable segment
    PT_DYNAMIC = 2, // Dynamic linking information
    PT_INTERP  = 3, // Program interpreter
};

// Segment permissions
enum : uint32_t {
    PF_X = 1U << 0, // Executable
    PF_W = 1U << 1, // Writable
    PF_R = 1U << 2, // Readable
};

static_assert(sizeof(Elf64_EHeader) == 64);
static_assert(sizeof(Elf64_PHeader) == 56);
