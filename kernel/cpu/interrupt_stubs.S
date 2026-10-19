.text
.altmacro

.macro INTERRUPT_STUB vector
.global skern_vector_\vector
.type skern_vector_\vector, @function
skern_vector_\vector:
    pushq $0
    pushq $\vector
    jmp skern_trap_entry
.size skern_vector_\vector, .-skern_vector_\vector
.endm

.set vector, 32
.rept 224
    INTERRUPT_STUB %vector
    .set vector, vector + 1
.endr

.noaltmacro

.section .note.GNU-stack,"",@progbits
