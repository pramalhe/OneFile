/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef ALLOC_INTERFACE_H
#define	ALLOC_INTERFACE_H

#include "debug_info.h"
#include "blockbag.h"
#include <iostream>
using namespace std;

template <typename T = void>
class allocator_interface {
public:
    debugInfo * const debug;
    
    const int NUM_PROCESSES;
    
    template<typename _Tp1>
    struct rebind {
        typedef allocator_interface<_Tp1> other;
    };
    
    // allocate space for one object of type T
    T* allocate(const int tid);
    void deallocate(const int tid, T * const p);
    void deallocateAndClear(const int tid, blockbag<T> * const bag);
    void initThread(const int tid);
    
    void debugPrintStatus(const int tid);

    allocator_interface(const int numProcesses, debugInfo * const _debug)
            : debug(_debug)
            , NUM_PROCESSES(numProcesses){
        VERBOSE DEBUG std::cout<<"constructor allocator_interface"<<std::endl;
    }
    ~allocator_interface() {
        VERBOSE DEBUG std::cout<<"destructor allocator_interface"<<std::endl;
    }
};

#endif
