; loop 10 million times
; this is 13x slower than native speed with -O3, and 3x slower than dino,
; but 4x faster than python 2.7.15
li cx, 10000000
li dx, 0
loop:
subi cx, 1
mr ax, cx
gt ax, dx
jnzi loop


