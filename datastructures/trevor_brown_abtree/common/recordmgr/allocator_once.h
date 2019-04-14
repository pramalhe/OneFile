/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef ALLOC_ONCE_H
#define	ALLOC_ONCE_H

#include "plaf.h"
#include "globals.h"
#include "allocator_interface.h"
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <vector>
using namespace std;

// this allocator only performs allocation once, at the beginning of the program.
// define the following to specify how much memory should be allocated.
#ifndef ALLOC_ONCE_MEMORY
    #define ALLOC_ONCE_MEMORY (1ULL<<32) /* default: 4 GB */
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))

template<typename T = void>
class allocator_once : public allocator_interface<T> {
private:
    const int cachelines;    // # cachelines needed to store an object of type T
    // for bump allocation from a contiguous chunk of memory
    T ** mem;             // mem[tid] = pointer to current array to perform bump allocation from
    size_t * memBytes;       // memBytes[tid*PREFETCH_SIZE_WORDS] = size of mem in bytes
    T ** current;         // current[tid*PREFETCH_SIZE_WORDS] = pointer to current position in array mem

    T* bump_memory_next(const int tid) {
        T* result = current[tid*PREFETCH_SIZE_WORDS];
        current[tid*PREFETCH_SIZE_WORDS] = (T*) (((char*) current[tid*PREFETCH_SIZE_WORDS]) + (cachelines*BYTES_IN_CACHE_LINE));
        return result;
    }
    int bump_memory_bytes_remaining(const int tid) {
        return (((char*) mem[tid])+memBytes[tid*PREFETCH_SIZE_WORDS]) - ((char*) current[tid*PREFETCH_SIZE_WORDS]);
    }
    bool bump_memory_full(const int tid) {
        return (((char*) current[tid*PREFETCH_SIZE_WORDS])+cachelines*BYTES_IN_CACHE_LINE > ((char*) mem[tid])+memBytes[tid*PREFETCH_SIZE_WORDS]);
    }

public:
    template<typename _Tp1>
    struct rebind {
        typedef allocator_once<_Tp1> other;
    };

    // reserve space for ONE object of type T
    T* allocate(const int tid) {
        if (bump_memory_full(tid)) return NULL;
        return bump_memory_next(tid);
    }
    void static deallocate(const int tid, T * const p) {
        // no op for this allocator; memory is freed only by the destructor.
        // however, we have to call the destructor for the object manually...
        p->~T();
    }
    void deallocateAndClear(const int tid, blockbag<T> * const bag) {
        // the bag is cleared, which makes it seem like we're leaking memory,
        // but it will be freed in the destructor as we release the huge
        // slabs of memory.
        bag->clearWithoutFreeingElements();
    }

    void debugPrintStatus(const int tid) {}
    
    void initThread(const int tid) {
//        // touch each page of memory before our trial starts
//        long pagesize = sysconf(_SC_PAGE_SIZE);
//        int last = (int) (memBytes[tid*PREFETCH_SIZE_WORDS]/pagesize);
//        VERBOSE COUTATOMICTID("touching each page... memBytes="<<memBytes[tid*PREFETCH_SIZE_WORDS]<<" pagesize="<<pagesize<<" last="<<last<<std::endl);
//        for (int i=0;i<last;++i) {
//            TRACE COUTATOMICTID("    "<<tid<<" touching page "<<i<<" at address "<<(long)((long*)(((char*) mem[tid])+i*pagesize))<<std::endl);
//            *((long*)(((char*) mem[tid])+i*pagesize)) = 0;
//        }
//        VERBOSE COUTATOMICTID(" finished touching each page."<<std::endl);
    }

    allocator_once(const int numProcesses, debugInfo * const _debug)
            : allocator_interface<T>(numProcesses, _debug)
            , cachelines((sizeof(T)+(BYTES_IN_CACHE_LINE-1))/BYTES_IN_CACHE_LINE) {
        VERBOSE DEBUG COUTATOMIC("constructor allocator_once"<<std::endl);
        mem = new T*[numProcesses];
        memBytes = new size_t[numProcesses*PREFETCH_SIZE_WORDS];
        current = new T*[numProcesses*PREFETCH_SIZE_WORDS];
        for (int tid=0;tid<numProcesses;++tid) {
            long long newSizeBytes = ALLOC_ONCE_MEMORY / numProcesses; // divide several GB amongst all threads.
            VERBOSE COUTATOMIC("newSizeBytes        = "<<newSizeBytes<<std::endl);
            assert((newSizeBytes % (cachelines*BYTES_IN_CACHE_LINE)) == 0);

            mem[tid] = (T*) malloc((size_t) newSizeBytes);
            if (mem[tid] == NULL) {
                cerr<<"could not allocate memory"<<std::endl;
                exit(-1);
            }
            //COUTATOMIC("successfully allocated"<<std::endl);
            memBytes[tid*PREFETCH_SIZE_WORDS] = (size_t) newSizeBytes;
            current[tid*PREFETCH_SIZE_WORDS] = mem[tid];
            // align on cacheline boundary
            int mod = (int) (((long) mem[tid]) % BYTES_IN_CACHE_LINE);
            if (mod > 0) {
                // we are ignoring the first mod bytes of mem, because if we
                // use them, we will not be aligning objects to cache lines.
                current[tid*PREFETCH_SIZE_WORDS] = (T*) (((char*) mem[tid]) + BYTES_IN_CACHE_LINE - mod);
            } else {
                current[tid*PREFETCH_SIZE_WORDS] = mem[tid];
            }
            assert((((long) current[tid*PREFETCH_SIZE_WORDS]) % BYTES_IN_CACHE_LINE) == 0);
        }
    }
    ~allocator_once() {
        long allocated = 0;
        for (int tid=0;tid<this->NUM_PROCESSES;++tid) {
            allocated += (((char*) current[tid*PREFETCH_SIZE_WORDS]) - ((char*) mem[tid]));
        }
        VERBOSE COUTATOMIC("destructor allocator_once allocated="<<allocated<<" bytes, or "<<(allocated/(cachelines*BYTES_IN_CACHE_LINE))<<" objects of size "<<sizeof(T)<<" occupying "<<cachelines<<" cache lines"<<std::endl);
        // free all allocated blocks of memory
        for (int tid=0;tid<this->NUM_PROCESSES;++tid) {
            delete mem[tid];
        }
        delete[] mem;
        delete[] memBytes;
        delete[] current;
    }
};
#endif	/* ALLOC_ONCE_H */

