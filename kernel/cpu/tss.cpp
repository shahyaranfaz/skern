#include "tss.hpp"

#include <cstdint>

#define DOUBLE_FAULT_STACK_SIZE (16 * 1024)

static TSS tss{};

alignas(16) static uint8_t double_fault_stack[DOUBLE_FAULT_STACK_SIZE];

TSS *tss_init() {
    tss = {};
    tss.iopb = sizeof(TSS);
    tss.ist1 = reinterpret_cast<uint64_t>(double_fault_stack + sizeof(double_fault_stack));
    return &tss;
}
