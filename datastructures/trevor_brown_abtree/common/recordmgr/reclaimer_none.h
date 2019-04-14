/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef RECLAIM_NOOP_H
#define	RECLAIM_NOOP_H

#include <cassert>
#include <iostream>
#include "pool_interface.h"
#include "reclaimer_interface.h"
using namespace std;

template <typename T = void, class Pool = pool_interface<T> >
class reclaimer_none : public reclaimer_interface<T, Pool> {
private:
public:
    template<typename _Tp1>
    struct rebind {
        typedef reclaimer_none<_Tp1, Pool> other;
    };
    template<typename _Tp1, typename _Tp2>
    struct rebind2 {
        typedef reclaimer_none<_Tp1, _Tp2> other;
    };
    
    string getSizeString() { return "no reclaimer"; }
    inline static bool shouldHelp() {
        return true;
    }
    
    inline static bool isQuiescent(const int tid) {
        return true;
    }
    inline static bool isProtected(const int tid, T * const obj) {
        return true;
    }
    inline static bool isQProtected(const int tid, T * const obj) {
        return false;
    }
    
    // for hazard pointers (and reference counting)
    inline static bool protect(const int tid, T * const obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool memoryBarrier = true) {
        return true;
    }
    inline static void unprotect(const int tid, T * const obj) {}
    inline static bool qProtect(const int tid, T * const obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool memoryBarrier = true) {
        return true;
    }
    inline static void qUnprotectAll(const int tid) {}
    
    // rotate the epoch bags and reclaim any objects retired two epochs ago.
    inline static void rotateEpochBags(const int tid) {
    }
    // invoke this at the beginning of each operation that accesses
    // objects reclaimed by this epoch manager.
    // returns true if the call rotated the epoch bags for thread tid
    // (and reclaimed any objects retired two epochs ago).
    // otherwise, the call returns false.
    inline static bool leaveQuiescentState(const int tid, void * const * const reclaimers, const int numReclaimers) {
        return false;
    }
    inline static void enterQuiescentState(const int tid) {
    }
    
    // for all schemes except reference counting
    inline static void retire(const int tid, T* p) {
    }

    void debugPrintStatus(const int tid) {
    }

//    set_of_bags<T> getBlockbags() {
//        set_of_bags<T> empty = {.bags = NULL, .numBags = 0};
//        return empty;
//    }
//    
//    void getOldestTwoBlockbags(const int tid, blockbag<T> ** oldest, blockbag<T> ** secondOldest) {
//        *oldest = *secondOldest = NULL;
//    }
//    
//    int getOldestBlockbagIndexOffset(const int tid) {
//        return -1;
//    }
    
    void getSafeBlockbags(const int tid, blockbag<T> ** bags) {
        bags[0] = NULL;
    }
    
    reclaimer_none(const int numProcesses, Pool *_pool, debugInfo * const _debug, RecoveryMgr<void *> * const _recoveryMgr = NULL)
            : reclaimer_interface<T, Pool>(numProcesses, _pool, _debug, _recoveryMgr) {
        VERBOSE DEBUG std::cout<<"constructor reclaimer_none"<<std::endl;
    }
    ~reclaimer_none() {
        VERBOSE DEBUG std::cout<<"destructor reclaimer_none"<<std::endl;
    }

}; // end class

#endif

