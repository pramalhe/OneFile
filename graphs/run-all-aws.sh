echo "Run this on a C5 AWS instance (c5.9xlarge) to generate all the data files for the plots"
export PMEM_IS_PMEM_FORCE=1
make persistencyclean
bin/pq-ll-enq-deq
make persistencyclean
bin/pset-hash-1k
make persistencyclean
bin/pset-ll-1k
make persistencyclean
bin/pset-tree-1k
make persistencyclean
bin/psps-integer
make persistencyclean
bin/q-array-enq-deq
bin/q-array-enq-deq-tiny
bin/q-ll-enq-deq
bin/q-ll-enq-deq-tiny
bin/set-hash-1k
bin/set-hash-1k-tiny
bin/set-ll-1k
bin/set-ll-1k-tiny
bin/set-tree-1k
bin/set-tree-1k-tiny
bin/set-tree-10k
bin/set-tree-10k-tiny
bin/set-tree-1m
bin/set-tree-1m-tiny
bin/sps-integer
bin/sps-integer-tiny
#bin/sps-object
#bin/sps-object-tiny

make persistencyclean
bin/pset-tree-1m-romlog
make persistencyclean
bin/pset-tree-1m-romlr
make persistencyclean
bin/pset-tree-1m-oflf
make persistencyclean
bin/pset-tree-1m-ofwf
make persistencyclean
bin/pset-tree-1m-pmdk


