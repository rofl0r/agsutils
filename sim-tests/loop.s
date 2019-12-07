; loop 10 million times
; this is ~20x slower than native speed with -O3, and 4.5x slower than dino,
; but 3x faster than python 2.7.15
li cx, 10000000
li dx, 0
loop:
subi cx, 1
mr ax, cx
gt ax, dx
jnzi loop


