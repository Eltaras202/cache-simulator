.data
array: .space 4096    # 4096 bytes = 128 blocks if block size = 32B

.text
.globl main
main:

    li $t0, 0          # i = 0
    li $t1, 128        # number of blocks to access
    li $t2, 32         # block size in bytes (1 per cache block)

loop:
    la $t3, array      # base address
    mul_loop:          # compute offset = i * 32 (manual multiplication)
        move $t4, $zero
        move $t5, $t0
    mul_acc:
        beq $t5, $zero, mul_done
        add $t4, $t4, $t2
        addi $t5, $t5, -1
        j mul_acc
    mul_done:

    add $t6, $t3, $t4   # address = base + offset
    lw $t7, 0($t6)      # load from array[i]

    addi $t0, $t0, 1
    bne $t0, $t1, loop

    # Now re-access same 128 blocks to test reuse
    li $t0, 0

loop2:
    la $t3, array
    move $t5, $t0
    move $t4, $zero
mul2:
    beq $t5, $zero, mul2_done
    add $t4, $t4, $t2
    addi $t5, $t5, -1
    j mul2
mul2_done:

    add $t6, $t3, $t4
    lw $t7, 0($t6)

    addi $t0, $t0, 1
    bne $t0, $t1, loop2

    li $v0, 10
    syscall
