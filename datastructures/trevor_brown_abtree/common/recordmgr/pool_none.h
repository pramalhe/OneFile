/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef POOL_NOOP_H
#define	POOL_NOOP_H

#include <cassert>
#include <iostream>
#include "blockbag.h"
#include "blockpool.h"
#include "pool_interface.h"
#include "plaf.h"
using namespace std;

template <typename T = void, class Alloc = allocator_interface<T> >
class pool_none : public pool_interface<T, Alloc> {
public:
    template<typename _Tp1>
    struct rebind {
        typedef pool_none<_Tp1, Alloc> other;
    };
    template<typename _Tp1, typename _Tp2>
    struct rebind2 {
        typedef pool_none<_Tp1, _Tp2> other;
    };
    
    string getSizeString() { return "no pool"; }
    /**
     * if the freebag contains any object, then remove one from the freebag
     * and return a pointer to it.
     * if not, then retrieve a new object from Alloc
     */
    inline T* get(const int tid) {
        MEMORY_STATS2 this->alloc->debug->addFromPool(tid, 1);
        return this->alloc->allocate(tid);
    }
    inline void add(const int tid, T* ptr) {
        this->alloc->deallocate(tid, ptr);
    }
    inline void addMoveFullBlocks(const int tid, blockbag<T> *bag, block<T> * const predecessor) {
        bag->clearWithoutFreeingElements();
        // note: this will leak memory, but i believe it is only used by debraplus (which really should use a pool)
    }
    inline void addMoveFullBlocks(const int tid, blockbag<T> *bag) {
        this->alloc->deallocateAndClear(tid, bag);
//        T* ptr;
//        while (ptr = bag->remove()) {
//            add(tid, ptr);
//        }
    }
    inline void addMoveAll(const int tid, blockbag<T> *bag) {
        this->alloc->deallocateAndClear(tid, bag);
//        T* ptr;
//        while (ptr = bag->remove()) {
//            add(tid, ptr);
//        }
    }
    inline int computeSize(const int tid) {
        return 0;
    }
    
    void debugPrintStatus(const int tid) {

    }
    
    pool_none(const int numProcesses, Alloc * const _alloc, debugInfo * const _debug)
            : pool_interface<T, Alloc>(numProcesses, _alloc, _debug) {
        VERBOSE DEBUG std::cout<<"constructor pool_none"<<std::endl;
    }
    ~pool_none() {
        VERBOSE DEBUG std::cout<<"destructor pool_none"<<std::endl;
    }
};

#endif

