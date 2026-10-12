#include "idt.hpp"

#include <cstdint>

#pragma pack(push, 1)

// clang-format off
struct IDT_Entry {
    uint16_t offset1;
    uint16_t selector;        // a code segment selector in GDT or LDT
    uint8_t  ist;             // bits 0–2: IST index (0 = disabled, 1–7 = IST1–IST7)
    uint8_t  type_attributes; // gate type, dpl, and p fields
    uint16_t offset2;
    uint32_t offset3;
    uint32_t reserved;
};
// clang-format on

struct IDT_Reg {
    uint16_t limit;
    uint64_t base;
};

#pragma pack(pop)

static_assert(sizeof(IDT_Entry) == 16);
static_assert(sizeof(IDT_Reg) == 10);

static IDT_Entry idt[256];
static IDT_Reg idtr;

extern "C" void idt_activate(IDT_Reg *idtr);

static void idt_init_entry(IDT_Entry &entry, uint64_t offset, uint8_t ist) {
    entry.offset1 = offset & 0xFFFF;
    entry.offset2 = (offset >> 16) & 0xFFFF;
    entry.offset3 = (offset >> 32) & 0xFFFFFFFF;

    entry.selector = 1 << 3;

    entry.ist = ist;

    entry.type_attributes = 0x8E;

    entry.reserved = 0;
}

static void idt_init_reg() {
    idtr.limit = sizeof(idt) - 1;
    idtr.base = reinterpret_cast<uint64_t>(&idt);
}

void idt_init() {
    // initialize table entries

    // initialize IDTR
    idt_init_reg();

    // activate the new IDT
    idt_activate(&idtr);
}
