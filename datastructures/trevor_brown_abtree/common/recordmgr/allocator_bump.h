/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef ALLOC_BUMP_H
#define	ALLOC_BUMP_H

#include "plaf.h"
#include "globals.h"
#include "allocator_interface.h"
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <vector>
using namespace std;

template<typename T = void>
class allocator_bump : public allocator_interface<T> {
    private:
        const int cachelines;    // # cachelines needed to store an object of type T
        // for bump allocation from a contiguous chunk of memory
        T ** mem;             // mem[tid*PREFETCH_SIZE_WORDS] = pointer to current array to perform bump allocation from
        int * memBytes;       // memBytes[tid*PREFETCH_SIZE_WORDS] = size of mem in bytes
        T ** current;         // current[tid*PREFETCH_SIZE_WORDS] = pointer to current position in array mem
        vector<T*> ** toFree; // toFree[tid] = pointer to vector of bump allocation arrays to free when this allocator is destroyed

        T* bump_memory_next(const int tid) {
            T* result = current[tid*PREFETCH_SIZE_WORDS];
            current[tid*PREFETCH_SIZE_WORDS] = (T*) (((char*) current[tid*PREFETCH_SIZE_WORDS]) + (cachelines*BYTES_IN_CACHE_LINE));
            return result;
        }
        int bump_memory_bytes_remaining(const int tid) {
            return (((char*) mem[tid*PREFETCH_SIZE_WORDS])+memBytes[tid*PREFETCH_SIZE_WORDS]) - ((char*) current[tid*PREFETCH_SIZE_WORDS]);
        }
        bool bump_memory_full(const int tid) {
            return (((char*) current[tid*PREFETCH_SIZE_WORDS])+cachelines*BYTES_IN_CACHE_LINE > ((char*) mem[tid*PREFETCH_SIZE_WORDS])+memBytes[tid*PREFETCH_SIZE_WORDS]);
        }
        // call this when mem is null, or doesn't contain enough space to allocate an object
        void bump_memory_allocate(const int tid) {
            mem[tid*PREFETCH_SIZE_WORDS] = (T*) malloc(1<<24);
            memBytes[tid*PREFETCH_SIZE_WORDS] = 1<<24;
            current[tid*PREFETCH_SIZE_WORDS] = mem[tid*PREFETCH_SIZE_WORDS];
            toFree[tid]->push_back(mem[tid*PREFETCH_SIZE_WORDS]); // remember we allocated this to free it later
#ifdef HAS_FUNCTION_aligned_alloc
#else
            // align on cacheline boundary
            int mod = (int) (((long) mem[tid*PREFETCH_SIZE_WORDS]) % BYTES_IN_CACHE_LINE);
            if (mod > 0) {
                // we are ignoring the first mod bytes of mem, because if we
                // use them, we will not be aligning objects to cache lines.
                current[tid*PREFETCH_SIZE_WORDS] = (T*) (((char*) mem[tid*PREFETCH_SIZE_WORDS]) + BYTES_IN_CACHE_LINE - mod);
            } else {
                current[tid*PREFETCH_SIZE_WORDS] = mem[tid*PREFETCH_SIZE_WORDS];
            }
#endif
            assert((((long) current[tid*PREFETCH_SIZE_WORDS]) % BYTES_IN_CACHE_LINE) == 0);
        }

    public:
        template<typename _Tp1>
        struct rebind {
            typedef allocator_bump<_Tp1> other;
        };

        // reserve space for ONE object of type T
        T* allocate(const int tid) {
            // bump-allocate from a contiguous chunk of memory
            if (!mem[tid*PREFETCH_SIZE_WORDS] || bump_memory_full(tid)) {
                bump_memory_allocate(tid);
                MEMORY_STATS {
                    this->debug->addAllocated(tid, memBytes[tid*PREFETCH_SIZE_WORDS] / cachelines / BYTES_IN_CACHE_LINE);
                    VERBOSE DEBUG2 {
//                        if ((this->debug->getAllocated(tid) % 2000) == 0) {
//                            this->debugInterfaces->reclaim->debugPrintStatus(tid);
//                            debugPrintStatus(tid);
                            COUTATOMICTID("allocated "<<(memBytes[tid*PREFETCH_SIZE_WORDS] / cachelines / BYTES_IN_CACHE_LINE)/*this->debug->getAllocated(tid)*/<<" records of size "<<sizeof(T)<<std::endl);
//                            COUTATOMIC(" ");
//                            this->pool->debugPrintStatus(tid);
//                            COUTATOMIC(endl);
//                        }
                    }
                }
            }
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

        void initThread(const int tid) {}
        
        allocator_bump(const int numProcesses, debugInfo * const _debug)
                : allocator_interface<T>(numProcesses, _debug)
                , cachelines((sizeof(T)+(BYTES_IN_CACHE_LINE-1))/BYTES_IN_CACHE_LINE){
            VERBOSE DEBUG COUTATOMIC("constructor allocator_bump"<<std::endl);
            mem = new T*[numProcesses*PREFETCH_SIZE_WORDS];
            memBytes = new int[numProcesses*PREFETCH_SIZE_WORDS];
            current = new T*[numProcesses*PREFETCH_SIZE_WORDS];
            toFree = new vector<T*>*[numProcesses];
            for (int tid=0;tid<numProcesses;++tid) {
                mem[tid*PREFETCH_SIZE_WORDS] = 0;
                memBytes[tid*PREFETCH_SIZE_WORDS] = 0;
                current[tid*PREFETCH_SIZE_WORDS] = 0;
                toFree[tid] = new vector<T*>();
            }
        }
        ~allocator_bump() {
            VERBOSE COUTATOMIC("destructor allocator_bump"<<std::endl);
            // free all allocated blocks of memory
            for (int tid=0;tid<this->NUM_PROCESSES;++tid) {
                int n = toFree[tid]->size();
                for (int i=0;i<n;++i) {
                    free((*toFree[tid])[i]);
                }
                delete toFree[tid];
            }
            delete[] mem;
            delete[] memBytes;
            delete[] current;
            delete[] toFree;
        }
    };

#endif	/* ALLOC_NEW_H */

