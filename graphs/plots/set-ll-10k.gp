set term postscript color eps enhanced 22
set output "set-ll-10k.eps"

set size 0.95,1.08

X=0.09
W=0.26
M=0.02

load "styles.inc"

#set tmargin 11.0
#set bmargin 2.5

set grid ytics

set ylabel offset 0.7,0 "Op/s"
set format y "10^{%T}"
set xtics ("" 1, 2, 4, 8, 16, 32) nomirror out offset -0.25,0.5
set ytics 100 offset 0.5,0
set mytics 10
set label at screen 0.5,0.03 center "Number of threads"
set label at screen 0.5,1.05 center "Linked List Sets with 10^{4} keys"

set multiplot layout 2,3

set logscale x
set xrange [1:32]

set lmargin at screen X
set rmargin at screen X+W

unset key
set label at graph 0.5,1.1 center "100%"

plot \
	'../data/set-ll-10k.txt'      using 1:2  with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/set-ll-10k.txt'      using 1:3  with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/set-ll-10k.txt'      using 1:4  with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/set-ll-10k-tiny.txt' using 1:2  with linespoints notitle ls 5 lw 3 dt (1,1), \
    '../data/set-ll-10k.txt'      using 1:5  with linespoints notitle ls 6 lw 3 dt (1,1), \
    '../data/set-ll-10k.txt'      using 1:6  with linespoints notitle ls 7 lw 3 dt (1,1)

unset ylabel
set ytics format ""

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W

unset label
set label at graph 0.5,1.1 center "50%"

plot \
    '../data/set-ll-10k.txt'      using 1:7  with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/set-ll-10k.txt'      using 1:8  with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/set-ll-10k.txt'      using 1:9  with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/set-ll-10k-tiny.txt' using 1:3  with linespoints notitle ls 5 lw 3 dt (1,1), \
    '../data/set-ll-10k.txt'      using 1:10 with linespoints notitle ls 6 lw 3 dt (1,1), \
    '../data/set-ll-10k.txt'      using 1:11 with linespoints notitle ls 7 lw 3 dt (1,1)

unset ylabel
set ytics format ""

set lmargin at screen X+2*(W+M)
set rmargin at screen X+2*(W+M)+W

unset label
set label at graph 0.5,1.1 center "10%"

plot \
    '../data/set-ll-10k.txt'      using 1:12 with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/set-ll-10k.txt'      using 1:13 with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/set-ll-10k.txt'      using 1:14 with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/set-ll-10k-tl2.txt'  using 1:4  with linespoints notitle ls 4 lw 3 dt (1,1), \
    '../data/set-ll-10k-tiny.txt' using 1:4  with linespoints notitle ls 5 lw 3 dt (1,1), \
    '../data/set-ll-10k.txt'      using 1:15 with linespoints notitle ls 6 lw 3 dt (1,1), \
    '../data/set-ll-10k.txt'      using 1:16 with linespoints notitle ls 7 lw 3 dt (1,1)


    
    
# Second row


set logscale x
#set yrange [1:1e7]
set lmargin at screen X
set rmargin at screen X+W

unset label
set label at graph 0.5,1.1 center "1%"

plot \
    '../data/set-ll-10k.txt'      using 1:17 with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/set-ll-10k.txt'      using 1:18 with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/set-ll-10k.txt'      using 1:19 with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/set-ll-10k-tiny.txt' using 1:5  with linespoints notitle ls 5 lw 3 dt (1,1), \
    '../data/set-ll-10k.txt'      using 1:20 with linespoints notitle ls 6 lw 3 dt (1,1), \
    '../data/set-ll-10k.txt'      using 1:21 with linespoints notitle ls 7 lw 3 dt (1,1)

unset ylabel
set ytics format ""

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W

unset label
set label at graph 0.5,1.1 center "0.1%"

plot \
    '../data/set-ll-10k.txt'      using 1:22 with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/set-ll-10k.txt'      using 1:23 with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/set-ll-10k.txt'      using 1:24 with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/set-ll-10k-tiny.txt' using 1:6  with linespoints notitle ls 5 lw 3 dt (1,1), \
    '../data/set-ll-10k.txt'      using 1:25 with linespoints notitle ls 6 lw 3 dt (1,1), \
    '../data/set-ll-10k.txt'      using 1:26 with linespoints notitle ls 7 lw 3 dt (1,1)

    
unset ylabel
set ytics format ""

set lmargin at screen X+2*(W+M)
set rmargin at screen X+2*(W+M)+W

unset label
set label at graph 0.5,1.1 center "0%"

plot \
    '../data/set-ll-10k.txt'      using 1:27 with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/set-ll-10k.txt'      using 1:28 with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/set-ll-10k.txt'      using 1:29 with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/set-ll-10k-tiny.txt' using 1:7  with linespoints notitle ls 5 lw 3 dt (1,1), \
    '../data/set-ll-10k.txt'      using 1:30 with linespoints notitle ls 6 lw 3 dt (1,1), \
    '../data/set-ll-10k.txt'      using 1:31 with linespoints notitle ls 7 lw 3 dt (1,1)



unset tics
unset border
unset xlabel
unset ylabel
unset label

set key at screen 0.62,0.22 samplen 2.0 bottom
plot [][0:1] \
    2 with linespoints title 'OF-LF'        ls 1, \
    2 with linespoints title 'OF-WF'        ls 2, \
    2 with linespoints title 'ESTM'         ls 3, \
    2 with linespoints title 'Tiny'         ls 5

set key at screen 0.88,0.28 samplen 2.0 bottom
plot [][0:1] \
    2 with linespoints title 'HarrisHP'     ls 6, \
    2 with linespoints title 'HarrisHE'     ls 7


unset multiplot
