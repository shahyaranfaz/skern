#include "cpu/gdt.hpp"
#include "cpu/tss.hpp"

#include "drivers/serial.hpp"

#include <skern/boot_info.h>

namespace skern {

void main(const BootInfo *boot_info) {
    serial::init();

    (void)boot_info;

    auto& tss_state = tss::init();
    gdt::init(tss_state);

    serial::write("skern booted!\n");

    while (true) {
        asm volatile("cli; hlt");
    }
}

} // namespace skern

extern "C" void kernel_main(const BootInfo *boot_info) {
    skern::main(boot_info);
}
