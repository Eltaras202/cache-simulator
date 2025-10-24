#!/usr/bin/env python3

def generate_random_access():
    """Generate machine code for random access benchmark"""
    code = []
    
    # li $t0, 0              # i = 0
    code.append("34080000")  # ori $t0, $zero, 0
    
    # la $t1, array          # base address of array
    code.append("3c091001")  # lui $t1, 0x1001
    code.append("35290000")  # ori $t1, $t1, 0x0000
    
    # lw $t4, size           # array size
    code.append("8d2c0000")  # lw $t4, 0($t1)
    
    # li $t5, 0x12345678     # seed for pseudo-random
    code.append("3c0d1234")  # lui $t5, 0x1234
    code.append("35ad5678")  # ori $t5, $t5, 0x5678
    
    # beq $t0, $t4, end      # if i == 8192, end loop
    code.append("110c0018")  # beq $t0, $t4, end (offset 18)
    
    # li $t6, 1103515245
    code.append("3c0e41c6")  # lui $t6, 0x41c6
    code.append("35ce6b6d")  # ori $t6, $t6, 0x6b6d
    
    # mult $t5, $t6
    code.append("01a60018")  # mult $t5, $t6
    
    # mflo $t7
    code.append("00003812")  # mflo $t7
    
    # addi $t7, $t7, 12345
    code.append("21e73039")  # addi $t7, $t7, 12345
    
    # andi $t5, $t7, 0x7fffffff
    code.append("31e57fff")  # andi $t5, $t7, 0x7fff
    
    # andi $t6, $t5, 0x1fff  # index = random & 0x1fff
    code.append("31a61fff")  # andi $t6, $t5, 0x1fff
    
    # sll $t7, $t6, 2        # offset = index * 4
    code.append("00067080")  # sll $t7, $t6, 2
    
    # add $t8, $t1, $t7      # addr = array + offset
    code.append("0127c020")  # add $t8, $t1, $t7
    
    # sw $t0, 0($t8)         # store i at array[random_index]
    code.append("ad880000")  # sw $t0, 0($t8)
    
    # addi $t0, $t0, 1       # i++
    code.append("21080001")  # addi $t0, $t0, 1
    
    # j loop
    code.append("08000002")  # j loop
    
    # end: li $v0, 10
    code.append("3402000a")  # ori $v0, $zero, 10
    
    # syscall
    code.append("0000000c")  # syscall
    
    return code

def generate_matrix_transpose():
    """Generate machine code for matrix transpose benchmark"""
    code = []
    
    # li $t0, 0              # i = 0
    code.append("34080000")  # ori $t0, $zero, 0
    
    # li $t1, 0              # j = 0
    code.append("34090000")  # ori $t1, $zero, 0
    
    # la $t2, matrix         # base address of matrix
    code.append("3c0a1001")  # lui $t2, 0x1001
    code.append("354a0000")  # ori $t2, $t2, 0x0000
    
    # lw $t3, size           # matrix size
    code.append("8d4b0000")  # lw $t3, 0($t2)
    
    # outer_loop: beq $t0, $t3, end
    code.append("110b0020")  # beq $t0, $t3, end (offset 32)
    
    # inner_loop: beq $t1, $t3, next_row
    code.append("112b0018")  # beq $t1, $t3, next_row (offset 24)
    
    # mul $t4, $t0, $t3      # i * size
    code.append("010b0018")  # mult $t0, $t3
    code.append("00006012")  # mflo $t4
    
    # add $t4, $t4, $t1      # i * size + j
    code.append("01896020")  # add $t4, $t4, $t1
    
    # sll $t4, $t4, 2        # (i * size + j) * 4
    code.append("00047080")  # sll $t4, $t4, 2
    
    # add $t4, $t2, $t4      # matrix + offset
    code.append("01447020")  # add $t4, $t2, $t4
    
    # mul $t5, $t1, $t3      # j * size
    code.append("012b0018")  # mult $t1, $t3
    code.append("00006812")  # mflo $t5
    
    # add $t5, $t5, $t0      # j * size + i
    code.append("01a86820")  # add $t5, $t5, $t0
    
    # sll $t5, $t5, 2        # (j * size + i) * 4
    code.append("00057080")  # sll $t5, $t5, 2
    
    # add $t5, $t2, $t5      # matrix + offset
    code.append("01456820")  # add $t5, $t2, $t5
    
    # lw $t6, 0($t4)         # load matrix[i][j]
    code.append("8d860000")  # lw $t6, 0($t4)
    
    # sw $t6, 0($t5)         # store to matrix[j][i]
    code.append("ada60000")  # sw $t6, 0($t5)
    
    # addi $t1, $t1, 1       # j++
    code.append("21290001")  # addi $t1, $t1, 1
    
    # j inner_loop
    code.append("08000004")  # j inner_loop
    
    # next_row: li $t1, 0    # j = 0
    code.append("34090000")  # ori $t1, $zero, 0
    
    # addi $t0, $t0, 1       # i++
    code.append("21080001")  # addi $t0, $t0, 1
    
    # j outer_loop
    code.append("08000003")  # j outer_loop
    
    # end: li $v0, 10
    code.append("3402000a")  # ori $v0, $zero, 10
    
    # syscall
    code.append("0000000c")  # syscall
    
    return code

if __name__ == "__main__":
    print("Random Access Benchmark:")
    random_code = generate_random_access()
    for i, instr in enumerate(random_code):
        print(f"{instr}  # instruction {i}")
    
    print("\nMatrix Transpose Benchmark:")
    transpose_code = generate_matrix_transpose()
    for i, instr in enumerate(transpose_code):
        print(f"{instr}  # instruction {i}") 