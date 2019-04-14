/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef RECLAIM_EPOCH_H
#define	RECLAIM_EPOCH_H

#include <cassert>
#include <iostream>
#include <sstream>
#include <limits.h>
#include "blockbag.h"
#include "plaf.h"
#include "allocator_interface.h"
#include "reclaimer_interface.h"
using namespace std;



template <typename T = void, class Pool = pool_interface<T> >
class reclaimer_debra : public reclaimer_interface<T, Pool> {
protected:
#define EPOCH_INCREMENT 2
#define BITS_EPOCH(ann) ((ann)&~(EPOCH_INCREMENT-1))
#define QUIESCENT(ann) ((ann)&1)
#define GET_WITH_QUIESCENT(ann) ((ann)|1)

#ifdef RAPID_RECLAMATION
#define MIN_OPS_BEFORE_READ 1
//#define MIN_OPS_BEFORE_CAS_EPOCH 1
#else
#define MIN_OPS_BEFORE_READ 20
//#define MIN_OPS_BEFORE_CAS_EPOCH 100
#endif
    
#define NUMBER_OF_EPOCH_BAGS 9
#define NUMBER_OF_ALWAYS_EMPTY_EPOCH_BAGS 3
    
    // for epoch based reclamation
    volatile long epoch;
    atomic_long *announcedEpoch;        // announcedEpoch[tid*PREFETCH_SIZE_WORDS] // todo: figure out if volatile here would help processes notice changes more quickly.
    long *checked;                      // checked[tid*PREFETCH_SIZE_WORDS] = how far we've come in checking the announced epochs of other threads
    blockbag<T> **epochbags;            // epochbags[NUMBER_OF_EPOCH_BAGS*tid+0..NUMBER_OF_EPOCH_BAGS*tid+(NUMBER_OF_EPOCH_BAGS-1)] are epoch bags for thread tid.
    blockbag<T> **currentBag;           // pointer to current epoch bag for each process
    long *index;                        // index of currentBag in epochbags for each process
    // note: oldest bag is number (index+1)%NUMBER_OF_EPOCH_BAGS
    long *opsSinceRead;
    
public:
    template<typename _Tp1>
    struct rebind {
        typedef reclaimer_debra<_Tp1, Pool> other;
    };
    template<typename _Tp1, typename _Tp2>
    struct rebind2 {
        typedef reclaimer_debra<_Tp1, _Tp2> other;
    };
    
//    inline int getOldestBlockbagIndexOffset(const int tid) {
//        long long min_val = LLONG_MAX;
//        int min_i = -1;
//        for (int i=0;i<NUMBER_OF_EPOCH_BAGS;++i) {
//            long long reclaimCount = epochbags[tid*NUMBER_OF_EPOCH_BAGS+i]->getReclaimCount();
//            if (reclaimCount % 1) { // bag's contents are currently being freed
//                return i;
//            }
//            if (reclaimCount < min_val) {
//                min_val = reclaimCount;
//                min_i = i;
//            }
//        }
//        return min_i;
//    }
//    
//    inline set_of_bags<T> getBlockbags() { // blockbag_iterator<T> ** const output) {
////        int cnt=0;
////        for (int tid=0;tid<NUM_PROCESSES;++tid) {
////            for (int j=0;j<NUMBER_OF_EPOCH_BAGS;++j) {
////                output[cnt++] = epochbags[NUMBER_OF_EPOCH_BAGS*tid+j];
////            }
////        }
////        return cnt;
//        return {epochbags, this->NUM_PROCESSES*NUMBER_OF_EPOCH_BAGS};
//    }
//    
//    inline void getOldestTwoBlockbags(const int tid, blockbag<T> ** oldest, blockbag<T> ** secondOldest) {
//        long long min_val = LLONG_MAX;
//        int min_i = -1;
//        for (int i=0;i<NUMBER_OF_EPOCH_BAGS;++i) {
//            long long reclaimCount = epochbags[tid*NUMBER_OF_EPOCH_BAGS+i]->getReclaimCount();
//            if (reclaimCount % 1) { // bag's contents are currently being freed
//                min_i = i;
//                break;
//            }
//            if (reclaimCount < min_val) {
//                min_val = reclaimCount;
//                min_i = i;
//            }
//        }
//        if (min_i == -1) {
//            *oldest = *secondOldest = NULL;
//        } else {
//            *oldest = epochbags[tid*NUMBER_OF_EPOCH_BAGS + min_i];
//            *secondOldest = epochbags[tid*NUMBER_OF_EPOCH_BAGS + ((min_i+1)%NUMBER_OF_EPOCH_BAGS)];
//        }
//    }
    
    inline void getSafeBlockbags(const int tid, blockbag<T> ** bags) {
        SOFTWARE_BARRIER;
        int ix = index[tid*PREFETCH_SIZE_WORDS];
        bags[0] = epochbags[tid*NUMBER_OF_EPOCH_BAGS+ix];
        bags[1] = epochbags[tid*NUMBER_OF_EPOCH_BAGS+((ix+NUMBER_OF_EPOCH_BAGS-1)%NUMBER_OF_EPOCH_BAGS)];
        bags[2] = epochbags[tid*NUMBER_OF_EPOCH_BAGS+((ix+NUMBER_OF_EPOCH_BAGS-2)%NUMBER_OF_EPOCH_BAGS)];
        bags[3] = NULL;
        SOFTWARE_BARRIER;
        
//        SOFTWARE_BARRIER;
//        // find first dangerous blockbag
//        long long min_val = LLONG_MAX;
//        int min_i = -1;
//        for (int i=0;i<NUMBER_OF_EPOCH_BAGS;++i) {
//            long long reclaimCount = epochbags[tid*NUMBER_OF_EPOCH_BAGS+i]->getReclaimCount();
//            if (reclaimCount % 1) { // bag's contents are currently being freed
//                min_i = i;
//                break;
//            }
//            if (reclaimCount < min_val) {
//                min_val = reclaimCount;
//                min_i = i;
//            }
//        }
//        assert(min_i != -1);
//        min_i = (min_i + NUMBER_OF_ALWAYS_EMPTY_EPOCH_BAGS) % NUMBER_OF_EPOCH_BAGS;
//        
//        // process might free from bag at offset min_i, or the next one.
//        // the others are safe.
//        int i;
//        for (i=0;i<NUMBER_OF_EPOCH_BAGS-NUMBER_OF_UNSAFE_EPOCH_BAGS;++i) {
//            bags[i] = epochbags[tid*NUMBER_OF_EPOCH_BAGS + ((min_i + NUMBER_OF_UNSAFE_EPOCH_BAGS + i)%NUMBER_OF_EPOCH_BAGS)];
//        }
//        bags[i] = NULL; // null terminated array
//        
////        bags[0] = epochbags[tid*NUMBER_OF_EPOCH_BAGS + ((min_i + NUMBER_OF_UNSAFE_EPOCH_BAGS)%NUMBER_OF_EPOCH_BAGS)];
////        bags[1] = NULL; // null terminated array
////        bags[0] = NULL;
//
//        SOFTWARE_BARRIER; 

//        SOFTWARE_BARRIER;
//        /**
//         * find first dangerous blockbag.
//         * a process may free bag index+i+NUMBER_OF_ALWAYS_EMPTY_EPOCH_BAGS,
//         * where i=1,2,...,(NUMBER_OF_EPOCH_BAGS - NUMBER_OF_ALWAYS_EMPTY_EPOCH_BAGS).
//         * the rest are safe, and
//         * MUST contain all nodes retired in this epoch or the last.
//         */
//        int ix = (index[tid*PREFETCH_SIZE_WORDS]+1+NUMBER_OF_ALWAYS_EMPTY_EPOCH_BAGS) % NUMBER_OF_EPOCH_BAGS;
//        SOFTWARE_BARRIER;
//        int i;
//        // #safebags = total - #unsafe
//        for (i=0;i<NUMBER_OF_EPOCH_BAGS-NUMBER_OF_UNSAFE_EPOCH_BAGS;++i) {
//            // find i-th safe bag
//            int ix2 = (ix+NUMBER_OF_UNSAFE_EPOCH_BAGS+i)%NUMBER_OF_EPOCH_BAGS; // UNFINISHED CODE FROM HERE DOWN
//            bags[i] = epochbags[tid*NUMBER_OF_EPOCH_BAGS + ix2];
//        }
//        bags[i] = NULL; // null terminated array
//        SOFTWARE_BARRIER;
    }
    
    long long getSizeInNodes() {
        long long sum = 0;
        for (int tid=0;tid<this->NUM_PROCESSES;++tid) {
            for (int j=0;j<NUMBER_OF_EPOCH_BAGS;++j) {
                sum += epochbags[NUMBER_OF_EPOCH_BAGS*tid+j]->computeSize();
            }
        }
        return sum;
    }
    string getSizeString() {
        stringstream ss;
        ss<<getSizeInNodes()<<" in epoch bags";
        return ss.str();
    }
    
    inline static bool quiescenceIsPerRecordType() { return false; }
    
    inline bool isQuiescent(const int tid) {
        return QUIESCENT(announcedEpoch[tid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed));
    }

    inline static bool isProtected(const int tid, T * const obj) {
        return true;
    }
    inline static bool isQProtected(const int tid, T * const obj) {
        return false;
    }
    inline static bool protect(const int tid, T * const obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool memoryBarrier = true) {
        return true;
    }
    inline static void unprotect(const int tid, T * const obj) {}
    inline static bool qProtect(const int tid, T * const obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool memoryBarrier = true) {
        return true;
    }
    inline static void qUnprotectAll(const int tid) {}
    
    inline static bool shouldHelp() { return true; }
    
    // rotate the epoch bags and reclaim any objects retired two epochs ago.
    inline void rotateEpochBags(const int tid) {
        int nextIndex = (index[tid*PREFETCH_SIZE_WORDS]+1) % NUMBER_OF_EPOCH_BAGS;
        blockbag<T> * const freeable = epochbags[NUMBER_OF_EPOCH_BAGS*tid + ((nextIndex+NUMBER_OF_ALWAYS_EMPTY_EPOCH_BAGS) % NUMBER_OF_EPOCH_BAGS)];
        this->pool->addMoveFullBlocks(tid, freeable); // moves any full blocks (may leave a non-full block behind)
        SOFTWARE_BARRIER;
        index[tid*PREFETCH_SIZE_WORDS] = nextIndex;
        currentBag[tid*PREFETCH_SIZE_WORDS] = epochbags[NUMBER_OF_EPOCH_BAGS*tid + nextIndex];
    }

    // objects reclaimed by this epoch manager.
    // returns true if the call rotated the epoch bags for thread tid
    // (and reclaimed any objects retired two epochs ago).
    // otherwise, the call returns false.
    inline bool leaveQuiescentState(const int tid, void * const * const reclaimers, const int numReclaimers) {
        SOFTWARE_BARRIER; // prevent any bookkeeping from being moved after this point by the compiler.
        bool result = false;

        // ver 1
        long readEpoch = epoch;
        const long ann = announcedEpoch[tid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
//        // debug ver2
//        const long ann = announcedEpoch[tid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
//        ++opsSinceRead[tid*PREFETCH_SIZE_WORDS];
//        long readEpoch = ((opsSinceRead[tid*PREFETCH_SIZE_WORDS] % MIN_OPS_BEFORE_READ) == 0) ? epoch : BITS_EPOCH(ann);

        // if our announced epoch is different from the current epoch
        if (readEpoch != BITS_EPOCH(ann)) {
            // announce the new epoch, and rotate the epoch bags and
            // reclaim any objects retired two epochs ago.
            checked[tid*PREFETCH_SIZE_WORDS] = 0;
            //rotateEpochBags(tid);
            for (int i=0;i<numReclaimers;++i) {
                ((reclaimer_debra<T, Pool> * const) reclaimers[i])->rotateEpochBags(tid);
            }
            result = true;
        }
        // note: readEpoch, when written to announcedEpoch[tid],
        //       will set the state to non-quiescent and non-neutralized

        // incrementally scan the announced epochs of all threads
        int otherTid = checked[tid*PREFETCH_SIZE_WORDS];
        if ((++opsSinceRead[tid*PREFETCH_SIZE_WORDS] % MIN_OPS_BEFORE_READ) == 0) {
            long otherAnnounce = announcedEpoch[otherTid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
            if (BITS_EPOCH(otherAnnounce) == readEpoch
                    || QUIESCENT(otherAnnounce)) {
                const int c = ++checked[tid*PREFETCH_SIZE_WORDS];
                if (c >= this->NUM_PROCESSES /*&& c > MIN_OPS_BEFORE_CAS_EPOCH*/) {
                    __sync_bool_compare_and_swap(&epoch, readEpoch, readEpoch+EPOCH_INCREMENT);
                }
            }
        }
        SOFTWARE_BARRIER;
        if (readEpoch != ann) {
            announcedEpoch[tid*PREFETCH_SIZE_WORDS].store(readEpoch, memory_order_relaxed);
        }
        return result;
    }
    
    inline void enterQuiescentState(const int tid) {
        const long ann = announcedEpoch[tid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
        announcedEpoch[tid*PREFETCH_SIZE_WORDS].store(GET_WITH_QUIESCENT(ann), memory_order_relaxed);
    }
    
    // for all schemes except reference counting
    inline void retire(const int tid, T* p) {
        currentBag[tid*PREFETCH_SIZE_WORDS]->add(p);
        DEBUG2 this->debug->addRetired(tid, 1);
    }
    
    inline void unretireLast(const int tid) {
        assert(false); // we do not use this, since it makes it harder to reason about iteration over blockbags when they shrink (aside from when their contents are being reclaimed, and we can determine this is the case by inspecting bag->getReclaimCount()...)
        currentBag[tid*PREFETCH_SIZE_WORDS]->remove();
    }

    void debugPrintStatus(const int tid) {
//        assert(tid >= 0);
//        assert(tid < this->NUM_PROCESSES);
        if (tid == 0) {
            std::cout<<"global epoch counter="<<epoch<<std::endl;
        }
//        long announce = announcedEpoch[tid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
//        std::cout<<"tid="<<tid<<": announce="<<announce<<" bags(";
//        for (int i=0;i<NUMBER_OF_EPOCH_BAGS;++i) {
//            std::cout<<(i?",":"")<</*" bag"<<i<<"="<<*/epochbags[NUMBER_OF_EPOCH_BAGS*tid+i]->computeSize();
//        }
//        std::cout<<")"<<std::endl;
    }

    reclaimer_debra(const int numProcesses, Pool *_pool, debugInfo * const _debug, RecoveryMgr<void *> * const _recoveryMgr = NULL)
            : reclaimer_interface<T, Pool>(numProcesses, _pool, _debug, _recoveryMgr) {
        VERBOSE std::cout<<"constructor reclaimer_debra helping="<<this->shouldHelp()<<std::endl;// scanThreshold="<<scanThreshold<<std::endl;
        epoch = 0;
        epochbags = new blockbag<T>*[NUMBER_OF_EPOCH_BAGS*numProcesses];
        currentBag = new blockbag<T>*[numProcesses*PREFETCH_SIZE_WORDS];
        index = new long[numProcesses*PREFETCH_SIZE_WORDS];
        opsSinceRead = new long[numProcesses*PREFETCH_SIZE_WORDS];
        announcedEpoch = new atomic_long[numProcesses*PREFETCH_SIZE_WORDS];
        checked = new long[numProcesses*PREFETCH_SIZE_WORDS];
        for (int tid=0;tid<numProcesses;++tid) {
            for (int i=0;i<NUMBER_OF_EPOCH_BAGS;++i) {
                epochbags[NUMBER_OF_EPOCH_BAGS*tid+i] = new blockbag<T>(tid, this->pool->blockpools[tid]);
            }
            currentBag[tid*PREFETCH_SIZE_WORDS] = epochbags[NUMBER_OF_EPOCH_BAGS*tid];
            index[tid*PREFETCH_SIZE_WORDS] = 0;
            opsSinceRead[tid*PREFETCH_SIZE_WORDS] = 0;
            announcedEpoch[tid*PREFETCH_SIZE_WORDS].store(GET_WITH_QUIESCENT(0), memory_order_relaxed);
            checked[tid*PREFETCH_SIZE_WORDS] = 0;
        }
    }
    ~reclaimer_debra() {
        VERBOSE DEBUG std::cout<<"destructor reclaimer_debra"<<std::endl;
        for (int tid=0;tid<this->NUM_PROCESSES;++tid) {
            // move contents of all bags into pool
            for (int i=0;i<NUMBER_OF_EPOCH_BAGS;++i) {
//                std::cout<<"main thread: moving "<<epochbags[NUMBER_OF_EPOCH_BAGS*tid+i]->computeSize()<<" objects from epoch bag of tid="<<tid<<" to pool"<<std::endl;
                this->pool->addMoveAll(tid, epochbags[NUMBER_OF_EPOCH_BAGS*tid+i]);
                delete epochbags[NUMBER_OF_EPOCH_BAGS*tid+i];
            }
        }
        delete[] epochbags;
        delete[] index;
        delete[] opsSinceRead;
        delete[] currentBag;
        delete[] announcedEpoch;
        delete[] checked;
    }

};

#endif

