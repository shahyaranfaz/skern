.text
.global skern_bootloader_handoff
.type skern_bootloader_handoff, @function

skern_bootloader_handoff:
    cli
    mov %rdi, %r8        # preserve entry_addr
    mov %rdx, %r9        # preserve boot_info
    mov %rcx, %r10       # preserve root_phys_addr

    mov $0xC0000080, %ecx
    rdmsr
    bts $11, %eax        # enable EFER.NXE
    wrmsr

    mov %r10, %cr3       # activate bootloader page tables
    mov %rsi, %rsp       # move stack_top to rsp
    sub $8, %rsp         # reserve space for fake return address
    movq $0, (%rsp)      # fake return address
    mov $0, %rbp         # clear old frame pointer
    mov %r9, %rdi        # pass boot_info as kernel_main's first argument
    jmp *%r8             # jump to kernel_main

.size skern_bootloader_handoff, .-skern_bootloader_handoff

.section .note.GNU-stack,"",@progbits
