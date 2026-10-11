.text
.global gdt_activate
.type gdt_activate, @function

gdt_activate:
    # disable interrupts
    cli

    # load GDTR (first arg)
    lgdt (%rdi)

    # reload CS with a far return
    pushq $0x08
    leaq .reload_cs(%rip), %rax
    pushq %rax
    lretq

.reload_cs:
    # reload data segment registers
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    ret

.size gdt_activate, .-gdt_activate

.section .note.GNU-stack,"",@progbits
