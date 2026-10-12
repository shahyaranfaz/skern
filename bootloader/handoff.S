.text
.global skern_bootloader_handoff
.type skern_bootloader_handoff, @function

skern_bootloader_handoff:
    mov %rsi, %rsp       # move stack_top to rsp
    sub $8, %rsp         # reserve space for fake return address
    movq $0, (%rsp)      # fake return address
    mov $0, %rbp         # clear old frame pointer
    jmp *%rdi            # jump to kernel_main

.section .note.GNU-stack,"",@progbits
