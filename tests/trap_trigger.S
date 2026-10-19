.text
.global skern_trigger_test
.type skern_trigger_test, @function

skern_trigger_test:
#if SKERN_TRAP_TEST == 2
1:
    cli
    hlt
    jmp 1b
#elif SKERN_TRAP_TEST == 6
    movabs $0x0101010101010101, %rax
    movabs $0x0202020202020202, %rbx
    movabs $0x0303030303030303, %rcx
    movabs $0x0404040404040404, %rdx
    movabs $0x0505050505050505, %rsi
    movabs $0x0606060606060606, %rdi
    movabs $0x0707070707070707, %rbp
    movabs $0x0808080808080808, %r8
    movabs $0x0909090909090909, %r9
    movabs $0x1010101010101010, %r10
    movabs $0x1111111111111111, %r11
    movabs $0x1212121212121212, %r12
    movabs $0x1313131313131313, %r13
    movabs $0x1414141414141414, %r14
    movabs $0x1515151515151515, %r15
    ud2
#elif SKERN_TRAP_TEST == 13
    movw $0x38, %ax
    movw %ax, %ds
#elif SKERN_TRAP_TEST == 14
    movabs $0x0000400000000000, %rax
    movq (%rax), %rax
#else
#error Unsupported SKERN_TRAP_TEST
#endif

    ret

.size skern_trigger_test, .-skern_trigger_test

.section .note.GNU-stack,"",@progbits
