set term postscript color eps enhanced 22
set output "q-array-enq-deq.eps"

#set size 0.95,0.6
#X=0.09
#W=0.26
#M=0.02

load "styles.inc"

#set tmargin 11.0
#set bmargin 2.5

set grid ytics

set ylabel offset 0.7,0 "Op/s"
set format y "10^{%T}"
set xtics ("" 1, 2, 4, 8, 16, 32, 64) nomirror out offset -0.25,0.5
#set ytics 100 offset 0.5,0
#set mytics 10
set label at screen 0.5,0.03 center "Number of threads"

set logscale x
#set yrange [1:1e7]
#set lmargin at screen X
#set rmargin at screen X+W

set label at graph 1.6,1.1 center "Queues (array based)"

plot \
	'../data/q-array-enq-deq.txt'      using 1:2  with linespoints title 'OF-LF' ls 1 lw 2 dt 1,     \
    '../data/q-array-enq-deq.txt'      using 1:3  with linespoints title 'OF-WF' ls 2 lw 2 dt 1,     \
    '../data/q-array-enq-deq.txt'      using 1:4  with linespoints title 'ESTM'  ls 3 lw 2 dt (1,1), \
    '../data/q-array-enq-deq-tiny.txt' using 1:2  with linespoints title 'Tiny'  ls 5 lw 2 dt (1,1), \
    '../data/q-array-enq-deq.txt'      using 1:5  with linespoints title 'FAA'   ls 6 lw 2 dt (1,1), \
    '../data/q-array-enq-deq.txt'      using 1:6  with linespoints title 'LCRQ'  ls 7 lw 2 dt (1,1), \


