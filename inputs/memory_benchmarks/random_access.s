.text
.globl main
main:

    # Load from 0x00000000
    ori   $t0, $zero, 0x0000
    lw    $v0, 0($t0)

    # Load from 0x00000040
    ori   $t0, $zero, 0x0040
    lw    $v0, 0($t0)

    # Load from 0x00000080
    ori   $t0, $zero, 0x0080
    lw    $v0, 0($t0)

    # Load from 0x00000000 again
    ori   $t0, $zero, 0x0000
    lw    $v0, 0($t0)

    # Load from 0x000000C0
    ori   $t0, $zero, 0x00C0
    lw    $v0, 0($t0)

    # Load from 0x00000040 again
    ori   $t0, $zero, 0x0040
    lw    $v0, 0($t0)

    # Load from 0x00000080 again
    ori   $t0, $zero, 0x0080
    lw    $v0, 0($t0)

    # Exit
    li    $v0, 10
    syscall
