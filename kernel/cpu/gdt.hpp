// clang-format off
#pragma once

#include <cstdint>

struct TSS;

void gdt_init(TSS *tss);

enum class SegmentSelector : uint16_t {
    Null       = 0x00,
    KernelCode = 0x08,
    KernelData = 0x10,
    UserCode   = 0x1B,
    UserData   = 0x23,
    TSS        = 0x28,
};
