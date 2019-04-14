Every .cpp file here outputs a plot taken from a benchmark.
The corresponding tab-separated values are placed in the data/ folder and can then be read by gnuplot to make the actual plots.
The .gp and corresponding pdf files are in the plots/ folder.
To generate the plots, type:
cd plots
./plot-all.sh
