/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef ALLOC_NEW_SEGREGATED_H
#define	ALLOC_NEW_SEGREGATED_H

#include "plaf.h"
#include "pool_interface.h"
#include <cstdlib>
#include <cassert>
#include <iostream>
#include <dlfcn.h>
#include <pthread.h>
using namespace std;

//__thread long long currentAllocatedBytes = 0;
//__thread long long maxAllocatedBytes = 0;

template<typename T = void>
class allocator_new_segregated : public allocator_interface<T> {
private:
    void* (*allocfn)(size_t size);
    void (*freefn)(void *ptr);
    
public:
    template<typename _Tp1>
    struct rebind {
        typedef allocator_new_segregated<_Tp1> other;
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
        return (T*) allocfn(sizeof(T));
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
        p->~T(); // explicitly call destructor, since we lose automatic destructor calls when we bypass new/delete([])
        freefn(p);
#endif
    }
    void deallocateAndClear(const int tid, blockbag<T> * const bag) {
#if defined NO_FREE
        bag->clearWithoutFreeingElements();
#else
        while (!bag->isEmpty()) {
            T* ptr = bag->remove();
            deallocate(tid, ptr);
        }
#endif
    }
    
    void debugPrintStatus(const int tid) {}
    
    void initThread(const int tid) {}
    
    static void* dummy_thr(void *p) { return 0; }
    
    allocator_new_segregated(const int numProcesses, debugInfo * const _debug)
            : allocator_interface<T>(numProcesses, _debug) {
        VERBOSE DEBUG std::cout<<"constructor allocator_new_segregated"<<std::endl;
        
	char *lib = getenv("TREE_MALLOC");
	if (!lib) {
		printf("no TREE_MALLOC defined: using default!\n");
                allocfn = malloc;
                freefn = free;
		return;
	}
	void *h = dlopen(lib, RTLD_NOW | RTLD_LOCAL);
	if (!h) {
		fprintf(stderr, "unable to load '%s': %s\n", lib, dlerror());
		exit(1);
	}

	// If the allocator exports pthread_create(), we assume it does so to detect
	// multi-threading (through interposition on pthread_create()) and so call
	// this function (since it might not be called otherwise, if the standard
	// allocator does a similar trick).
	int (*pthread_create)(pthread_t *, const pthread_attr_t *, void *(*) (void *), void *);
	pthread_create = (__typeof(pthread_create)) dlsym(h, "pthread_create");
	if (pthread_create) {
		pthread_t thr;
		pthread_create(&thr, NULL, dummy_thr, NULL);
		pthread_join(thr, NULL);
	}
	allocfn = (__typeof(allocfn)) dlsym(h, "malloc");
	if (!allocfn) {
		fprintf(stderr, "unable to resolve malloc\n");
		exit(1);
	}
	freefn = (__typeof(freefn)) dlsym(h, "free");
	if (!freefn) {
		fprintf(stderr, "unable to resolve free\n");
		exit(1);
	}
    }
    ~allocator_new_segregated() {
        VERBOSE DEBUG std::cout<<"destructor allocator_new_segregated"<<std::endl;
    }
};

#endif	/* ALLOC_NEW_SEGREGATED_H */

