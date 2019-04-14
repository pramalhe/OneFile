set term postscript color eps enhanced 22
set output "stress-multi-process-q.eps"

set size 0.95,0.6

S=0.2125
X=0.1
W=0.375
M=0.075

load "styles.inc"

set tmargin 10.5
set bmargin 3

# We can fit a second graph if need be (remove S "hack")
set multiplot layout 1,2

unset key

set grid ytics

set xtics (2, 4, 8, 16, 32) nomirror out offset -0.25,0.5
set label at screen 0.5,0.04 center "Number of processes"

set logscale x
set xrange [2:32]

# First row

set lmargin at screen S+X
set rmargin at screen S+X+W

set ylabel offset 1.5,0 "Transactions ({/Symbol \264}10^6/s)"
set ytics 1 offset 0.5,0
set yrange [0:2]

set label at graph 0.5,1.075 center font "Helvetica-bold,18" "Swapping item from q to q"
set key at graph 0.99,0.99 samplen 1.5

# set label at graph 0.14,0.89 center font "Helvetica,18" "FHMP"
# set arrow from graph 0.14,0.84 to graph 0.14,0.76 size screen 0.015,25 lw 3

# we divide by 1e8 because the data is in total number of tx and we want tx/second
plot \
    '../data/stress-multi-process-q-nokills.txt' using 1:($2/1e8) with linespoints title "No kill"            ls 1 lw 2 dt (1,1), \
    '../data/stress-multi-process-q-kills.txt'   using 1:($2/1e8) with linespoints title "1 kill every 100ms" ls 2 lw 2 dt 1
