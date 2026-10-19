#include "console/console.hpp"
#include "cpu/gdt.hpp"
#include "cpu/idt.hpp"
#include "cpu/tss.hpp"
#include "drivers/serial.hpp"

#include <skern/boot_info.h>

extern "C" void skern_trigger_test();

namespace skern {

void trap_test_main(const BootInfo *boot_info) {
    serial::init();

    if (boot_info == nullptr || !console::init(&boot_info->framebuffer)) {
        serial::write("TRAP TEST SETUP FAILED\n");
        while (true) asm volatile("cli; hlt");
    }

    auto &tss_state = tss::init();
    gdt::init(tss_state);
    idt::init();

    serial::write("TRAP TEST READY\n");
    skern_trigger_test();

    serial::write("TRAP TEST RETURNED\n");
    while (true) asm volatile("cli; hlt");
}

} // namespace skern

extern "C" void kernel_main(const BootInfo *boot_info) {
    skern::trap_test_main(boot_info);
}
