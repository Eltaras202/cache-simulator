    li $t0, 0            # i = 0
    li $t4, 256          # loop until i == 256
    la $t1, array        # base address of the array

loop_seq:
    beq $t0, $t4, end_seq     # if i == 256, exit loop

    sll $t6, $t0, 2           # offset = i * 4 (word addressing)
    add $t7, $t1, $t6         # effective address = base + offset

    lw  $t8, 0($t7)           # load word from array[i]
    addi $t8, $t8, 1          # increment the value
    sw  $t8, 0($t7)           # store it back to array[i]

    addi $t0, $t0, 1          # i++

    j loop_seq               # repeat loop

end_seq:
    li $v0, 10                # syscall to terminate
    syscall

.data
array: .space 1024            # reserve 1024 bytes (256 words)
