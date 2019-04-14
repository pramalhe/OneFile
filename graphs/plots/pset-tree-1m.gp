set term postscript color eps enhanced 22
set output "pset-tree-1m.eps"

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

set xtics ("" 1, "" 2, 4, "" 8, 16, 32, 48, 64) nomirror out offset -0.25,0.5
set label at screen 0.5,0.04 center "Number of threads"
set label at screen 0.5,1.09 center "Persistent red-black tree sets with 10^{6} keys"

set xrange [1:64]

# First row

set lmargin at screen X
set rmargin at screen X+W

set ylabel offset 1.5,0 "Operations ({/Symbol \264}10^6/s)"
set ytics 0.1 offset 0.5,0
set format y "%g"
set yrange [0:0.3]

set label at graph 0.5,1.075 center font "Helvetica-bold,18" "100%"

set key at graph 0.99,0.99 samplen 1.5

plot \
    '../data/pset-tree-1m-oflf.txt'   using 1:($2/1e6) with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/pset-tree-1m-ofwf.txt'   using 1:($2/1e6) with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/pset-tree-1m-romlog.txt' using 1:($2/1e6) with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/pset-tree-1m-romlr.txt'  using 1:($2/1e6) with linespoints notitle ls 4 lw 3 dt (1,1), \
    '../data/pset-tree-1m-pmdk.txt'   using 1:($2/1e6) with linespoints notitle ls 5 lw 3 dt (1,1)

unset ylabel
set ytics format ""

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W

unset label
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "50%"

plot \
    '../data/pset-tree-1m-oflf.txt'   using 1:($3/1e6) with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/pset-tree-1m-ofwf.txt'   using 1:($3/1e6) with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/pset-tree-1m-romlog.txt' using 1:($3/1e6) with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/pset-tree-1m-romlr.txt'  using 1:($3/1e6) with linespoints notitle ls 4 lw 3 dt (1,1), \
    '../data/pset-tree-1m-pmdk.txt'   using 1:($3/1e6) with linespoints notitle ls 5 lw 3 dt (1,1)

set lmargin at screen X+2*(W+M)
set rmargin at screen X+2*(W+M)+W

unset label
set ytics 0.2 offset 0.5,0
set yrange [0:1.4]
set style textbox opaque noborder fillcolor rgb "white"
set label at first 1,1.4 front boxed left offset -0.5,0 "1.4"
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "10%"

plot \
    '../data/pset-tree-1m-oflf.txt'   using 1:($4/1e6) with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/pset-tree-1m-ofwf.txt'   using 1:($4/1e6) with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/pset-tree-1m-romlog.txt' using 1:($4/1e6) with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/pset-tree-1m-romlr.txt'  using 1:($4/1e6) with linespoints notitle ls 4 lw 3 dt (1,1), \
    '../data/pset-tree-1m-pmdk.txt'   using 1:($4/1e6) with linespoints notitle ls 5 lw 3 dt (1,1)


# Second row

set lmargin at screen X
set rmargin at screen X+W

set ylabel offset 0.5,0 "Operations ({/Symbol \264}10^6/s)"
set ytics 5 offset 0.5,0
set format y "%g"
set yrange [0:12]

unset label
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "1%"

plot \
    '../data/pset-tree-1m-oflf.txt'   using 1:($5/1e6) with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/pset-tree-1m-ofwf.txt'   using 1:($5/1e6) with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/pset-tree-1m-romlog.txt' using 1:($5/1e6) with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/pset-tree-1m-romlr.txt'  using 1:($5/1e6) with linespoints notitle ls 4 lw 3 dt (1,1), \
    '../data/pset-tree-1m-pmdk.txt'   using 1:($5/1e6) with linespoints notitle ls 5 lw 3 dt (1,1)

unset ylabel
set ytics format ""

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W

unset label
set ytics 5 offset 0.5,0
set yrange [0:25]
set style textbox opaque noborder fillcolor rgb "white"
set label at first 1,25 front boxed left offset -0.5,0 "25"
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "0.1%"

plot \
    '../data/pset-tree-1m-oflf.txt'   using 1:($6/1e6) with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/pset-tree-1m-ofwf.txt'   using 1:($6/1e6) with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/pset-tree-1m-romlog.txt' using 1:($6/1e6) with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/pset-tree-1m-romlr.txt'  using 1:($6/1e6) with linespoints notitle ls 4 lw 3 dt (1,1), \
    '../data/pset-tree-1m-pmdk.txt'   using 1:($6/1e6) with linespoints notitle ls 5 lw 3 dt (1,1)

set lmargin at screen X+2*(W+M)
set rmargin at screen X+2*(W+M)+W

unset label
set ytics 5 offset 0.5,0
set yrange [0:38]
set style textbox opaque noborder fillcolor rgb "white"
set label at first 1,38 front boxed left offset -0.5,0 "38"
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "0%"

plot \
    '../data/pset-tree-1m-oflf.txt'   using 1:($7/1e6) with linespoints notitle ls 1 lw 3 dt 1,     \
    '../data/pset-tree-1m-ofwf.txt'   using 1:($7/1e6) with linespoints notitle ls 2 lw 3 dt 1,     \
    '../data/pset-tree-1m-romlog.txt' using 1:($7/1e6) with linespoints notitle ls 3 lw 3 dt (1,1), \
    '../data/pset-tree-1m-romlr.txt'  using 1:($7/1e6) with linespoints notitle ls 4 lw 3 dt (1,1), \
    '../data/pset-tree-1m-pmdk.txt'   using 1:($7/1e6) with linespoints notitle ls 5 lw 3 dt (1,1)


unset tics
unset border
unset xlabel
unset ylabel
unset label

#set key at screen 0.92,0.20 samplen 2.0 bottom
#plot [][0:1] \
#    2 with linespoints title 'OF-LF'   ls 1, \
#    2 with linespoints title 'OF-WF'   ls 2, \
#    2 with linespoints title 'RomLog'  ls 3, \
#    2 with linespoints title 'RomLR'   ls 4, \
#    2 with linespoints title 'PMDK'    ls 5       
    