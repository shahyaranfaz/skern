.text
.global skern_gdt_load
.type skern_gdt_load, @function

skern_gdt_load:
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

    # load tss selector into task register
    movw $0x28, %ax
    ltr %ax

    ret

.size skern_gdt_load, .-skern_gdt_load

.section .note.GNU-stack,"",@progbits
