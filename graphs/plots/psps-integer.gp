set term postscript color eps enhanced 22
set output "psps-integer.eps"

set size 0.95,1.12

S1=0.005
S2=0.3
X=0.1+S1
W=0.26
M=0.025

load "styles.inc"

set tmargin 0
set bmargin 3

set multiplot layout 2,3

unset key

set grid ytics

set xtics (1, 2, 4, 8, "" 16, 32, "" 64, 128, "" 256) nomirror out offset -0.25,0.5
set label at screen 0.5,0.04 center "Number of swaps per transaction"
set label at screen 0.5,1.09 center "Persistent SPS integer swap"

set logscale x
set xrange [1:256]

# First row

set lmargin at screen X
set rmargin at screen X+W

set ylabel offset 1.5-S2,0 "Swaps ({/Symbol \264}10^6/s)"
set ytics offset 0.5,0
set yrange [0:14]

set label at graph 0.5,1.075 center font "Helvetica-bold,18" "1 thread"

plot \
	'../data/psps-integer.txt'       using 1:($2 /1e6) with linespoints notitle ls 1 lw 3 dt 1,    \
    '../data/psps-integer.txt'       using 1:($10/1e6) with linespoints notitle ls 2 lw 3 dt 1,    \
    '../data/psps-integer.txt'       using 1:($18/1e6) with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/psps-integer.txt'       using 1:($26/1e6) with linespoints notitle ls 4 lw 3 dt (1,1), \
    '../data/psps-integer-atlas.txt' using 1:($2 /1e6) with linespoints notitle ls 5 lw 3 dt (1,1)

unset ylabel
set ytics format ""

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W

unset label
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "2 threads"

plot \
    '../data/psps-integer.txt'       using 1:($3 /1e6) with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/psps-integer.txt'       using 1:($11/1e6) with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/psps-integer.txt'       using 1:($19/1e6) with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/psps-integer.txt'       using 1:($27/1e6) with linespoints notitle ls 4 lw 3 dt (1,1), \
    '../data/psps-integer-atlas.txt' using 1:($3 /1e6) with linespoints notitle ls 5 lw 3 dt (1,1)

set lmargin at screen X+2*(W+M)
set rmargin at screen X+2*(W+M)+W

unset label
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "4 threads"

plot \
    '../data/psps-integer.txt'       using 1:($4 /1e6) with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/psps-integer.txt'       using 1:($12/1e6) with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/psps-integer.txt'       using 1:($20/1e6) with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/psps-integer.txt'       using 1:($28/1e6) with linespoints notitle ls 4 lw 3 dt (1,1), \
    '../data/psps-integer-atlas.txt' using 1:($4 /1e6) with linespoints notitle ls 5 lw 3 dt (1,1)

# Second row

set lmargin at screen X
set rmargin at screen X+W

set ylabel offset 0.5-S2,0 "Swaps ({/Symbol \264}10^6/s)"
set ytics offset 0.5,0
set ytics format "%g"
set yrange [0:11]

unset label
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "16 threads"

plot \
    '../data/psps-integer.txt'       using 1:($6 /1e6) with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/psps-integer.txt'       using 1:($14/1e6) with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/psps-integer.txt'       using 1:($22/1e6) with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/psps-integer.txt'       using 1:($30/1e6) with linespoints notitle ls 4 lw 3 dt (1,1), \
    '../data/psps-integer-atlas.txt' using 1:($6 /1e6) with linespoints notitle ls 5 lw 3 dt (1,1)

unset ylabel
set ytics format ""

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W

unset label
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "32 threads"

plot \
    '../data/psps-integer.txt'       using 1:($7 /1e6) with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/psps-integer.txt'       using 1:($15/1e6) with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/psps-integer.txt'       using 1:($23/1e6) with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/psps-integer.txt'       using 1:($31/1e6) with linespoints notitle ls 4 lw 3 dt (1,1), \
    '../data/psps-integer-atlas.txt' using 1:($7 /1e6) with linespoints notitle ls 5 lw 3 dt (1,1)

set lmargin at screen X+2*(W+M)
set rmargin at screen X+2*(W+M)+W

unset label
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "64 threads"

plot \
    '../data/psps-integer.txt'       using 1:($9 /1e6) with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/psps-integer.txt'       using 1:($17/1e6) with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/psps-integer.txt'       using 1:($25/1e6) with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/psps-integer.txt'       using 1:($33/1e6) with linespoints notitle ls 4 lw 3 dt (1,1)#, \
#    '../data/psps-integer-atlas.txt' using 1:($9 /1e6) with linespoints notitle ls 5 lw 3 dt (1,1)
