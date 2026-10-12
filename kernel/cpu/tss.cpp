#include "tss.hpp"

#include <cstdint>

namespace skern::tss {
namespace {

constexpr uint64_t double_fault_stack_size = 16 * 1024;

State state{};
alignas(16) uint8_t double_fault_stack[double_fault_stack_size];

} // namespace

State &init() {
    state = {};
    state.iopb = sizeof(State);
    state.ist1 = reinterpret_cast<uint64_t>(double_fault_stack + sizeof(double_fault_stack));
    return state;
}

} // namespace skern::tss
