#include "console/console.hpp"
#include "cpu/gdt.hpp"
#include "cpu/idt.hpp"
#include "cpu/tss.hpp"
#include "drivers/serial.hpp"
#include "mm/memory_map.hpp"
#include "panic.hpp"

#include <skern/boot_info.h>

namespace skern {

void main(const BootInfo *boot_info) {
    serial::init();

    if (boot_info == nullptr) panic("Missing boot info");
    if (boot_info->magic != SKERN_BOOT_INFO_MAGIC) panic("Invalid boot info magic");
    if (boot_info->version != SKERN_BOOT_INFO_VERSION) panic("Unsupported boot info version");
    if (boot_info->size != sizeof(BootInfo)) panic("Invalid boot info size");

    if (!console::init(&boot_info->framebuffer)) panic("Failed to initialize console");
    if (!memory_map::normalize(boot_info->memory_map)) panic("Invalid memory map");

    auto &tss_state = tss::init();
    gdt::init(tss_state);
    idt::init();

    serial::write("skern booted!\n");
    console::write("skern booted!\n");

    while (true) {
        asm volatile("cli; hlt");
    }
}

} // namespace skern

extern "C" void kernel_main(const BootInfo *boot_info) {
    skern::main(boot_info);
}
