jmpi main
hello:
; set mar to sp
ptrstack 0
; write 'H' to stack
li ax, 72
memwrite1 ax
addi mar, 1
; write 'e' to stack
li ax, 101
memwrite1 ax
addi mar, 1
; write 'l' to stack
li ax, 108
memwrite1 ax
addi mar, 1
; write 'l' to stack
li ax, 108
memwrite1 ax
addi mar, 1
; write 'o' to stack
li ax, 111
memwrite1 ax
addi mar, 1
; write '\n' to stack
li ax, 10
memwrite1 ax
addi mar, 1
;reset mar to point to the beginning of stack, i.e. our string
ptrstack 0
; syscall nr
li ax, 1
; fd 1: stdout
li bx, 1
; buf
mr cx, mar
; number of bytes to write
li dx, 6
callscr ax
ret
main:
li ax, hello
call ax
; SYS_exit
li ax, 60
li bx 0
callscr ax
