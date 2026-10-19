#include "tss.hpp"

#include <cstdint>

namespace {

using namespace skern;

enum : uint64_t {
    IST_STACK_SIZE = 16 * 1024,
};

tss::State state{};
alignas(16) uint8_t double_fault_stack[IST_STACK_SIZE];
alignas(16) uint8_t nmi_stack[IST_STACK_SIZE];
alignas(16) uint8_t machine_check_stack[IST_STACK_SIZE];

} // namespace

namespace skern::tss {

State &init() {
    state = {};
    state.iopb = sizeof(State);
    state.ist1 = reinterpret_cast<uint64_t>(double_fault_stack + sizeof(double_fault_stack));
    state.ist2 = reinterpret_cast<uint64_t>(nmi_stack + sizeof(nmi_stack));
    state.ist3 = reinterpret_cast<uint64_t>(machine_check_stack + sizeof(machine_check_stack));
    return state;
}

} // namespace skern::tss
