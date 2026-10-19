.text

# pushq $0 if the caller does not provide error code (synthetic error code)
# 8, 10, 11, 12, 13, 14, 17, 21, 29, 30 provide codes

# pushq vector (exception num)

.global skern_exception_0
skern_exception_0:
    pushq $0
    pushq $0
    jmp skern_trap_entry

.global skern_exception_1
skern_exception_1:
    pushq $0
	pushq $1
	jmp skern_trap_entry

.global skern_exception_2
skern_exception_2:
    pushq $0
	pushq $2
    jmp skern_trap_entry

.global skern_exception_3
skern_exception_3:
    pushq $0
	pushq $3
    jmp skern_trap_entry

.global skern_exception_4
skern_exception_4:
    pushq $0
	pushq $4
    jmp skern_trap_entry

.global skern_exception_5
skern_exception_5:
    pushq $0
	pushq $5
    jmp skern_trap_entry

.global skern_exception_6
skern_exception_6:
    pushq $0
	pushq $6
    jmp skern_trap_entry

.global skern_exception_7
skern_exception_7:
    pushq $0
	pushq $7
    jmp skern_trap_entry

.global skern_exception_8
skern_exception_8:
    pushq $8
    jmp skern_trap_entry

.global skern_exception_9
skern_exception_9:
    pushq $0
    pushq $9
    jmp skern_trap_entry

.global skern_exception_10
skern_exception_10:
    pushq $10
    jmp skern_trap_entry

.global skern_exception_11
skern_exception_11:
    pushq $11
    jmp skern_trap_entry

.global skern_exception_12
skern_exception_12:
    pushq $12
    jmp skern_trap_entry

.global skern_exception_13
skern_exception_13:
    pushq $13
    jmp skern_trap_entry

.global skern_exception_14
skern_exception_14:
    pushq $14
    jmp skern_trap_entry

.global skern_exception_15
skern_exception_15:
    pushq $0
    pushq $15
    jmp skern_trap_entry

.global skern_exception_16
skern_exception_16:
    pushq $0
	pushq $16
    jmp skern_trap_entry

.global skern_exception_17
skern_exception_17:
    pushq $17
    jmp skern_trap_entry

.global skern_exception_18
skern_exception_18:
    pushq $0
	pushq $18
    jmp skern_trap_entry

.global skern_exception_19
skern_exception_19:
    pushq $0
	pushq $19
    jmp skern_trap_entry

.global skern_exception_20
skern_exception_20:
    pushq $0
	pushq $20
    jmp skern_trap_entry

.global skern_exception_21
skern_exception_21:
	pushq $21
    jmp skern_trap_entry

.global skern_exception_22
skern_exception_22:
    pushq $0
    pushq $22
    jmp skern_trap_entry

.global skern_exception_23
skern_exception_23:
    pushq $0
    pushq $23
    jmp skern_trap_entry

.global skern_exception_24
skern_exception_24:
    pushq $0
    pushq $24
    jmp skern_trap_entry

.global skern_exception_25
skern_exception_25:
    pushq $0
    pushq $25
    jmp skern_trap_entry

.global skern_exception_26
skern_exception_26:
    pushq $0
    pushq $26
    jmp skern_trap_entry

.global skern_exception_27
skern_exception_27:
    pushq $0
    pushq $27
    jmp skern_trap_entry

.global skern_exception_28
skern_exception_28:
    pushq $0
	pushq $28
    jmp skern_trap_entry

.global skern_exception_29
skern_exception_29:
	pushq $29
    jmp skern_trap_entry

.global skern_exception_30
skern_exception_30:
	pushq $30
    jmp skern_trap_entry

.global skern_exception_31
skern_exception_31:
    pushq $0
    pushq $31
    jmp skern_trap_entry

.section .note.GNU-stack,"",@progbits
