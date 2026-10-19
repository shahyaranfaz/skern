#include "serial.hpp"

#include <cstddef>
#include <cstdint>

namespace {

constexpr uint16_t COM1 = 0x3F8;
constexpr size_t WAIT_TIMEOUT = 100000;

void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

bool transmit_ready() {
    return (inb(COM1 + 5) & 0x20) != 0;
}

bool wait_for_transmit() {
    for (size_t i = 0; i < WAIT_TIMEOUT; ++i) {
        if (transmit_ready()) {
            return true;
        }
    }
    return false;
}

} // namespace

namespace skern::serial {

bool init() {
    outb(COM1 + 1, 0x00); // Disable interrupts

    outb(COM1 + 3, 0x80); // Enable DLAB
    outb(COM1 + 0, 0x03); // Divisor low byte
    outb(COM1 + 1, 0x00); // Divisor high byte

    outb(COM1 + 3, 0x03); // 8N1
    outb(COM1 + 2, 0xC7); // Enable and clear FIFOs

    outb(COM1 + 4, 0x1E); // Enable loopback mode
    outb(COM1 + 0, 0xAE); // Send a test byte
    if (inb(COM1 + 0) != 0xAE) {
        outb(COM1 + 4, 0x00);
        return false;
    }

    outb(COM1 + 4, 0x0B); // Leave loopback; enable DTR, RTS, and OUT2
    return true;
}

bool write(char c) {
    if (!wait_for_transmit()) return false;
    outb(COM1, static_cast<uint8_t>(c));
    return true;
}

bool write(const char *str) {
    while (*str != '\0') {
        if (!write(*str)) return false;
        str++;
    }
    return true;
}

bool write(const char *data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        if (!write(data[i])) return false;
    }
    return true;
}

} // namespace skern::serial
