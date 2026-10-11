#include "gdt.hpp"

#include <cstdint>

static GDT_Entry gdt[5];
static GDT_Reg gdtr;

extern "C" void gdt_activate(GDT_Reg *gdtr);

static void gdt_init_entry(GDT_Entry &entry, std::uint32_t base, std::uint32_t limit,
                           std::uint8_t access, std::uint8_t flags) {
    entry.limit_low = limit & 0xFFFF;
    entry.base_low = base & 0xFFFF;
    entry.base_middle = (base >> 16) & 0xFF;
    entry.access = access;
    entry.limit_high_flags = ((limit >> 16) & 0xF) | ((flags & 0xF) << 4);
    entry.base_high = (base >> 24) & 0xFF;
}

static void gdt_init_reg() {
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (std::uint64_t)&gdt;
}

void gdt_init() {
    // initialize descriptors
    gdt_init_entry(gdt[0], 0, 0, 0, 0);
    gdt_init_entry(gdt[1], 0, 0xFFFFF, 0x9A, 0xA);
    gdt_init_entry(gdt[2], 0, 0xFFFFF, 0x92, 0xC);
    gdt_init_entry(gdt[3], 0, 0xFFFFF, 0xFA, 0xA);
    gdt_init_entry(gdt[4], 0, 0xFFFFF, 0xF2, 0xC);

    // initialize GDTR
    gdt_init_reg();

    // activate the new GDT
    gdt_activate(&gdtr);
}
