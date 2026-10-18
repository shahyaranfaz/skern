#include "panic.hpp"

#include "console/console.hpp"
#include "drivers/serial.hpp"

namespace skern {

void panic(const char *message) {
    asm volatile("cli");

    serial::write("\nPANIC: ");
    serial::write(message);
    serial::write("\n");

    console::write("\nPANIC: ");
    console::write(message);
    console::write("\n");

    while (true) {
        asm volatile("hlt");
    }
}

} // namespace skern
