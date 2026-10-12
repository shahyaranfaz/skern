#include "gdt.hpp"

#include "tss.hpp"

#include <cstdint>

#pragma pack(push, 1)

struct GDT_Entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t limit_high_flags;
    uint8_t base_high;
};

struct TSS_Entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle_low;
    uint8_t access;
    uint8_t limit_high_flags;
    uint8_t base_middle_high;
    uint32_t base_high;
    uint32_t reserved;
};

struct GDT_Table {
    GDT_Entry entries[5];
    TSS_Entry tss;
};

struct GDT_Reg {
    uint16_t limit;
    uint64_t base;
};

#pragma pack(pop)

static_assert(sizeof(GDT_Entry) == 8);
static_assert(sizeof(TSS_Entry) == 16);
static_assert(sizeof(GDT_Table) == 56);
static_assert(sizeof(GDT_Reg) == 10);

static GDT_Table gdt;
static GDT_Reg gdtr;

extern "C" void gdt_activate(GDT_Reg *gdtr);

static void gdt_init_entry(GDT_Entry &entry, uint32_t base, uint32_t limit, uint8_t access,
                           uint8_t flags) {
    entry.limit_low = limit & 0xFFFF;
    entry.base_low = base & 0xFFFF;
    entry.base_middle = (base >> 16) & 0xFF;
    entry.access = access;
    entry.limit_high_flags = ((limit >> 16) & 0xF) | ((flags & 0xF) << 4);
    entry.base_high = (base >> 24) & 0xFF;
}

static void gdt_init_tss(TSS_Entry &entry, TSS *tss) {
    uint64_t base = reinterpret_cast<uint64_t>(tss);

    entry.base_low = base & 0xFFFF;
    entry.base_middle_low = (base >> 16) & 0xFF;
    entry.base_middle_high = (base >> 24) & 0xFF;
    entry.base_high = (base >> 32) & 0xFFFFFFFF;

    // 103 fits in 16 bits
    entry.limit_low = sizeof(TSS) - 1;
    entry.limit_high_flags = 0x00;

    entry.access = 0x89;

    entry.reserved = 0;
}

static void gdt_init_reg() {
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = reinterpret_cast<uint64_t>(&gdt);
}

void gdt_init(TSS *tss) {
    // initialize descriptors
    gdt_init_entry(gdt.entries[0], 0, 0, 0, 0);
    gdt_init_entry(gdt.entries[1], 0, 0xFFFFF, 0x9A, 0xA);
    gdt_init_entry(gdt.entries[2], 0, 0xFFFFF, 0x92, 0xC);
    gdt_init_entry(gdt.entries[3], 0, 0xFFFFF, 0xFA, 0xA);
    gdt_init_entry(gdt.entries[4], 0, 0xFFFFF, 0xF2, 0xC);

    // initialize tss entry in gdt
    gdt_init_tss(gdt.tss, tss);

    // initialize GDTR
    gdt_init_reg();

    // activate the new GDT
    gdt_activate(&gdtr);
}
