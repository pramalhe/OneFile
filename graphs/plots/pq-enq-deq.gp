set term postscript color eps enhanced 22
set output "pq-enq-deq.eps"

set size 0.95,0.6

X=0.08
W=0.370
M=0.100

load "styles.inc"

set tmargin 10.5
set bmargin 3

set multiplot layout 1,2

unset key

set grid ytics

set xtics (" 1" 1, 2, 4, 8, 16, "32 " 32, 64) nomirror out offset -0.25,0.5
set label at screen 0.25,0.04 center "Number of threads"

set logscale x
set xrange [1:64]

# First row

set lmargin at screen X
set rmargin at screen X+W

set ylabel offset 1.5,0 "Operations ({/Symbol \264}10^6/s)"
set ytics 1 offset 0.5,0
set yrange [0:4]

set label at graph 0.5,1.075 center font "Helvetica-bold,18" "Linked list-based queue"
set key at graph 0.50,0.99 samplen 1.5

plot \
    '../data/pq-ll-enq-deq.txt' using 1:($2/1e6) with linespoints notitle ls 1 lw 2 dt 1,     \
    '../data/pq-ll-enq-deq.txt' using 1:($3/1e6) with linespoints notitle ls 2 lw 2 dt 1,     \
    '../data/pq-ll-enq-deq.txt' using 1:($4/1e6) with linespoints notitle ls 3 lw 2 dt (1,1), \
    '../data/pq-ll-enq-deq.txt' using 1:($5/1e6) with linespoints notitle ls 4 lw 2 dt (1,1), \
    '../data/pq-ll-enq-deq.txt' using 1:($6/1e6) with linespoints notitle ls 5 lw 2 dt (1,1), \
    '../data/pq-ll-enq-deq.txt' using 1:($7/1e6) with linespoints title "FHMP" ls 6 lw 2 dt (1,1)

unset ylabel
set ytics 10 offset 0.5,0
set yrange [0:40]

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W

unset label
unset arrow
set xtics (2, 4, 8, 16, 32) nomirror out offset -0.25,0.5
set ylabel offset 1.5,0 "Transactions ({/Symbol \264}10^6/s)"
set label at screen 0.75,0.04 center "Number of processes"

set ytics ("" 0.5,1,"" 1.5,2) offset 0.5,0
set xrange [2:32]
set yrange [0:2]

set label at graph 0.5,1.075 center font "Helvetica-bold,18" "Swapping item between queues"
set key at graph 0.99,0.99 samplen 1.5

# we divide by 1e8 because the data is in total number of tx and we want tx/second
plot \
    '../data/stress-multi-process-q-nokills.txt' using 1:($2/1e8) with linespoints title "No kill"            ls 1 lw 4 dt (1,1), \
    '../data/stress-multi-process-q-kills.txt'   using 1:($2/1e8) with linespoints title "1 kill every 100ms" ls 2 lw 2 dt 1
