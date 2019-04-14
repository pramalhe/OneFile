#!/bin/sh

for i in \
caption.gp \
sps-integer.gp \
sps-object.gp \
q-enq-deq.gp \
set-ll-1k.gp \
set-hash-1k.gp \
set-tree-1k.gp \
set-tree-10k.gp \
latency-counter.gp \
pcaption.gp \
psps-integer.gp \
pset-ll-1k.gp \
pset-tree-1k.gp \
pset-hash-1k.gp \
pq-ll-enq-deq.gp \
;
do
  echo "Processing:" $i
  gnuplot $i
  epstopdf `basename $i .gp`.eps
  rm `basename $i .gp`.eps
done
