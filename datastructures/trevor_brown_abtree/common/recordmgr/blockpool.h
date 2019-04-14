/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef BLOCKPOOL_H
#define	BLOCKPOOL_H

#include "blockbag.h"
#include "plaf.h"
#include <iostream>
using namespace std;

#define MAX_BLOCK_POOL_SIZE 32

#ifndef VERBOSE
#define VERBOSE if(0)
#endif

template <typename T>
class block;

template <typename T>
class blockpool {
private:
    block<T> *pool[MAX_BLOCK_POOL_SIZE];
    int poolSize;

    long debugAllocated;
    long debugPoolDeallocated;
    long debugPoolAllocated;
    long debugFreed;
public:
    blockpool() {
        poolSize = 0;
        debugAllocated = 0;
        debugPoolAllocated = 0;
        debugPoolDeallocated = 0;
        debugFreed = 0;
    }
    ~blockpool() {
        VERBOSE DEBUG std::cout<<"destructor blockpool;";
        for (int i=0;i<poolSize;++i) {
            //DEBUG ++debugFreed;
            assert(pool[i]->isEmpty());
            delete pool[i];                           // warning: uses locks (for some allocators)
        }
        VERBOSE DEBUG std::cout<<" blocks allocated "<<debugAllocated<<" pool-allocated "<<debugPoolAllocated<<" freed "<<debugFreed<<" pool-deallocated "<<debugPoolDeallocated<<std::endl;
    }
    block<T>* allocateBlock(block<T> * const next) {
        if (poolSize) {
            //DEBUG ++debugPoolAllocated;
            block<T> *result = pool[--poolSize]; // pop a block off the stack
            *result = block<T>(next);
            assert(result->next == next);
            assert(result->computeSize() == 0);
            assert(result->isEmpty());
            return result;
        } else {
            //DEBUG ++debugAllocated;
            return new block<T>(next);                // warning: uses locks (for some allocators)
        }
    }
    void deallocateBlock(block<T> * const b) {
        assert(b->isEmpty());
        if (poolSize == MAX_BLOCK_POOL_SIZE) {
            //DEBUG ++debugFreed;
//            assert(poolSize < MAX_BLOCK_POOL_SIZE); // for the RQ benchmarks, we want to assert that we never free a block
#ifndef NO_FREE
            delete b;                                 // warning: uses locks (for some allocators)
#endif
        } else {
            //DEBUG ++debugPoolDeallocated;
            pool[poolSize++] = b;
        }
    }
};

#endif	/* BLOCKPOOL_H */

