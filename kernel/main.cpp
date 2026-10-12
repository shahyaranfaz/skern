#include "cpu/gdt.hpp"
#include "cpu/tss.hpp"

namespace skern {

void main() {
    auto& tss_state = tss::init();
    gdt::init(tss_state);

    const char *message = "Hello from skern!\n";

    for (const char *p = message; *p != '\0'; ++p) {
        asm volatile("outb %0, $0xE9" : : "a"(*p));
    }

    while (true) {
        asm volatile("cli; hlt");
    }
}

} // namespace skern

extern "C" void kernel_main() {
    skern::main();
}
