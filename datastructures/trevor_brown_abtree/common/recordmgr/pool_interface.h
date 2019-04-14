/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef POOL_INTERFACE_H
#define	POOL_INTERFACE_H

#include <iostream>
#include "allocator_interface.h"
#include "debug_info.h"
#include "blockpool.h"
#include "blockbag.h"
using namespace std;

template <typename T = void, class Alloc = allocator_interface<T> >
class pool_interface {
public:
    debugInfo * const debug;
    
    const int NUM_PROCESSES;
    blockpool<T> **blockpools; // allocated (or not) and freed by descendants
    Alloc *alloc;

    template<typename _Tp1>
    struct rebind {
        typedef pool_interface<_Tp1, Alloc> other;
    };
    template<typename _Tp1, typename _Tp2>
    struct rebind2 {
        typedef pool_interface<_Tp1, _Tp2> other;
    };
    
    string getSizeString() { return ""; }
//    long long getSizeInNodes() { return 0; }
    /**
     * if the pool contains any object, then remove one from the pool
     * and return a pointer to it. otherwise, return NULL.
     */
    inline T* get(const int tid);
    inline void add(const int tid, T* ptr);
    inline void addMoveFullBlocks(const int tid, blockbag<T> *bag);
    inline void addMoveAll(const int tid, blockbag<T> *bag);
    inline int computeSize(const int tid);
    
    void debugPrintStatus(const int tid);
    
    pool_interface(const int numProcesses, Alloc * const _alloc, debugInfo * const _debug)
            : debug(_debug) 
            , NUM_PROCESSES(numProcesses)
            , alloc(_alloc){
        VERBOSE DEBUG std::cout<<"constructor pool_interface"<<std::endl;
        this->blockpools = new blockpool<T>*[numProcesses];
        for (int tid=0;tid<numProcesses;++tid) {
            this->blockpools[tid] = new blockpool<T>();
        }
    }
    ~pool_interface() {
        VERBOSE DEBUG std::cout<<"destructor pool_interface"<<std::endl;
        for (int tid=0;tid<this->NUM_PROCESSES;++tid) {
            delete this->blockpools[tid];
        }
        delete[] this->blockpools;
    }
};

#endif

