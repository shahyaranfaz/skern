#include "console/console.hpp"
#include "cpu/gdt.hpp"
#include "cpu/idt.hpp"
#include "cpu/tss.hpp"
#include "drivers/serial.hpp"
#include "mm/memory_map.hpp"
#include "mm/pmm.hpp"
#include "mm/vmm.hpp"
#include "panic.hpp"

#include <skern/boot_info.h>
#include <skern/memory_layout.h>

extern "C" uint8_t __kernel_phys_start[];
extern "C" uint8_t __kernel_phys_end[];

namespace skern {

void main(const BootInfo *boot_info) {
    serial::init();

    if (boot_info == nullptr) panic("Missing boot info");

    uint64_t boot_info_start = reinterpret_cast<uint64_t>(boot_info);
    boot_info = reinterpret_cast<const BootInfo *>(SKERN_HHDM_BASE + boot_info_start);

    if (boot_info->magic != SKERN_BOOT_INFO_MAGIC) panic("Invalid boot info magic");
    if (boot_info->version != SKERN_BOOT_INFO_VERSION) panic("Unsupported boot info version");
    if (boot_info->size != sizeof(BootInfo)) panic("Invalid boot info size");

    if (!console::init(&boot_info->framebuffer)) panic("Failed to initialize console");

    if (!memory_map::normalize(boot_info->memory_map)) panic("Invalid memory map");
    pmm::init();

    uint64_t boot_info_end = boot_info_start + sizeof(BootInfo);
    pmm::protect_range(boot_info_start, boot_info_end);

    uint64_t bootstrap_stack_start = boot_info->bootstrap_stack.base;
    uint64_t bootstrap_stack_end = bootstrap_stack_start + boot_info->bootstrap_stack.size;
    pmm::protect_range(bootstrap_stack_start, bootstrap_stack_end);

    uint64_t kernel_phys_start = reinterpret_cast<uint64_t>(__kernel_phys_start);
    uint64_t kernel_phys_end = reinterpret_cast<uint64_t>(__kernel_phys_end);
    pmm::protect_range(kernel_phys_start, kernel_phys_end);

    uint64_t memory_map_start = boot_info->memory_map.descriptors;
    uint64_t memory_map_end = memory_map_start + boot_info->memory_map.byte_size;
    pmm::protect_range(memory_map_start, memory_map_end);

    if (!vmm::init()) panic("Failed to initialize virtual memory");

    auto &tss_state = tss::init();
    gdt::init(tss_state);
    idt::init();

    pmm::reclaim_bootloader_memory();

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
