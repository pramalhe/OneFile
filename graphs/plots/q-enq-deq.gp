set term postscript color eps enhanced 22
set output "q-enq-deq.eps"

set size 0.95,0.6

X=0.1
W=0.375
M=0.075

load "styles.inc"

set tmargin 10.5
set bmargin 3

set multiplot layout 1,2

unset key

set grid ytics

set xtics (" 1" 1, 2, 4, 8, 16, "32 " 32, 64) nomirror out offset -0.25,0.5
set label at screen 0.5,0.04 center "Number of threads"

set logscale x
set xrange [1:64]

# First row

set lmargin at screen X
set rmargin at screen X+W

set ylabel offset 1.5,0 "Operations ({/Symbol \264}10^6/s)"
set ytics 4 offset 0.5,0
set yrange [0:15]

set label at graph 0.5,1.075 center font "Helvetica-bold,18" "Linked list-based queue"
set key at graph 0.99,0.92 samplen 1.5

# set label at graph 0.12,0.77 center font "Helvetica,18" "MS"
# set arrow from graph 0.12,0.82 to graph 0.12,0.90 size screen 0.015,25 lw 3
# set label at graph 0.55,0.72 left font "Helvetica,18" "SimQ"
# set arrow from graph 0.53,0.72 to graph 0.47,0.72 size screen 0.015,25 lw 3
# set label at graph 0.88,0.60 center font "Helvetica,18" "TurnQ"
# set arrow from graph 0.88,0.55 to graph 0.88,0.47 size screen 0.015,25 lw 3

plot \
    '../data/q-ll-enq-deq.txt'      using 1:($2/1e6) with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/q-ll-enq-deq.txt'      using 1:($3/1e6) with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/q-ll-enq-deq.txt'      using 1:($4/1e6) with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/q-ll-enq-deq-tiny.txt' using 1:($2/1e6) with linespoints notitle ls 5 lw 3 dt (1,1), \
    '../data/q-ll-enq-deq.txt'      using 1:($5/1e6) with linespoints title "MS" ls 6 lw 3 dt (1,1), \
    '../data/q-ll-enq-deq.txt'      using 1:($6/1e6) with linespoints title "SimQ" ls 7 lw 3 dt (1,1), \
    '../data/q-ll-enq-deq.txt'      using 1:($7/1e6) with linespoints title "TurnQ" ls 8 lw 3 dt (1,1)

unset ylabel
set ytics 10 offset 0.5,0
set yrange [0:40]

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W

unset label
unset arrow
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "Array-based queue"
set key at graph 0.99,0.35 samplen 1.5

# set label at graph 0.55,0.61 left font "Helvetica,18" "FAA"
# set arrow from graph 0.53,0.61 to graph 0.47,0.61 size screen 0.015,25 lw 3
# set label at graph 0.37,0.76 right font "Helvetica,18" "LCRQ"
# set arrow from graph 0.39,0.76 to graph 0.45,0.76 size screen 0.015,25 lw 3

plot \
    '../data/q-array-enq-deq.txt'     using 1:($2/1e6) with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/q-array-enq-deq.txt'     using 1:($3/1e6) with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/q-array-enq-deq.txt'     using 1:($4/1e6) with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/q-array-enq-deq.txt'     using 1:($5/1e6) with linespoints title "FAA" ls 6 lw 3 dt (1,1), \
    '../data/q-array-enq-deq.txt'     using 1:($6/1e6) with linespoints title "LCRQ" ls 7 lw 3 dt (1,1)
