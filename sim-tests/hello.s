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
;move stack pointer past our buffer
addi sp, 8
; push number of bytes to write onto stack
li ax, 6
push ax
; push address of buffer
push mar
; push 1 (fd number of stdout)
li ax, 1
push ax
; put syscall number 1 (write) into ax
li ax, 1
; do the syscall
callscr ax
; restore stackptr to prev value so return addr is correct
mr sp, mar
; return
ret
main:
li ax, hello
call ax
; push 0 onto stack
li ax, 0
push ax
; SYS_exit
li ax, 60
callscr ax
