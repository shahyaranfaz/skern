#include "cpu/gdt.hpp"
#include "cpu/tss.hpp"

#include <skern/boot_info.h>

namespace skern {

void main(const BootInfo *boot_info) {
    (void)boot_info;

    auto& tss_state = tss::init();
    gdt::init(tss_state);

    char message[] = "Hello from skern!\n";

    for (char *p = message; *p != '\0'; ++p) {
        asm volatile("outb %0, $0xE9" : : "a"(*p));
    }

    while (true) {
        asm volatile("cli; hlt");
    }
}

} // namespace skern

extern "C" void kernel_main(const BootInfo *boot_info) {
    skern::main(boot_info);
}
