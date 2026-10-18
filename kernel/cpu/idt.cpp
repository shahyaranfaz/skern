#include "idt.hpp"

#include <cstdint>

extern "C" void skern_idt_load(const void *idtr);

namespace {

#pragma pack(push, 1)

// clang-format off
struct Entry {
    uint16_t offset1;
    uint16_t selector;        // a code segment selector in GDT or LDT
    uint8_t  ist;             // bits 0–2: IST index (0 = disabled, 1–7 = IST1–IST7)
    uint8_t  type_attributes; // gate type, dpl, and p fields
    uint16_t offset2;
    uint32_t offset3;
    uint32_t reserved;
};
// clang-format on

struct Register {
    uint16_t limit;
    uint64_t base;
};

#pragma pack(pop)

static_assert(sizeof(Entry) == 16);
static_assert(sizeof(Register) == 10);

Entry table[256]{};
Register idtr{};

void init_entry(Entry &entry, uint64_t offset, uint8_t ist) {
    entry.offset1 = offset & 0xFFFF;
    entry.offset2 = (offset >> 16) & 0xFFFF;
    entry.offset3 = (offset >> 32) & 0xFFFFFFFF;

    entry.selector = 1 << 3;

    entry.ist = ist;

    entry.type_attributes = 0x8E;

    entry.reserved = 0;
}

void init_register() {
    idtr.limit = sizeof(table) - 1;
    idtr.base = reinterpret_cast<uint64_t>(&table);
}

} // namespace

namespace skern::idt {

void init() {
    (void)init_entry;

    // initialize table entries

    // initialize IDTR
    init_register();

    // activate the new IDT
    skern_idt_load(&idtr);
}

} // namespace skern::idt
