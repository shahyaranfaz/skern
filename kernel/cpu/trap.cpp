#include "trap.hpp"

#include "../console/console.hpp"
#include "../drivers/serial.hpp"

#include <cstdint>

namespace {

using namespace skern;

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

void write(const char *text) {
    serial::write(text);
    console::write(text);
}

void write_hex(uint64_t value) {
    char text[19];
    text[0] = '0';
    text[1] = 'x';

    for (uint32_t index = 0; index < 16; index++) {
        uint32_t shift = (15 - index) * 4;
        uint8_t digit = (value >> shift) & 0xF;
        text[index + 2] = digit < 10 ? '0' + digit : 'A' + digit - 10;
    }

    text[18] = '\0';
    write(text);
}

void write_register(const char *name, uint64_t value) {
    write(name);
    write("=");
    write_hex(value);
    write(" ");
}

const char *exception_name(uint64_t vector) {
    switch (vector) {
        case 0:
            return "Divide Error";
        case 1:
            return "Debug Exception";
        case 2:
            return "Non-Maskable Interrupt";
        case 3:
            return "Breakpoint";
        case 4:
            return "Overflow";
        case 5:
            return "BOUND Range Exceeded";
        case 6:
            return "Invalid Opcode";
        case 7:
            return "Device Not Available";
        case 8:
            return "Double Fault";
        case 10:
            return "Invalid TSS";
        case 11:
            return "Segment Not Present";
        case 12:
            return "Stack-Segment Fault";
        case 13:
            return "General Protection Fault";
        case 14:
            return "Page Fault";
        case 16:
            return "x87 Floating-Point Exception";
        case 17:
            return "Alignment Check";
        case 18:
            return "Machine Check";
        case 19:
            return "SIMD Floating-Point Exception";
        case 20:
            return "Virtualization Exception";
        case 21:
            return "Control Protection Exception";
        case 28:
            return "Hypervisor Injection Exception";
        case 29:
            return "VMM Communication Exception";
        case 30:
            return "Security Exception";
        default:
            return "Reserved or Unexpected Exception";
    }
}

uint64_t read_cr2() {
    uint64_t value;
    asm volatile("mov %%cr2, %0" : "=r"(value));
    return value;
}

void handle_trap(TrapFrame *frame) {
    asm volatile("cli");

    write("\nTRAP: ");
    if (frame->vector < 32) {
        write(exception_name(frame->vector));
    } else {
        write("Unexpected Interrupt");
    }

    write("\n");
    write_register("VECTOR", frame->vector);
    write_register("ERROR", frame->error_code);
    write("\n");

    write_register("RIP", frame->rip);
    write_register("CS", frame->cs);
    write_register("RFLAGS", frame->rflags);
    write("\n");

    if (frame->vector == 14) {
        write_register("CR2", read_cr2());
        write("\n");
    }

    write_register("RAX", frame->rax);
    write_register("RBX", frame->rbx);
    write_register("RCX", frame->rcx);
    write_register("RDX", frame->rdx);
    write("\n");

    write_register("RSI", frame->rsi);
    write_register("RDI", frame->rdi);
    write_register("RBP", frame->rbp);
    write("\n");

    write_register("R8", frame->r8);
    write_register("R9", frame->r9);
    write_register("R10", frame->r10);
    write_register("R11", frame->r11);
    write("\n");

    write_register("R12", frame->r12);
    write_register("R13", frame->r13);
    write_register("R14", frame->r14);
    write_register("R15", frame->r15);
    write("\n");

    while (true) {
        asm volatile("hlt");
    }
}

} // namespace

namespace skern::trap {

extern "C" void skern_trap_dispatch(void *frame) {
    handle_trap(static_cast<TrapFrame *>(frame));
}

} // namespace skern::trap
