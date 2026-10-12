#include "panic.hpp"

#include "drivers/serial.hpp"

namespace skern {

void panic(const char *message) {
    asm volatile("cli");

    serial::write("\nPANIC: ");
    serial::write(message);
    serial::write("\n");

    while (true) {
        asm volatile("hlt");
    }
}

} // namespace skern
