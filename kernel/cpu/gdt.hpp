// clang-format off
#pragma once

#include <cstdint>

namespace skern::tss {

struct State;

} // namespace skern::tss

namespace skern::gdt {

void init(const tss::State& tss);

enum class SegmentSelector : uint16_t {
    NULL_SEGMENT = 0x00,
    KERNEL_CODE  = 0x08,
    KERNEL_DATA  = 0x10,
    USER_CODE    = 0x1B,
    USER_DATA    = 0x23,
    TSS        = 0x28,
};

} // namespace skern::gdt
