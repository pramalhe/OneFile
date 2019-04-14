/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef RECLAIM_HAZARDPTR_STACK_H
#define	RECLAIM_HAZARDPTR_STACK_H

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include "blockbag.h"
#include "plaf.h"
#include "allocator_interface.h"
#include "hashtable.h"
#include "reclaimer_interface.h"
#include "arraylist.h"
using namespace std;
using namespace hashset_namespace;

#define MAX_HAZARDPTRS_PER_THREAD 16

template <typename T = void, class Pool = pool_interface<T> >
class reclaimer_hazardptr : public reclaimer_interface<T, Pool> {
private:
    AtomicArrayList<T> **announce;  // announce[tid] = set of announced hazard pointers for thread tid
    ArrayList<T> **retired;         // retired[tid] = set of retired objects for thread tid
    hashset_new<T> **comparing;     // comparing[tid] = set of announced hazard pointers for ALL threads, as collected by thread tid during it's last retire(tid, ...) call
    
    // number of elements that retired[tid] must contain
    // before we scan hazard pointers to determine
    // which elements of retired[tid] can be deallocated.
    // to get amortized constant scanning time per object,
    // this must be nk+Omega(nk), where
    //      n = number of threads and
    //      k = max number of hazard pointers a thread can hold at once
    const int scanThreshold;
    
public:
    template<typename _Tp1>
    struct rebind {
        typedef reclaimer_hazardptr<_Tp1, Pool> other;
    };
    template<typename _Tp1, typename _Tp2>
    struct rebind2 {
        typedef reclaimer_hazardptr<_Tp1, _Tp2> other;
    };
    
    inline static bool shouldHelp() {
        return false;
    }
    
    bool isProtected(const int tid, T * const obj) {
        return announce[tid]->contains(obj);
    }
    bool static isQProtected(const int tid, T * const obj) {
        return false;
    }
    inline static bool isQuiescent(const int tid) {
        return true;
    }    
    
    // for hazard pointers (and counting references from threads)
    inline bool protect(const int tid, T * const obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool memoryBarrier = true) {
        TRACE std::cout<<"reclaimer_hazardptr::protect(tid="<<tid<<", "<<debugPointerOutput(obj)<<")"<<std::endl;
        int size; DEBUG2 size = announce[tid]->size();
//        DEBUG if (sizeof(T) < 80 /* is a node */) assert(!announce[tid]->contains(obj));
        announce[tid]->add(obj);
        if (memoryBarrier) __sync_synchronize(); // prevent retired from being read before we set a hazard pointer to obj
        DEBUG2 assert(isProtected(tid, obj)); //announce[tid]->contains(obj));
        DEBUG2 assert(size + 1 == announce[tid]->size());
//        SOFTWARE_BARRIER;
        if (notRetiredCallback(callbackArg)) {
//            SOFTWARE_BARRIER;
            TRACE std::cout<<"notRetiredCallback returns true"<<std::endl;
            DEBUG2 assert(announce[tid]->size() <= MAX_HAZARDPTRS_PER_THREAD);
            DEBUG2 assert(isProtected(tid, obj));
//            SOFTWARE_BARRIER;
            assert(isProtected(tid, obj));
            return true;
        } else {
            TRACE std::cout<<"notRetiredCallback returns false"<<std::endl;
            unprotect(tid, obj); // note: it is unnecessary to unprotect here if we promise to enter a quiescent state as soon as we fail to protect an object.
//            DEBUG if (sizeof(T) < 80 /* is a node */) assert(!isProtected(tid, obj));
//            SOFTWARE_BARRIER;
            return false;
        }
    }
    inline void unprotect(const int tid, T * const obj) {
        TRACE std::cout<<"reclaimer_hazardptr::unprotect(tid="<<tid<<", "<<debugPointerOutput(obj)<<")"<<std::endl;
//        SOFTWARE_BARRIER;
        DEBUG2 assert(isProtected(tid, obj));
        int size; DEBUG2 size = announce[tid]->size();
        announce[tid]->erase(obj);
//        DEBUG if (sizeof(T) < 80 /* is a node */) assert(!announce[tid]->contains(obj));
        DEBUG2 assert(size - 1 == announce[tid]->size());
//        SOFTWARE_BARRIER;
    }
    inline bool qProtect(const int tid, T * const obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool memoryBarrier = true) {
        TRACE std::cout<<"reclaimer_debraplus::qProtect(tid="<<tid<</*", "<<*obj<<*/")"<<std::endl;
        return false;
    }
    inline void qUnprotectAll(const int tid) {
        TRACE std::cout<<"reclaimer_debraplus::qUnprotectAll(tid="<<tid<<")"<<std::endl;
    }
    
    // for epoch based reclamation
    inline void enterQuiescentState(const int tid) {
        TRACE std::cout<<"reclaimer_hazardptr::enterQuiescentState(tid="<<tid<<")"<<std::endl;
//        SOFTWARE_BARRIER;
        announce[tid]->clear();
//        __sync_synchronize();
//        announce[tid]->clearWithoutFreeingElements();
        DEBUG2 assert(announce[tid]->size() == 0);
        DEBUG2 assert(announce[tid]->isEmpty());
//        SOFTWARE_BARRIER;
    }
    inline static bool leaveQuiescentState(const int tid, void * const * const reclaimers, const int numReclaimers) {
        TRACE std::cout<<"reclaimer_hazardptr::leaveQuiescentState(tid="<<tid<<")"<<std::endl;
//        SOFTWARE_BARRIER;
        return false;
    }
    inline static void rotateEpochBags(const int tid) {}
    
    string debugPointerOutput(T* p) {
        long x = (long) p;
        ostringstream os;
        const int base = 10+26+26;
        while (x > 0) {
            int c = x % base;
            if (c < 10) os<<(char)(c+(int)'0');
            else if (c < 10+26) os<<(char)(c-10+(int)'a');
            else os<<(char)(c-10-26+(int)'A');
            x /= base;
        }
        return os.str();
    }
    
    inline void retire(const int tid, T* p) {
        TRACE std::cout<<"reclaimer_hazardptr::retire(tid="<<tid<<", "<<debugPointerOutput(p)<<")"<<std::endl;
        DEBUG2 this->debug->addRetired(tid, 1);
        retired[tid]->add(p);
        
        // if the retired bag is sufficiently large
        if (retired[tid]->isFull()) {
//            __sync_synchronize(); // not necessary, since there is a membar implied by the update cas between here and the marked bit that makes the retired predicate return true... (it follows that the retired predicate for a node u will see marked and return true if it executes when we are performing retire(u).)
            
//            TRACE std::cout<<"retiring... we have "<<retired[tid]->size()<<" things waiting to be retired (#hps="<<announce[tid]->size()<<")...";
//            // hash all announcements
//            int totalSize = 0;
//            int sizes[MAX_TID_POW2];
//            for (int otherTid=0; otherTid < this->NUM_PROCESSES; ++otherTid) {
//                sizes[otherTid] = announce[tid]->size();
//                totalSize += sizes[otherTid];
//            }
//            hashset_new<T> hset = hashset_new<T>(totalSize);
//            for (int otherTid=0; otherTid < this->NUM_PROCESSES; ++otherTid) {
//                for (int i=0;i<sizes[tid];++i) {
//                    hset.insert(announce[tid]->get(i));
//                }
//            }
//            
//            // iterate over all items in retired[tid]
//            TRACE std::cout<<"retiring... we have "<<retired[tid]->size()<<" things waiting to be retired (#hps="<<announce[tid]->size()<<", totalSize="<<totalSize<<")...";
//            for (int ix=0;ix<retired[tid]->size();++ix) {
//                TRACE std::cout<<" "<<debugPointerOutput(retired[tid]->get(ix))<<"="<<(hset.contains(retired[tid]->get(ix))?"1":"0");
//                if (!hset.contains(retired[tid]->get(ix))) {
//                    // no hazard pointers point to the item, so we send it to the pool
//                    this->pool->add(tid, retired[tid]->get(ix));
//                    // now we remove the item from retired[tid] and
//                    // adjust ix to continue where we left off
//                    retired[tid]->erase(ix);
//                    --ix;
//                }
//            }
//            TRACE std::cout<<"    afterwards, we have "<<retired[tid]->size()<<" things waiting to be retired..."<<std::endl;

//            TRACE std::cout<<"retiring... we have "<<retired[tid]->size()<<" things waiting to be retired (THIS thread #hps="<<announce[tid]->size()<<")...";
//            for (int ix=0;ix<retired[tid]->size();) {
//                // check if retired[tid]->data[ix] is in any set of hazard pointers
//                bool found = false;
//                for (int otherTid=0;otherTid<this->NUM_PROCESSES;++otherTid) {
//                    int sz = announce[otherTid]->size();
//                    for (int ixHP=0;ixHP<sz;++ixHP) {
//                        if (retired[tid]->get(ix) == announce[otherTid]->get(ixHP)) {
//                            found = true;
//                            // break out of both loops
//                            otherTid = this->NUM_PROCESSES;
//                            break;
//                        }
//                    }
//                }
//                if (!found) {
//                    // no hazard pointers point to the item, so we send it to the pool
//                    this->pool->add(tid, retired[tid]->get(ix));
//                    // now we remove the item from retired[tid]
//                    retired[tid]->erase(ix);
//                } else {
//                    ++ix; // we didn't erase, so we need to move on to the next element
//                }
//            }
//            TRACE std::cout<<"    afterwards, we have "<<retired[tid]->size()<<" things waiting to be retired..."<<std::endl;

            TRACE std::cout<<"retiring... we have "<<retired[tid]->size()<<" things waiting to be retired (THIS thread #hps="<<announce[tid]->size()<<")...";
            // hash all announcements
            comparing[tid]->clear();
            assert(comparing[tid]->size() == 0);
            for (int otherTid=0; otherTid < this->NUM_PROCESSES; ++otherTid) {
                int sz = announce[otherTid]->size();
                assert(sz < MAX_HAZARDPTRS_PER_THREAD);
                for (int ixHP=0;ixHP<sz;++ixHP) {
                    int oldSize; DEBUG2 oldSize = comparing[tid]->size();
                    comparing[tid]->insert(announce[otherTid]->get(ixHP));
                    DEBUG2 assert(comparing[tid]->size() <= oldSize + 1); // might not increase size if comparing[tid] already contains this item...
                }
            }
            for (int ix=0;ix<retired[tid]->size();) {
                // check if retired[tid]->data[ix] is in any set of hazard pointers
                if (!comparing[tid]->contains(retired[tid]->get(ix))) {
                    // no hazard pointers point to the item, so we send it to the pool
                    this->pool->add(tid, retired[tid]->get(ix));
                    // now we remove the item from retired[tid]
                    retired[tid]->erase(ix);
                } else {
                    ++ix; // we didn't erase, so we need to move on to the next element
                }
            }
            TRACE std::cout<<"    afterwards, we have "<<retired[tid]->size()<<" things waiting to be retired..."<<std::endl;
            
            DEBUG2 assert(!retired[tid]->isFull());
        }
    }

    void debugPrintStatus(const int tid) {
//        assert(tid >= 0);
//        assert(tid < this->NUM_PROCESSES);
    }

    reclaimer_hazardptr(const int numProcesses, Pool *_pool, debugInfo * const _debug, RecoveryMgr<void *> * const _recoveryMgr = NULL)
            : scanThreshold(5*numProcesses*MAX_HAZARDPTRS_PER_THREAD),
              reclaimer_interface<T, Pool>(numProcesses, _pool, _debug, _recoveryMgr) {
        VERBOSE DEBUG std::cout<<"constructor reclaimer_hazardptr"<<std::endl;
        announce = new AtomicArrayList<T>*[numProcesses];
        retired = new ArrayList<T>*[numProcesses];
        comparing = new hashset_new<T>*[numProcesses];
        for (int tid=0;tid<numProcesses;++tid) {
            announce[tid] = new AtomicArrayList<T>(MAX_HAZARDPTRS_PER_THREAD);
            retired[tid] = new ArrayList<T>(scanThreshold);
            comparing[tid] = new hashset_new<T>(numProcesses*MAX_HAZARDPTRS_PER_THREAD);
        }
    }
    ~reclaimer_hazardptr() {
        VERBOSE DEBUG std::cout<<"destructor reclaimer_hazardptr"<<std::endl;
        for (int tid=0;tid<this->NUM_PROCESSES;++tid) {
            int sz = retired[tid]->size();
            for (int ix=0;ix<sz;++ix) {
                this->pool->add(tid, retired[tid]->get(ix));
            }
            delete announce[tid];
            delete retired[tid];
            delete comparing[tid];
        }
        delete[] announce;
        delete[] retired;
        delete[] comparing;
    }

}; // end class

#endif

