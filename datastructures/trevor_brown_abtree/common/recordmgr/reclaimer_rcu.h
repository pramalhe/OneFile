/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

// NOTE: this reclaimer can ONLY be used with allocator_new,
//       and cannot be used with any pool!

#ifndef RECLAIM_RCU_H
#define	RECLAIM_RCU_H

#include <cassert>
#include <iostream>
#include <sstream>
#include <urcu.h>
#include "blockbag.h"
#include "plaf.h"
#ifdef USE_DEBUGCOUNTERS
    #include "debugcounter.h"
#endif
#include "allocator_interface.h"
#include "reclaimer_interface.h"
#ifdef BST
    #include "node.h"
    #include "scxrecord.h"
#elif defined KCAS_MAXK
    #include "kcas.h"
#else
    #error ONLY SUPPORTS BST(main.cpp) and KCAS(ubench.cpp)
#endif
using namespace std;

#include <cstddef>

template <typename T, typename M> M get_member_type(M T::*);
template <typename T, typename M> T get_class_type(M T::*);

template <typename T,
          typename R,
          R T::*M
         >
constexpr std::size_t offset_of()
{
    return reinterpret_cast<std::size_t>(&(((T*)0)->*M));
}

#define OFFSET_OF(m) offset_of<decltype(get_class_type(m)), \
                     decltype(get_member_type(m)), m>()

#define comma ,

__thread long long rcuthrFreesNode = 0;       // for RCU THREADS ONLY
__thread long long rcuthrFreesDescriptor = 0; // for RCU THREADS ONLY
long long freesNode = 0;
long long freesDescriptor = 0;

#if defined BST || defined BST_THROWAWAY
    void rcuCallback_Node(struct rcu_head *rcu) {
        Node<test_type, test_type> * n = (Node<test_type, test_type> *)
                (((char*) rcu) - OFFSET_OF(
                        &Node<test_type comma test_type>::rcuHeadField));
        if (++rcuthrFreesNode == 1<<10) {
            __sync_fetch_and_add(&freesNode, rcuthrFreesNode);
            rcuthrFreesNode = 0;
        }
        free(n);
    }
    #ifdef BST_THROWAWAY
        void rcuCallback_SCXRecord(struct rcu_head *rcu) {
            SCXRecord<test_type, test_type> * n = (SCXRecord<test_type, test_type> *)
                    (((char*) rcu) - OFFSET_OF(
                            &SCXRecord<test_type comma test_type>::rcuHeadField));
            if (++rcuthrFreesDescriptor == 1<<10) {
                __sync_fetch_and_add(&freesDescriptor, rcuthrFreesDescriptor);
                rcuthrFreesDescriptor = 0;
            }
            free(n);
        }
    #endif
#elif defined KCAS_MAXK
    void rcuCallback_kcasdesc(struct rcu_head *rcu) {
        kcasdesc_t<KCAS_MAXK, KCAS_MAXTHREADS> * n = (kcasdesc_t<KCAS_MAXK, KCAS_MAXTHREADS> *)
                (((char*) rcu) - OFFSET_OF(
                        &kcasdesc_t<KCAS_MAXK comma KCAS_MAXTHREADS>::rcuHeadField));
        if (++rcuthrFreesDescriptor == 1<<10) {
            __sync_fetch_and_add(&freesDescriptor, rcuthrFreesDescriptor);
            rcuthrFreesDescriptor = 0;
        }
        free(n);
    }
    void rcuCallback_rdcssdesc(struct rcu_head *rcu) {
        rdcssdesc_t * n = (rdcssdesc_t *)
                (((char*) rcu) - OFFSET_OF(
                        &rdcssdesc_t::rcuHeadField));
        if (++rcuthrFreesNode == 1<<10) {
            __sync_fetch_and_add(&freesNode, rcuthrFreesNode);
            rcuthrFreesNode = 0;
        }
        free(n);
    }
#endif

//template <typename T>
//void rcuCallback(struct rcu_head *rcu) {
//    T * n = (T *) (((char *) rcu) - OFFSET_OF(&T::rcuHeadField));
//    free(n);
//}

__thread bool calledRCULock = false;
__thread bool rcuInitialized = false;

template <typename T = void, class Pool = pool_interface<T> >
class reclaimer_rcu : public reclaimer_interface<T, Pool> {
protected:
    
public:
    template<typename _Tp1>
    struct rebind {
        typedef reclaimer_rcu<_Tp1, Pool> other;
    };
    template<typename _Tp1, typename _Tp2>
    struct rebind2 {
        typedef reclaimer_rcu<_Tp1, _Tp2> other;
    };
    
    long long getSizeInNodes() {
        long long sum = 0;
        return sum;
    }
    string getSizeString() {
        stringstream ss;
        ss<<getSizeInNodes()<<" in reclaimer_rcu";
        return ss.str();
    }
    
    inline static bool quiescenceIsPerRecordType() { return false; }
    
    inline bool isQuiescent(const int tid) {
        return false;
    }

    inline static bool isProtected(const int tid, T * const obj) {
        return true;
    }
    inline static bool isQProtected(const int tid, T * const obj) {
        return false;
    }
    inline static bool protect(const int tid, T * const obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool memoryBarrier = true) {
        return true;
    }
    inline static void unprotect(const int tid, T * const obj) {}
    inline static bool qProtect(const int tid, T * const obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool memoryBarrier = true) {
        return true;
    }
    inline static void qUnprotectAll(const int tid) {}
    
    inline static bool shouldHelp() { return true; }
    
    inline void rotateEpochBags(const int tid) {}
    
    inline bool leaveQuiescentState(const int tid, void * const * const reclaimers, const int numReclaimers) {
        if (!calledRCULock) {
            rcu_read_lock();
            calledRCULock = true;
        }
        return true;
    }
    
    inline void enterQuiescentState(const int tid) {
        if (calledRCULock) {
            rcu_read_unlock();
            calledRCULock = false;
        }
    }
    
    // for all schemes except reference counting
    inline void retire(const int tid, T* p) {
//        call_rcu(&p->rcuHeadField, rcuCallback<T>);
#if defined BST || defined BST_THROWAWAY
        if (sizeof(*p) == sizeof(Node<test_type, test_type>)) {
            call_rcu(&p->rcuHeadField, rcuCallback_Node);
#ifdef BST_THROWAWAY
        } else if (sizeof(*p) == sizeof(SCXRecord<test_type, test_type>)) {
            call_rcu(&p->rcuHeadField, rcuCallback_SCXRecord);
#endif
        }
#elif defined KCAS_MAXK
        if (sizeof(*p) == sizeof(kcasdesc_t<KCAS_MAXK comma KCAS_MAXTHREADS>)) {
            call_rcu(&p->rcuHeadField, rcuCallback_kcasdesc);
        } else if (sizeof(*p) == sizeof(rdcssdesc_t)) {
            call_rcu(&p->rcuHeadField, rcuCallback_rdcssdesc);
        }
#endif
    }

    void debugPrintStatus(const int tid) {
        if (freesNode) std::cout<<"freesNode="<<freesNode<<std::endl;
        if (freesDescriptor) std::cout<<"freesDescriptor="<<freesDescriptor<<std::endl;
    }
    
    void initThread(const int tid) {
        if (!rcuInitialized) {
            rcu_register_thread();
            struct call_rcu_data * crdp = create_call_rcu_data(0,-1);
            set_thread_call_rcu_data(crdp);
            rcuInitialized = true;
        }
    }
    
    void deinitThread(const int tid) {
        if (rcuInitialized) {
            rcu_unregister_thread();
            rcuInitialized = false;
        }
    }

    reclaimer_rcu(const int numProcesses, Pool *_pool, debugInfo * const _debug, RecoveryMgr<void *> * const _recoveryMgr = NULL)
            : reclaimer_interface<T, Pool>(numProcesses, _pool, _debug, _recoveryMgr) {
        rcu_init();
    }
    ~reclaimer_rcu() {}
};

#endif

