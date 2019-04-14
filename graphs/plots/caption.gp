set term postscript color eps enhanced 22
set output "caption.eps"

set size 0.95,0.15

load "styles.inc"

unset tics
unset border
unset xlabel
unset ylabel
unset label

set object 1 rectangle from screen 0.02,0.01 to screen 0.925,0.14 fillcolor rgb "white" dashtype (2,3) behind
set label at screen 0.5,0.11 center "{/Helvetica-bold Legend for volatile memory} (all graphs of {\247}V.A)"

set key at screen 0.9,0.07 samplen 1.5 maxrows 2 width 0.25
plot [][0:1] \
    2 with linespoints title 'OF-LF'        ls 1 lw 3 dt 1, \
    2 with linespoints title 'OF-WF'        ls 2 lw 3 dt 1, \
    2 with linespoints title 'ESTM'         ls 3 lw 3 dt (1,1), \
    2 with linespoints title 'Tiny'         ls 5 lw 3 dt (1,1)
