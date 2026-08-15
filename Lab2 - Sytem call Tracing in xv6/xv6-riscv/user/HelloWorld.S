.section .data
msg:
    .asciz "Hello CS23B006! Welcome to CS3500\n"

.section .text
.globl main
main:
    li a0, 1              
    la a1, msg            
    li a2, 34             
    li a7, 16            
    ecall

    # exit(0)
    li a0, 0
    li a7, 2             
    ecall