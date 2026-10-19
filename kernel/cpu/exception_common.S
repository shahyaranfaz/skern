.text
.global skern_trap_entry
.type skern_trap_entry, @function

skern_trap_entry:
    # Save registers
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Clear direction flag
    cld

    # Pass TrapFrame* as first argument
    movq %rsp, %rdi

    # Preserve stack pointer and align stack
    movq %rsp, %r12
    andq $-16, %rsp

    # Call cpp exception handler
    call skern_trap_dispatch

    # Restore original stack pointer
    movq %r12, %rsp

    # Restore registers
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    # Remove vector and error code
    addq $16, %rsp

    # Return from exception
    iretq

.size skern_trap_entry, .-skern_trap_entry

.section .note.GNU-stack,"",@progbits
