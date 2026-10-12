.text
.global skern_bootloader_handoff
.type skern_bootloader_handoff, @function

skern_bootloader_handoff:
    mov %rdi, %rax       # preserve entry_addr
    mov %rsi, %rsp       # move stack_top to rsp
    sub $8, %rsp         # reserve space for fake return address
    movq $0, (%rsp)      # fake return address
    mov $0, %rbp         # clear old frame pointer
    mov %rdx, %rdi       # pass boot_info as kernel_main's first argument
    jmp *%rax            # jump to kernel_main

.section .note.GNU-stack,"",@progbits
