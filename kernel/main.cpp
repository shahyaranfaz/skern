#include "cpu/gdt.hpp"
#include "cpu/tss.hpp"

extern "C" void kernel_main() {
    TSS *tss = tss_init();
    gdt_init(tss);

    const char *message = "Hello from skern!\n";

    for (const char *p = message; *p != '\0'; ++p) {
        asm volatile("outb %0, $0xE9" : : "a"(*p));
    }

    while (true) {
        asm volatile("cli; hlt");
    }
}
