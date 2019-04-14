/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef ALLOC_NEW_H
#define	ALLOC_NEW_H

#include "plaf.h"
#include "pool_interface.h"
#include <cstdlib>
#include <cassert>
#include <iostream>
using namespace std;

//__thread long long currentAllocatedBytes = 0;
//__thread long long maxAllocatedBytes = 0;

template<typename T = void>
class allocator_new : public allocator_interface<T> {
public:
    template<typename _Tp1>
    struct rebind {
        typedef allocator_new<_Tp1> other;
    };
    
    // reserve space for ONE object of type T
    T* allocate(const int tid) {
        // allocate a new object
        MEMORY_STATS {
            this->debug->addAllocated(tid, 1);
            VERBOSE {
                if ((this->debug->getAllocated(tid) % 2000) == 0) {
                    debugPrintStatus(tid);
                }
            }
//            currentAllocatedBytes += sizeof(T);
//            if (currentAllocatedBytes > maxAllocatedBytes) {
//                maxAllocatedBytes = currentAllocatedBytes;
//            }
        }
        return new T; //(T*) malloc(sizeof(T));
    }
    void deallocate(const int tid, T * const p) {
        // note: allocators perform the actual freeing/deleting, since
        // only they know how memory was allocated.
        // pools simply call deallocate() to request that it is freed.
        // allocators do not invoke pool functions.
        MEMORY_STATS {
            this->debug->addDeallocated(tid, 1);
//            currentAllocatedBytes -= sizeof(T);
        }
#if !defined NO_FREE
        delete p;
#endif
    }
    void deallocateAndClear(const int tid, blockbag<T> * const bag) {
#ifdef NO_FREE
        bag->clearWithoutFreeingElements();
#else
        while (!bag->isEmpty()) {
            T* ptr = bag->remove();
            deallocate(tid, ptr);
        }
#endif
    }
    
    void debugPrintStatus(const int tid) {
//        std::cout<</*"thread "<<tid<<" "<<*/"allocated "<<this->debug->getAllocated(tid)<<" objects of size "<<(sizeof(T));
//        std::cout<<" ";
////        this->pool->debugPrintStatus(tid);
//        std::cout<<std::endl;
    }
    
    void initThread(const int tid) {}
    
    allocator_new(const int numProcesses, debugInfo * const _debug)
            : allocator_interface<T>(numProcesses, _debug) {
        VERBOSE DEBUG std::cout<<"constructor allocator_new"<<std::endl;
    }
    ~allocator_new() {
        VERBOSE DEBUG std::cout<<"destructor allocator_new"<<std::endl;
    }
};

#endif	/* ALLOC_NEW_H */

