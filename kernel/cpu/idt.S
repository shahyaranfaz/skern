.text
.global skern_idt_load
.type skern_idt_load, @function

skern_idt_load:
    lidt (%rdi)
    ret

.size skern_idt_load, .-skern_idt_load

.section .note.GNU-stack,"",@progbits
