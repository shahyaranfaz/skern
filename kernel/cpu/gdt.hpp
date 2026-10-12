#pragma once

#include <cstdint>

void gdt_init();

enum class SegmentSelector : std::uint16_t {
    Null = 0x00,
    KernelCode = 0x08,
    KernelData = 0x10,
    UserCode = 0x1B,
    UserData = 0x23,
};

#pragma pack(push, 1)

struct GDT_Entry {
    std::uint16_t limit_low;
    std::uint16_t base_low;
    std::uint8_t base_middle;
    std::uint8_t access;
    std::uint8_t limit_high_flags;
    std::uint8_t base_high;
};

struct GDT_Reg {
    std::uint16_t limit;
    std::uint64_t base;
};

#pragma pack(pop)

static_assert(sizeof(GDT_Entry) == 8);
static_assert(sizeof(GDT_Reg) == 10);
