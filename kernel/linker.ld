OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(kernel_main)

SECTIONS {
    . = 0x200000;
    __kernel_start = .;

    .text : ALIGN(4096) {
        *(.text .text.*)
    }

    .rodata.skernfont : ALIGN(4096) {
        __skernfont_start = .;
        KEEP(*(.rodata.skernfont))
        __skernfont_end = .;
    }

    .rodata : ALIGN(4096) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4096) {
        *(.data .data.*)
    }

    .bss : ALIGN(4096) {
        *(.bss .bss.*)
        *(COMMON)
    }

    . = ALIGN(4096);
    __kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame .eh_frame.*)
        *(.note .note.*)
    }
}
