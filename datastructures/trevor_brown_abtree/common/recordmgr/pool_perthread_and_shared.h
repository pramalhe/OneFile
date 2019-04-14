/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef POOL_PERTHREAD_AND_SHARED_H
#define	POOL_PERTHREAD_AND_SHARED_H

#include <cassert>
#include <iostream>
#include <sstream>
#include "blockbag.h"
#include "blockpool.h"
#include "pool_interface.h"
#include "plaf.h"
#include "globals.h"
using namespace std;

#define POOL_THRESHOLD_IN_BLOCKS 10

template <typename T = void, class Alloc = allocator_interface<T> >
class pool_perthread_and_shared : public pool_interface<T, Alloc> {
private:
    lockfreeblockbag<T> *sharedBag;       // shared bag that we offload blocks on when we have too many in our freeBag
    blockbag<T> **freeBag;                // freeBag[tid] = bag of objects of type T that are ready to be reused by the thread with id tid

    // note: only does something if freeBag contains at least two full blocks
    inline bool tryGiveFreeObjects(const int tid) {
        if (freeBag[tid]->getSizeInBlocks() >= POOL_THRESHOLD_IN_BLOCKS) {
            block<T> *b = freeBag[tid]->removeFullBlock(); // returns NULL if freeBag has < 2 full blocks
            assert(b);
//            if (b) {
                sharedBag->addBlock(b);
                MEMORY_STATS this->debug->addGiven(tid, 1);
                //DEBUG2 COUTATOMIC("  thread "<<this->tid<<" sharedBag("<<(sizeof(T)==sizeof(Node<long,long>)?"Node":"SCXRecord")<<") now contains "<<sharedBag->size()<<" blocks"<<std::endl);
//            }
            return true;
        }
        return false;
    }
//    
//    inline void tryTakeFreeObjects(const int tid) {
//        block<T> *b = sharedBag->getBlock();
//        if (b) {
//            freeBag[tid]->addFullBlock(b);
//            DEBUG this->debug->addTaken(tid, 1);
//            //DEBUG2 COUTATOMIC("  thread "<<this->tid<<" took "<<b->computeSize()<<" objects from sharedBag"<<std::endl);
//        }
//    }
public:
    template<typename _Tp1>
    struct rebind {
        typedef pool_perthread_and_shared<_Tp1, Alloc> other;
    };
    template<typename _Tp1, typename _Tp2>
    struct rebind2 {
        typedef pool_perthread_and_shared<_Tp1, _Tp2> other;
    };
    
//    long long getSizeInNodes() {
//        long long sum = 0;
//        for (int tid=0;tid<this->NUM_PROCESSES;++tid) {
//            sum += freeBag[tid]->computeSize();
//        }
////        sum += sharedBag->sizeInBlocks() * BLOCK_SIZE;
//        return sum;
//    }
    string getSizeString() {
        stringstream ss;
        long long insharedbag = sharedBag->size();
        long long infreebags = 0;
        for (int tid=0;tid<this->NUM_PROCESSES;++tid) {
            infreebags += freeBag[tid]->computeSize();
        }
        ss<<infreebags<<" in free bags and "<<insharedbag<<" in the shared bag";
        return ss.str();
    }
    
    /**
     * if the freebag contains any object, then remove one from the freebag
     * and return a pointer to it.
     * if not, then retrieve a new object from Alloc
     */
    inline T* get(const int tid) {
        MEMORY_STATS2 this->alloc->debug->addFromPool(tid, 1);
        return freeBag[tid]->template remove<Alloc>(tid, sharedBag, this->alloc);
    }
    inline void add(const int tid, T* ptr) {
        MEMORY_STATS2 this->debug->addToPool(tid, 1);
        freeBag[tid]->add(tid, ptr, sharedBag, POOL_THRESHOLD_IN_BLOCKS, this->alloc);
    }
    inline void addMoveFullBlocks(const int tid, blockbag<T> *bag, block<T> * const predecessor) {
        // WARNING: THE FOLLOWING DEBUG COMPUTATION GETS THE WRONG NUMBER OF BLOCKS.
        MEMORY_STATS2 this->debug->addToPool(tid, (bag->getSizeInBlocks()-1)*BLOCK_SIZE);
        freeBag[tid]->appendMoveFullBlocks(bag, predecessor);
        while (tryGiveFreeObjects(tid)) {}
    }
    inline void addMoveFullBlocks(const int tid, blockbag<T> *bag) {
        // WARNING: THE FOLLOWING DEBUG COMPUTATION GETS THE WRONG NUMBER OF BLOCKS.
        MEMORY_STATS2 this->debug->addToPool(tid, (bag->getSizeInBlocks()-1)*BLOCK_SIZE);
        freeBag[tid]->appendMoveFullBlocks(bag);
        while (tryGiveFreeObjects(tid)) {}
    }
    inline void addMoveAll(const int tid, blockbag<T> *bag) {
        MEMORY_STATS2 this->debug->addToPool(tid, bag->computeSize());
        freeBag[tid]->appendMoveAll(bag);
        while (tryGiveFreeObjects(tid)) {}
    }
    inline int computeSize(const int tid) {
        return freeBag[tid]->computeSize();
    }
    
    void debugPrintStatus(const int tid) {
//        long free = computeSize(tid);
//        long share = sharedBag->sizeInBlocks();
//        COUTATOMIC("free="<<free<<" share="<<share);
    }
    
    pool_perthread_and_shared(const int numProcesses, Alloc * const _alloc, debugInfo * const _debug)
            : pool_interface<T, Alloc>(numProcesses, _alloc, _debug) {
        VERBOSE DEBUG COUTATOMIC("constructor pool_perthread_and_shared"<<std::endl);
        freeBag = new blockbag<T>*[numProcesses];
        for (int tid=0;tid<numProcesses;++tid) {
            freeBag[tid] = new blockbag<T>(tid, this->blockpools[tid]);
        }
        sharedBag = new lockfreeblockbag<T>();
    }
    ~pool_perthread_and_shared() {
        VERBOSE DEBUG COUTATOMIC("destructor pool_perthread_and_shared"<<std::endl);
        // clean up shared bag
        const int dummyTid = 0;
        block<T> *fullBlock;
        while ((fullBlock = sharedBag->getBlock()) != NULL) {
            while (!fullBlock->isEmpty()) {
                T * const ptr = fullBlock->pop();
                this->alloc->deallocate(dummyTid, ptr);
            }
            this->blockpools[dummyTid]->deallocateBlock(fullBlock);
        }
        // clean up free bags
        for (int tid=0;tid<this->NUM_PROCESSES;++tid) {
            this->alloc->deallocateAndClear(tid, freeBag[tid]);
            delete freeBag[tid];
        }
        delete[] freeBag;
        delete sharedBag;
    }
};

#endif

