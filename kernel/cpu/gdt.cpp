#include "gdt.hpp"

#include "tss.hpp"

#include <cstdint>

extern "C" void skern_gdt_load(const void *gdtr);

namespace skern::gdt {
namespace {

#pragma pack(push, 1)

struct Entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t limit_high_flags;
    uint8_t base_high;
};

struct TssEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle_low;
    uint8_t access;
    uint8_t limit_high_flags;
    uint8_t base_middle_high;
    uint32_t base_high;
    uint32_t reserved;
};

struct Table {
    Entry entries[5];
    TssEntry tss;
};

struct Register {
    uint16_t limit;
    uint64_t base;
};

#pragma pack(pop)

static_assert(sizeof(Entry) == 8);
static_assert(sizeof(TssEntry) == 16);
static_assert(sizeof(Table) == 56);
static_assert(sizeof(Register) == 10);

Table table{};
Register gdtr{};

void init_entry(Entry &entry, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    entry.limit_low = limit & 0xFFFF;
    entry.base_low = base & 0xFFFF;
    entry.base_middle = (base >> 16) & 0xFF;
    entry.access = access;
    entry.limit_high_flags = ((limit >> 16) & 0xF) | ((flags & 0xF) << 4);
    entry.base_high = (base >> 24) & 0xFF;
}

void init_tss(TssEntry &entry, tss::State &state) {
    uint64_t base = reinterpret_cast<uint64_t>(&state);

    entry.base_low = base & 0xFFFF;
    entry.base_middle_low = (base >> 16) & 0xFF;
    entry.base_middle_high = (base >> 24) & 0xFF;
    entry.base_high = (base >> 32) & 0xFFFFFFFF;

    // 103 fits in 16 bits
    entry.limit_low = sizeof(tss::State) - 1;
    entry.limit_high_flags = 0x00;

    entry.access = 0x89;

    entry.reserved = 0;
}

void init_register() {
    gdtr.limit = sizeof(table) - 1;
    gdtr.base = reinterpret_cast<uint64_t>(&table);
}

} // namespace

void init(tss::State &state) {
    // initialize descriptors
    init_entry(table.entries[0], 0, 0, 0, 0);
    init_entry(table.entries[1], 0, 0xFFFFF, 0x9A, 0xA);
    init_entry(table.entries[2], 0, 0xFFFFF, 0x92, 0xC);
    init_entry(table.entries[3], 0, 0xFFFFF, 0xFA, 0xA);
    init_entry(table.entries[4], 0, 0xFFFFF, 0xF2, 0xC);

    // initialize tss entry in gdt
    init_tss(table.tss, state);

    // initialize GDTR
    init_register();

    // activate the new GDT
    skern_gdt_load(&gdtr);
}

} // namespace skern::gdt
