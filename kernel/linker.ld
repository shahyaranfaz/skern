OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(kernel_main)

SKERN_VIRT_BASE = 0xFFFFFFFF80000000;
SKERN_PHYS_BASE = 0x200000;
SKERN_ADDR_OFFSET = SKERN_PHYS_BASE - SKERN_VIRT_BASE;

SECTIONS {
    . = SKERN_VIRT_BASE;
    __kernel_start = .;

    .text ALIGN(4096) : AT(ADDR(.text) + SKERN_ADDR_OFFSET) {
        *(.text .text.*)
    }

    .rodata.skernfont ALIGN(4096) : AT(ADDR(.rodata.skernfont) + SKERN_ADDR_OFFSET) {
        __skernfont_start = .;
        KEEP(*(.rodata.skernfont))
        __skernfont_end = .;
    }

    .rodata ALIGN(4096) : AT(ADDR(.rodata) + SKERN_ADDR_OFFSET) {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4096) : AT(ADDR(.data) + SKERN_ADDR_OFFSET) {
        *(.data .data.*)
    }

    .bss ALIGN(4096) : AT(ADDR(.bss) + SKERN_ADDR_OFFSET) {
        *(.bss .bss.*)
        *(COMMON)
    }

    . = ALIGN(4096);
    __kernel_end = .;

    __kernel_phys_start = LOADADDR(.text);
    __kernel_phys_end = SKERN_PHYS_BASE + (__kernel_end - SKERN_VIRT_BASE);

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame .eh_frame.*)
        *(.note .note.*)
    }
}
