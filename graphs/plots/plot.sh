#!/bin/sh

while [ $# -gt 0 ]
do
  echo "Processing:" $1
  gnuplot $1
  epstopdf `basename $1 .gp`.eps
  rm `basename $1 .gp`.eps
  open `basename $1 .gp`.pdf
  shift
done
