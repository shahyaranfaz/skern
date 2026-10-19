#include "trap.hpp"

#include <cstdint>

namespace {

struct TrapFrame {
    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx;
    uint64_t rcx, rbx, rax;

    uint64_t vector;
    uint64_t error_code;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
};

static_assert(sizeof(TrapFrame) == 160);

void handle_trap(TrapFrame *frame) {
    switch (frame->vector) {
        case 0:  // #DE - Divide Error
            break;

        case 1:  // #DB - Debug Exception
            break;

        case 2:  // NMI - Non-Maskable Interrupt
            break;

        case 3:  // #BP - Breakpoint
            break;

        case 4:  // #OF - Overflow
            break;

        case 5:  // #BR - BOUND Range Exceeded
            break;

        case 6:  // #UD - Invalid Opcode
            break;

        case 7:  // #NM - Device Not Available
            break;

        case 8:  // #DF - Double Fault
            break;

        case 10: // #TS - Invalid TSS
            break;

        case 11: // #NP - Segment Not Present
            break;

        case 12: // #SS - Stack-Segment Fault
            break;

        case 13: // #GP - General Protection Fault
            break;

        case 14: // #PF - Page Fault
            break;

        case 16: // #MF - x87 Floating-Point Exception
            break;

        case 17: // #AC - Alignment Check
            break;

        case 18: // #MC - Machine Check
            break;

        case 19: // #XM - SIMD Floating-Point Exception
            break;

        case 20: // #VE - Virtualization Exception
            break;

        case 21: // #CP - Control Protection Exception
            break;

        case 28: // #HV - Hypervisor Injection Exception
            break;

        case 29: // #VC - VMM Communication Exception
            break;

        case 30: // #SX - Security Exception
            break;

        default: // Reserved or unexpected vector
            break;
    }

    // TODO: Report exception and halt or recover.
}

} // namespace

namespace skern::trap {

extern "C" void skern_trap_dispatch(void *frame) {
    handle_trap(static_cast<TrapFrame *>(frame));
}

} // namespace skern::trap
