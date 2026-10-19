.section .rodata
.p2align 3
.altmacro

.global skern_vector_table
.type skern_vector_table, @object

.macro EXCEPTION_ENTRY vector
    .quad skern_exception_\vector
.endm

.macro INTERRUPT_ENTRY vector
    .quad skern_vector_\vector
.endm

skern_vector_table:
    .set vector, 0
    .rept 32
        EXCEPTION_ENTRY %vector
        .set vector, vector + 1
    .endr

    .set vector, 32
    .rept 224
        INTERRUPT_ENTRY %vector
        .set vector, vector + 1
    .endr

.size skern_vector_table, .-skern_vector_table

.noaltmacro

.section .note.GNU-stack,"",@progbits
