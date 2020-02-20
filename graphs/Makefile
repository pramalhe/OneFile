CXX = g++-8
CXXFLAGS = -std=c++17 -g -O2 -DPWB_IS_CLFLUSHOPT # -fuse-ld=gold -fsanitize=address
# For castor-1
#CXXFLAGS = -std=c++17 -g -O2 -DPWB_IS_CLWB -DPM_REGION_SIZE=64*1024*1024*1024ULL -DPM_USE_DAX -DPM_FILE_NAME="\"/mnt/pmem0/durable\""

# Possible options for PWB are:
# -DPWB_IS_CLFLUSH		pwb is a CLFLUSH and pfence/psync are nops      (Broadwell)
# -DPWB_IS_CLFLUSHOPT	pwb is a CLFLUSHOPT and pfence/psync are SFENCE (Kaby Lake) 
# -DPWB_IS_CLWB			pwb is a CLWB and pfence/psync are SFENCE       (Sky Lake SP, or Canon Lake SP and beyond)
# -DPWB_IS_NOP			pwb/pfence/psync are nops. Used for shared memory persistence


INCLUDES = -I../ -I../common/ 
#LIBS = -l/home/vagrant/tinystm/lib/libstm.a

# This library is needed for ESTM
ESTM_LIB = -L../stms/estm-0.3.0/lib/ -lstm -lpthread

# This library is needed for TinySTM
TINYSTM_LIB = -L../stms/tinystm/lib/ -lstm -lpthread
TINYSTM_INC = -I../stms/tinystm/

# This library is needed for PMDK
PMDKLIBS = -L/usr/local/lib -lpmemobj


BINARIES = \
	bin/sps-integer \
	bin/sps-integer-tiny \
	bin/sps-object \
	bin/sps-object-tiny \
	bin/set-ll-1k \
	bin/set-ll-1k-tiny \
	bin/set-ll-10k \
	bin/set-ll-10k-tiny \
	bin/set-tree-1k \
	bin/set-tree-1k-tiny \
	bin/set-tree-10k \
	bin/set-tree-10k-tiny \
	bin/set-tree-1m \
	bin/set-tree-1m-tiny \
	bin/set-hash-1k \
	bin/set-hash-1k-tiny \
	bin/q-ll-enq-deq \
	bin/q-ll-enq-deq-tiny \
	bin/q-array-enq-deq \
	bin/q-array-enq-deq-tiny \
	bin/psps-integer \
	bin/pset-ll-1k \
	bin/pset-ll-10k \
	bin/pset-hash-1k \
	bin/pset-tree-1k \
	bin/pq-ll-enq-deq \
	bin/latency-counter \
	bin/latency-counter-tiny \
    bin/pset-tree-1m-oflf \
    bin/pset-tree-1m-ofwf \
    bin/pset-tree-1m-pmdk \
	bin/pset-tree-1m-romlog \
	bin/pset-tree-1m-romlr \
#	bin/pset-tree-1m-pmdk \
#	bin/pread-while-writing-romlog \
	bin/pread-while-writing-romlr \
	bin/pread-while-writing-oflf \
	bin/pread-while-writing-ofwf \
	bin/pread-while-writing-pmdk \

STMS = \
	../stms/CRWWPSTM.hpp \
	../stms/OneFileLF.hpp \
	../stms/OneFileWF.hpp \
	../stms/TinySTM.hpp \
	../stms/tinystm/lib/libstm.a \

PTMS = \
	../ptms/OneFilePTMLF.hpp \
	../ptms/OneFilePTMWF.hpp \
	../ptms/PMDKTM.hpp \
	lib/libromulus.a \
    ../ptms/romuluslog/RomulusLog.hpp \
    ../ptms/romuluslr/RomulusLR.hpp \

SRC_LISTS = \
	../datastructures/linkedlists/CRWWPLinkedListSet.hpp \
	../datastructures/linkedlists/ESTMLinkedListSet.hpp \
	../datastructures/linkedlists/OFLFLinkedListSet.hpp \
	../datastructures/linkedlists/OFWFLinkedListSet.hpp \
	../datastructures/linkedlists/STMLinkedListSet.hpp \
	../datastructures/linkedlists/TinySTMLinkedListSet.hpp \
	
SRC_TREES = \
	../datastructures/treemaps/ESTMRedBlackTree.hpp \
	../datastructures/treemaps/NatarajanTreeHE.hpp \
	../datastructures/treemaps/OFLFRedBlackTree.hpp \
	../datastructures/treemaps/OFWFRedBlackTree.hpp \

QUEUES_DEP = \
	../datastructures/queues/ESTMArrayLinkedListQueue.hpp \
	../datastructures/queues/ESTMLinkedListQueue.hpp \
	../datastructures/queues/FAAArrayQueue.hpp \
	../datastructures/queues/LCRQueue.hpp \
	../datastructures/queues/MichaelScottQueue.hpp \
	../datastructures/queues/OFLFArrayLinkedListQueue.hpp \
	../datastructures/queues/OFLFLinkedListQueue.hpp \
	../datastructures/queues/OFWFArrayLinkedListQueue.hpp \
	../datastructures/queues/OFWFLinkedListQueue.hpp \
	../datastructures/queues/TurnQueue.hpp \
	
PQUEUES_DEP = \
	../pdatastructures/pqueues/MichaelScottQueue.hpp \
	../pdatastructures/pqueues/PFriedmanQueue.hpp \
	../pdatastructures/pqueues/PMDKLinkedListQueue.hpp \
	../pdatastructures/pqueues/PMichaelScottQueue.hpp \
	../pdatastructures/TMLinkedListQueue.hpp \
	../pdatastructures/pqueues/POFLFLinkedListQueue.hpp \
	../pdatastructures/pqueues/POFWFLinkedListQueue.hpp \
	../pdatastructures/pqueues/RomLogLinkedListQueue.hpp \
	../pdatastructures/pqueues/RomLRLinkedListQueue.hpp \

ROMULUS_LIB_SRC = \
	../common/ThreadRegistry.cpp \
	../ptms/romuluslog/malloc.cpp \
	../ptms/romuluslog/RomulusLog.cpp \
	../ptms/romuluslr/malloc.cpp \
	../ptms/romuluslr/RomulusLR.cpp \

ROMULUS_LIB_DEP = \
	$(ROMULUS_LIB_SRC) \
	../ptms/romuluslog/RomulusLog.hpp \
	../ptms/romuluslr/RomulusLR.hpp \
		
TREVOR_BROWN_INCLUDES = \
	-I../datastructures/trevor_brown_abtree/common/recordmgr \
	-I../datastructures/trevor_brown_abtree/common \
	-I../datastructures/trevor_brown_abtree/common/descriptors \
	-I../datastructures/trevor_brown_abtree/common/rq \
	-I../datastructures/trevor_brown_abtree/common/rq \
	-I../datastructures/trevor_brown_abtree/common/atomic_ops \

all: $(BINARIES) persistencyclean


clean: persistencyclean
	rm -f bin/*
	rm -f lib/*


persistencyclean:
	rm -f /dev/shm/*_shared
	rm -f /dev/shm/psegments/*


#
# Create a library for RomulusLog and RomulusLR
#
lib/threadregistry.o: $(ROMULUS_LIB_DEP)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c ../common/ThreadRegistry.cpp -o lib/threadregistry.o

lib/mallocromlog.o: $(ROMULUS_LIB_DEP)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c ../ptms/romuluslog/malloc.cpp -o lib/mallocromlog.o

lib/romlog.o: $(ROMULUS_LIB_DEP)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c ../ptms/romuluslog/RomulusLog.cpp -o lib/romlog.o

lib/mallocromlr.o: $(ROMULUS_LIB_DEP)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c ../ptms/romuluslr/malloc.cpp -o lib/mallocromlr.o

lib/romlr.o: $(ROMULUS_LIB_DEP)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c ../ptms/romuluslr/RomulusLR.cpp -o lib/romlr.o
	
lib/libromulus.a: lib/threadregistry.o lib/mallocromlog.o lib/romlog.o lib/mallocromlr.o lib/romlr.o
	ar rcs lib/libromulus.a lib/threadregistry.o lib/mallocromlog.o lib/romlog.o lib/mallocromlr.o lib/romlr.o



#
# Queues for volatile memory
#	
bin/q-ll-enq-deq: q-ll-enq-deq.cpp $(STMS) $(QUEUES_DEP)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CSRCS) q-ll-enq-deq.cpp -o bin/q-ll-enq-deq -lpthread $(ESTM_LIB)
	
bin/q-array-enq-deq: q-array-enq-deq.cpp $(STMS) $(QUEUES_DEP)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CSRCS) q-array-enq-deq.cpp -o bin/q-array-enq-deq -lpthread $(ESTM_LIB)
	
bin/q-ll-burst: q-ll-burst.cpp $(QUEUES_DEP)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CSRCS) q-ll-burst.cpp -o bin/q-ll-burst -lpthread $(ESTM_LIB)

# Same as above but for TinySTM
bin/q-ll-enq-deq-tiny: q-ll-enq-deq.cpp $(STMS) $(QUEUES_DEP)
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) q-ll-enq-deq.cpp -o bin/q-ll-enq-deq-tiny -lpthread $(TINYSTM_LIB)

bin/q-array-enq-deq-tiny: q-array-enq-deq.cpp $(STMS) $(QUEUES_DEP)
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) q-array-enq-deq.cpp -o bin/q-array-enq-deq-tiny -lpthread $(TINYSTM_LIB)


#
# Queues for persistent memory
#	
bin/pq-ll-enq-deq: pq-ll-enq-deq.cpp $(PTMS) $(PQUEUES_DEP) PBenchmarkQueues.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) pq-ll-enq-deq.cpp -o bin/pq-ll-enq-deq -lpthread $(PMDKLIBS) lib/libromulus.a

	
#
# Sets for volatile memory
#	
bin/set-ll-1k: set-ll-1k.cpp $(STMS) $(SRC_LISTS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CSRCS) set-ll-1k.cpp -o bin/set-ll-1k -lpthread $(ESTM_LIB)

bin/set-ll-10k: set-ll-10k.cpp $(STMS) $(SRC_LISTS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CSRCS) set-ll-10k.cpp -o bin/set-ll-10k -lpthread $(ESTM_LIB)

bin/set-tree-1k: set-tree-1k.cpp $(STMS) $(SRC_TREES)
	$(CXX) $(CXXFLAGS) -fuse-ld=gold -fsanitize=address $(INCLUDES) $(TREVOR_BROWN_INCLUDES) ../common/ThreadRegistry.cpp $(CSRCS) set-tree-1k.cpp -o bin/set-tree-1k -lpthread $(ESTM_LIB)

bin/set-tree-10k: set-tree-10k.cpp $(STMS) $(SRC_TREES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TREVOR_BROWN_INCLUDES) ../common/ThreadRegistry.cpp $(CSRCS) set-tree-10k.cpp -o bin/set-tree-10k -lpthread $(ESTM_LIB)

bin/set-tree-1m: set-tree-1m.cpp $(STMS) $(SRC_TREES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TREVOR_BROWN_INCLUDES) ../common/ThreadRegistry.cpp $(CSRCS) set-tree-1m.cpp -o bin/set-tree-1m -lpthread $(ESTM_LIB)

bin/set-hash-1k: set-hash-1k.cpp $(STMS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CSRCS) set-hash-1k.cpp -o bin/set-hash-1k -lpthread $(ESTM_LIB)



# Same as above, but for Tiny STM only
bin/set-ll-1k-tiny: set-ll-1k.cpp $(STMS) $(SRC_LISTS)
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) set-ll-1k.cpp -o bin/set-ll-1k-tiny -lpthread $(TINYSTM_LIB)

bin/set-ll-10k-tiny: set-ll-10k.cpp $(STMS) $(SRC_LISTS)
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) set-ll-10k.cpp -o bin/set-ll-10k-tiny -lpthread $(TINYSTM_LIB)

bin/set-tree-1k-tiny: set-tree-1k.cpp $(STMS)
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) set-tree-1k.cpp -o bin/set-tree-1k-tiny -lpthread $(TINYSTM_LIB)

bin/set-tree-10k-tiny: set-tree-10k.cpp $(STMS)
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) set-tree-10k.cpp -o bin/set-tree-10k-tiny -lpthread $(TINYSTM_LIB)

bin/set-tree-1m-tiny: set-tree-1m.cpp $(STMS)
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) set-tree-1m.cpp -o bin/set-tree-1m-tiny -lpthread $(TINYSTM_LIB)

bin/set-hash-1k-tiny: set-hash-1k.cpp $(STMS)
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) set-hash-1k.cpp -o bin/set-hash-1k-tiny -lpthread $(TINYSTM_LIB)



#
# Sets for persistent memory
#	
bin/pset-ll-1k: pset-ll-1k.cpp $(PTMS) PBenchmarkSets.hpp ../pdatastructures/TMLinkedListSet.hpp lib/libromulus.a
	$(CXX) $(CXXFLAGS) $(INCLUDES) pset-ll-1k.cpp -o bin/pset-ll-1k -lpthread $(PMDKLIBS) lib/libromulus.a

bin/pset-ll-10k: pset-ll-10k.cpp $(PTMS) PBenchmarkSets.hpp ../pdatastructures/TMLinkedListSet.hpp lib/libromulus.a
	$(CXX) $(CXXFLAGS) $(INCLUDES) pset-ll-10k.cpp -o bin/pset-ll-10k -lpthread $(PMDKLIBS) lib/libromulus.a

bin/pset-hash-1k: pset-hash-1k.cpp $(PTMS) PBenchmarkSets.hpp ../pdatastructures/TMHashMap.hpp lib/libromulus.a
	$(CXX) $(CXXFLAGS) $(INCLUDES) pset-hash-1k.cpp -o bin/pset-hash-1k -lpthread $(PMDKLIBS) lib/libromulus.a

bin/pset-tree-1k: pset-tree-1k.cpp $(PTMS) PBenchmarkSets.hpp ../pdatastructures/TMRedBlackTree.hpp lib/libromulus.a
	$(CXX) $(CXXFLAGS) $(INCLUDES) pset-tree-1k.cpp -o bin/pset-tree-1k -lpthread $(PMDKLIBS) lib/libromulus.a
	
bin/pset-tree-1m: pset-tree-1m.cpp $(PTMS) PBenchmarkSets.hpp ../pdatastructures/TMRedBlackTree.hpp lib/libromulus.a
	$(CXX) $(CXXFLAGS) $(INCLUDES) pset-tree-1m.cpp -o bin/pset-tree-1m -lpthread $(PMDKLIBS) lib/libromulus.a

	
#	
# SPS for volatile memory
#
bin/sps-integer: sps-integer.cpp $(STMS) BenchmarkSPS.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CSRCS) sps-integer.cpp -o bin/sps-integer -lpthread $(ESTM_LIB)

bin/sps-integer-tiny: sps-integer.cpp $(STMS) BenchmarkSPS.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) sps-integer.cpp -o bin/sps-integer-tiny -lpthread $(TINYSTM_LIB)

bin/sps-object: sps-object.cpp $(STMS) BenchmarkSPS.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CSRCS) sps-object.cpp -o bin/sps-object -lpthread $(ESTM_LIB)

bin/sps-object-tiny: sps-object.cpp $(STMS) BenchmarkSPS.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) sps-object.cpp -o bin/sps-object-tiny -lpthread $(TINYSTM_LIB)


#
# SPS for persistent memory
#
bin/psps-integer: psps-integer.cpp $(PTMS) PBenchmarkSPS.hpp lib/libromulus.a
	$(CXX) $(CXXFLAGS) $(INCLUDES) psps-integer.cpp -o bin/psps-integer -lpthread $(PMDKLIBS) lib/libromulus.a

bin/psps-integer-atlas: psps-integer-atlas.cpp $(PTMS) PBenchmarkSPS.hpp persistencyclean lib/libromulus.a
	$(CXX) $(CXXFLAGS) $(INCLUDES) psps-integer-atlas.cpp -o bin/psps-integer-atlas -lpthread lib/libromulus.a
	

# TODO: is it worth doing sps-object for PTMs ?


#
# Latency for STMs
#
bin/latency-counter: latency-counter.cpp $(STMS) BenchmarkLatencyCounter.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(CSRCS) latency-counter.cpp -o bin/latency-counter -lpthread $(ESTM_LIB)

bin/latency-counter-tiny: latency-counter.cpp $(STMS) BenchmarkLatencyCounter.hpp
	$(CXX) $(CXXFLAGS) -DUSE_TINY $(INCLUDES) $(TINYSTM_INC) $(CSRCS) latency-counter.cpp -o bin/latency-counter-tiny -lpthread $(TINYSTM_LIB)



	
# 
# Persistent balanced tree with 1M keys. Must be compiled one at a time otherwise you get all the NVM heaps allocated, which is too much 
#
bin/pset-tree-1m-romlog: pset-tree-1m.cpp PBenchmarkSets.hpp ../pdatastructures/TMRedBlackTree.hpp lib/libromulus.a 
	$(CXX) $(CXXFLAGS) -DUSE_ROMLOG $(INCLUDES) pset-tree-1m.cpp -o bin/pset-tree-1m-romlog -lpthread lib/libromulus.a

bin/pset-tree-1m-romlr: pset-tree-1m.cpp PBenchmarkSets.hpp ../pdatastructures/TMRedBlackTree.hpp lib/libromulus.a 
	$(CXX) $(CXXFLAGS) -DUSE_ROMLR $(INCLUDES) pset-tree-1m.cpp -o bin/pset-tree-1m-romlr -lpthread lib/libromulus.a

bin/pset-tree-1m-oflf: pset-tree-1m.cpp PBenchmarkSets.hpp ../pdatastructures/TMRedBlackTree.hpp ../ptms/OneFilePTMLF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFLF $(INCLUDES) pset-tree-1m.cpp -o bin/pset-tree-1m-oflf -lpthread

bin/pset-tree-1m-ofwf: pset-tree-1m.cpp PBenchmarkSets.hpp ../pdatastructures/TMRedBlackTree.hpp ../ptms/OneFilePTMWF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFWF $(INCLUDES) pset-tree-1m.cpp -o bin/pset-tree-1m-ofwf -lpthread

bin/pset-tree-1m-pmdk: pset-tree-1m.cpp PBenchmarkSets.hpp ../pdatastructures/TMRedBlackTree.hpp 
	$(CXX) $(CXXFLAGS) -DUSE_PMDK $(INCLUDES) pset-tree-1m.cpp -o bin/pset-tree-1m-pmdk -lpthread $(PMDKLIBS)
	

# experimental...
bin/pread-while-writing-romlog: pread-while-writing.cpp PBenchmarkSets.hpp ../pdatastructures/TMRedBlackTree.hpp lib/libromulus.a 
	$(CXX) $(CXXFLAGS) -DUSE_ROMLOG $(INCLUDES) pread-while-writing.cpp -o bin/pread-while-writing-romlog -lpthread lib/libromulus.a

bin/pread-while-writing-romlr: pread-while-writing.cpp PBenchmarkSets.hpp ../pdatastructures/TMRedBlackTree.hpp lib/libromulus.a 
	$(CXX) $(CXXFLAGS) -DUSE_ROMLR $(INCLUDES) pread-while-writing.cpp -o bin/pread-while-writing-romlr -lpthread lib/libromulus.a

bin/pread-while-writing-oflf: pread-while-writing.cpp PBenchmarkSets.hpp ../pdatastructures/TMRedBlackTree.hpp ../ptms/OneFilePTMLF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFLF $(INCLUDES) pread-while-writing.cpp -o bin/pread-while-writing-oflf -lpthread

bin/pread-while-writing-ofwf: pread-while-writing.cpp PBenchmarkSets.hpp ../pdatastructures/TMRedBlackTree.hpp ../ptms/OneFilePTMWF.hpp
	$(CXX) $(CXXFLAGS) -DUSE_OFWF $(INCLUDES) pread-while-writing.cpp -o bin/pread-while-writing-ofwf -lpthread

bin/pread-while-writing-pmdk: pread-while-writing.cpp PBenchmarkSets.hpp ../pdatastructures/TMRedBlackTree.hpp 
	$(CXX) $(CXXFLAGS) -DUSE_PMDK $(INCLUDES) pread-while-writing.cpp -o bin/pread-while-writing-pmdk -lpthread $(PMDKLIBS)
	
