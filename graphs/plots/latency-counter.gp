set term postscript color eps enhanced 22
set output "latency-counter.eps"

set size 0.95,1.12

X=0.1
W=0.26
M=0.025

load "styles.inc"

set tmargin 0
set bmargin 3

set multiplot layout 2,3

unset key

set grid ytics

set xtics ("" 1, 2, 4, 8, 16, 32, 64) nomirror out offset -0.25,0.5
set label at screen 0.5,0.04 center "Number of threads"
set label at screen 0.5,1.09 center "Latency when incrementing an array of 64 counters"

set logscale x
set logscale y
set xrange [1:64]

# First row

set lmargin at screen X
set rmargin at screen X+W

set ylabel offset 0,0 "Tx time duration ({/Symbol m}s)"
set ytics offset 0.5,0
set ytics add ("" 1e1, "" 1e3, "" 1e5, "" 1e7)
set format y "10^{%T}"
set yrange [1e0:1e8]

set label at graph 0.2,1.075 font "Helvetica-bold,18" "50% (median)"

plot \
	'../data/latency-counter-10m-cervino.txt'      using 1:2  with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/latency-counter-10m-cervino.txt'      using 1:8  with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/latency-counter-10m-cervino.txt'      using 1:14 with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/latency-counter-10m-cervino-tiny.txt' using 1:2  with linespoints notitle ls 5 lw 3 dt (1,1)

unset ylabel
set ytics format ""

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W

unset label
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "90%"

plot \
    '../data/latency-counter-10m-cervino.txt'      using 1:3  with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/latency-counter-10m-cervino.txt'      using 1:9  with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/latency-counter-10m-cervino.txt'      using 1:15 with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/latency-counter-10m-cervino-tiny.txt' using 1:3  with linespoints notitle ls 5 lw 3 dt (1,1)

set lmargin at screen X+2*(W+M)
set rmargin at screen X+2*(W+M)+W

unset label
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "99%"

plot \
    '../data/latency-counter-10m-cervino.txt'      using 1:4  with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/latency-counter-10m-cervino.txt'      using 1:10 with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/latency-counter-10m-cervino.txt'      using 1:16 with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/latency-counter-10m-cervino-tiny.txt' using 1:4  with linespoints notitle ls 5 lw 3 dt (1,1)

# Second row

set lmargin at screen X
set rmargin at screen X+W

set ylabel offset 0,0 "Tx time duration ({/Symbol m}s)"
set ytics offset 0.5,0
set format y "10^{%T}"
set yrange [1e0:1e8]

unset label
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "99.9%"

plot \
    '../data/latency-counter-10m-cervino.txt'      using 1:5  with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/latency-counter-10m-cervino.txt'      using 1:11 with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/latency-counter-10m-cervino.txt'      using 1:17 with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/latency-counter-10m-cervino-tiny.txt' using 1:5  with linespoints notitle ls 5 lw 3 dt (1,1)

unset ylabel
set ytics format ""

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W

unset label
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "99.99%"

plot \
    '../data/latency-counter-10m-cervino.txt'      using 1:6  with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/latency-counter-10m-cervino.txt'      using 1:12 with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/latency-counter-10m-cervino.txt'      using 1:18 with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/latency-counter-10m-cervino-tiny.txt' using 1:6  with linespoints notitle ls 5 lw 3 dt (1,1)

set lmargin at screen X+2*(W+M)
set rmargin at screen X+2*(W+M)+W

unset label
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "99.999%"

plot \
    '../data/latency-counter-10m-cervino.txt'      using 1:7  with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/latency-counter-10m-cervino.txt'      using 1:13 with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/latency-counter-10m-cervino.txt'      using 1:19 with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/latency-counter-10m-cervino-tiny.txt' using 1:7  with linespoints notitle ls 5 lw 3 dt (1,1)

