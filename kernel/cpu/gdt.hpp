#pragma once

#include <cstdint>

void gdt_init();

enum class SegmentSelector : std::uint16_t {
    Null = 0x00,
    KernelCode = 0x08,
    KernelData = 0x10,
    UserCode = 0x18,
    UserData = 0x20,
    TSS = 0x28,
};

struct [[gnu::packed]] GDT_Entry {
    std::uint16_t limit_low;
    std::uint16_t base_low;
    std::uint8_t base_middle;
    std::uint8_t access;
    std::uint8_t limit_high_flags;
    std::uint8_t base_high;
};

struct [[gnu::packed]] GDT_Reg {
    std::uint16_t limit;
    std::uint64_t base;
};

static_assert(sizeof(GDT_Entry) == 8);
static_assert(sizeof(GDT_Reg) == 10);
