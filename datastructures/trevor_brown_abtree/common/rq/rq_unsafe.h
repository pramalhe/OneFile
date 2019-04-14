/* 
 * File:   rq_unsafe.h
 * Author: trbot
 *
 * Created on May 15, 2017, 5:06 PM
 */

#ifndef RQ_UNSAFE_H
#define	RQ_UNSAFE_H

#include "rq_debugging.h"
#include <rwlock.h>
#include <pthread.h>

#ifndef casword_t
#define casword_t uintptr_t
#endif

template <typename K, typename V, typename NodeType, typename DataStructure, typename RecordManager, bool logicalDeletion, bool canRetireNodesLogicallyDeletedByOtherProcesses>
class RQProvider {
private:
    struct __rq_thread_data {
        #define __RQ_THREAD_DATA_SIZE 1024
        union {
            struct { // anonymous struct inside anonymous union means we don't need to type anything special to access these variables
                long long rq_lin_time;
            };
            char bytes[__RQ_THREAD_DATA_SIZE]; // avoid false sharing
        };
    } __attribute__((aligned(__RQ_THREAD_DATA_SIZE)));

    #define TIMESTAMP_NOT_SET 0
    
    const int NUM_PROCESSES;
    volatile char padding0[PREFETCH_SIZE_BYTES];
    volatile long long timestamp = 1;
    volatile char padding1[PREFETCH_SIZE_BYTES];
    RWLock rwlock;
    volatile char padding2[PREFETCH_SIZE_BYTES];
    __rq_thread_data * threadData;
    
    DataStructure * ds;
    RecordManager * const recmgr;

    int init[MAX_TID_POW2] = {0,};

public:
    RQProvider(const int numProcesses, DataStructure * ds, RecordManager * recmgr) : NUM_PROCESSES(numProcesses), ds(ds), recmgr(recmgr) {
        threadData = new __rq_thread_data[numProcesses];
        DEBUG_INIT_RQPROVIDER(numProcesses);
    }

    ~RQProvider() {
        delete[] threadData;
        DEBUG_DEINIT_RQPROVIDER(NUM_PROCESSES);
    }

    // invoke before a given thread can perform any rq_functions
    void initThread(const int tid) {
        if (init[tid]) return; else init[tid] = !init[tid];

        DEBUG_INIT_THREAD(tid);
    }

    // invoke once a given thread will no longer perform any rq_ functions
    void deinitThread(const int tid) {
        if (!init[tid]) return; else init[tid] = !init[tid];

        DEBUG_DEINIT_THREAD(tid);
    }

    // invoke whenever a new node is created/initialized
    inline void init_node(const int tid, NodeType * const node) {}

    // for each address addr that is modified by rq_linearize_update_at_write
    // or rq_linearize_update_at_cas, you must replace any initialization of addr
    // with invocations of rq_write_addr
    template <typename T>
    inline void write_addr(const int tid, T volatile * const addr, const T val) {
        *addr = val;
    }

    // for each address addr that is modified by rq_linearize_update_at_write
    // or rq_linearize_update_at_cas, you must replace any reads of addr with
    // invocations of rq_read_addr
    template <typename T>
    inline T read_addr(const int tid, T volatile * const addr) {
        return *addr;
    }
    
    // IF DATA STRUCTURE PERFORMS LOGICAL DELETION
    // run some time BEFORE the physical deletion of a node
    // whose key has ALREADY been logically deleted.
    inline void announce_physical_deletion(const int tid, NodeType * const * const deletedNodes) {}

    // IF DATA STRUCTURE PERFORMS LOGICAL DELETION
    // run AFTER performing announce_physical_deletion,
    // if the cas that was trying to physically delete node failed.
    inline void physical_deletion_failed(const int tid, NodeType * const * const deletedNodes) {}
    
    // IF DATA STRUCTURE PERFORMS LOGICAL DELETION
    // run AFTER performing announce_physical_deletion,
    // if the cas that was trying to physically delete node succeeded.
    inline void physical_deletion_succeeded(const int tid, NodeType * const * const deletedNodes) {
        int i;
        for (i=0;deletedNodes[i];++i) {
            recmgr->retire(tid, deletedNodes[i]);
        }
    }

    // replace the linearization point of an update that inserts or deletes nodes
    // with an invocation of this function if the linearization point is a WRITE
    template <typename T>
    inline T linearize_update_at_write(
            const int tid,
            T volatile * const lin_addr,
            const T& lin_newval,
            NodeType * const * const insertedNodes,
            NodeType * const * const deletedNodes) {

        if (!logicalDeletion) {
            // physical deletion will happen at the same time as logical deletion
            announce_physical_deletion(tid, deletedNodes);
        }
        
#ifdef RQ_USE_TIMESTAMPS
        rwlock.readLock();
        long long ts = timestamp;
#else
        long long ts = 1;
#endif

        *lin_addr = lin_newval; // original linearization point
#ifdef RQ_USE_TIMESTAMPS
        rwlock.readUnlock();
#endif

        if (!logicalDeletion) {
            // physical deletion will happen at the same time as logical deletion
            physical_deletion_succeeded(tid, deletedNodes);
        }
        
#if defined USE_RQ_DEBUGGING
        DEBUG_RECORD_UPDATE_CHECKSUM<K,V>(tid, ts, insertedNodes, deletedNodes, ds);
#endif
        return lin_newval;
    }

    // replace the linearization point of an update that inserts or deletes nodes
    // with an invocation of this function if the linearization point is a CAS
    template <typename T>
    inline T linearize_update_at_cas(
            const int tid,
            T volatile * const lin_addr,
            const T& lin_oldval,
            const T& lin_newval,
            NodeType * const * const insertedNodes,
            NodeType * const * const deletedNodes) {

        if (!logicalDeletion) {
            // physical deletion will happen at the same time as logical deletion
            announce_physical_deletion(tid, deletedNodes);
        }
        
#ifdef RQ_USE_TIMESTAMPS
        rwlock.readLock();
        long long ts = timestamp;
#else
        long long ts = 1;
#endif
        T res = __sync_val_compare_and_swap(lin_addr, lin_oldval, lin_newval); // original linearization point
#ifdef RQ_USE_TIMESTAMPS
        rwlock.readUnlock();
#endif

        if (res == lin_oldval) {
            if (!logicalDeletion) {
                // physical deletion will happen at the same time as logical deletion
                physical_deletion_succeeded(tid, deletedNodes);
            }

#if defined USE_RQ_DEBUGGING
            DEBUG_RECORD_UPDATE_CHECKSUM<K,V>(tid, ts, insertedNodes, deletedNodes, ds);
#endif
        } else {
            if (!logicalDeletion) {
                // physical deletion will happen at the same time as logical deletion
                physical_deletion_failed(tid, deletedNodes);
            }
        }
        
        return res;
    }

    // invoke at the start of each traversal
    inline void traversal_start(const int tid) {
#ifdef RQ_USE_TIMESTAMPS
        rwlock.writeLock();
        threadData[tid].rq_lin_time = ++timestamp; // linearization point of range query (at the write to timestamp)
        rwlock.writeUnlock();
#endif
    }

    // invoke each time a traversal visits a node with a key in the desired range:
    // if the node belongs in the range query, it will be placed in rqResult[index]
    inline void traversal_try_add(const int tid, NodeType * const node, K * const rqResultKeys, V * const rqResultValues, int * const startIndex, const K& lo, const K& hi) {
        int start = (*startIndex);
        int keysInNode = ds->getKeys(tid, node, rqResultKeys+start, rqResultValues+start);
        assert(keysInNode < RQ_DEBUGGING_MAX_KEYS_PER_NODE);
        if (keysInNode == 0) return;
        int location = start; 
        for (int i=start;i<keysInNode+start;++i) {
            if (ds->isInRange(rqResultKeys[i], lo, hi)){
                rqResultKeys[location] = rqResultKeys[i];
                rqResultValues[location] = rqResultValues[i];
                ++location;
            }   
        }
        *startIndex = location;
#if defined MICROBENCH
        assert(*startIndex <= RQSIZE);
#endif
    }

    // invoke at the end of each traversal:
    // any nodes that were deleted during the traversal,
    // and were consequently missed during the traversal,
    // are placed in rqResult[index]
    inline void traversal_end(const int tid, K * const rqResultKeys, V * const rqResultValues, int * const startIndex, const K& lo, const K& hi) {
        DEBUG_RECORD_RQ_SIZE(*startIndex);
        DEBUG_RECORD_RQ_CHECKSUM(tid, threadData[tid].rq_lin_time, rqResultKeys, *startIndex);
    }
};

#endif	/* RQ_UNSAFE_H */

