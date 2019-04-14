/* 
 * File:   rq_rwlock.h
 * Author: trbot
 *
 * Created on April 20, 2017, 1:03 PM
 * 
 * Implementation of Shahar Timnat's Iterator algorithm.
 * 
 * WARNING:
 * 1. Shahar's algorithm ONLY supports data structures with logical deletion.
 * 2. It only supports data structures where each node contains ONE key.
 * 3. It only supports taking a snapshot of the entire data structure.
 * 4. It only supports insertion, deletion and search operations.
 *    If a data structure has other operations, it might not be linearizable.
 */

#ifndef RQ_RWLOCK_H
#define	RQ_RWLOCK_H

#define MAX_NODES_DELETED_ATOMICALLY 1
#define MAX_KEYS_PER_NODE 1

#include "errors.h"
#include "rq_debugging.h"
#include <record_manager.h>
#include <pthread.h>
#include <cassert>
#include "snapcollector.h"

template <typename K, typename V, typename NodeType, typename DataStructure, typename RecordManager, bool logicalDeletion, bool canRetireNodesLogicallyDeletedByOtherProcesses>
class RQProvider {
private:
    struct __rq_thread_data {
        #define __RQ_THREAD_DATA_SIZE 1024
        union {
            struct { // anonymous struct inside anonymous union means we don't need to type anything special to access these variables
                long long rq_lin_time;
                SnapCollector<NodeType,K> * currentSnapCollector;
                SnapCollector<NodeType,K> * snapCollectorToRetire;
            };
            char bytes[__RQ_THREAD_DATA_SIZE]; // avoid false sharing
        };
    } __attribute__((aligned(__RQ_THREAD_DATA_SIZE)));

    const int NUM_PROCESSES;
    volatile long long timestamp = 1;
    pthread_rwlock_t rwlock;
    __rq_thread_data * threadData;
    
    DataStructure * const ds;
    RecordManager * const recmgr;
    volatile char padding[PREFETCH_SIZE_BYTES];
    SnapCollector<NodeType,K> * volatile snapPointer;
    
    int init[MAX_TID_POW2] = {0,};

public:
    RQProvider(const int numProcesses, DataStructure * ds, RecordManager * recmgr) : NUM_PROCESSES(numProcesses), ds(ds), recmgr(recmgr) {
        assert(logicalDeletion); // Timnat's iterator algorithm REQUIRES logical deletion!
        if (pthread_rwlock_init(&rwlock, NULL)) error("could not init rwlock");
        threadData = new __rq_thread_data[numProcesses];
        
        const int dummyTid = 0;
        
        recmgr->initThread(dummyTid); // must initialize record manager before allocating!!
        initThread(dummyTid);
        
        // initialize dummy snap collector
        snapPointer = recmgr->template allocate<SnapCollector<NodeType,K> >(dummyTid);
#ifdef __HANDLE_STATS
        GSTATS_APPEND(dummyTid, extra_type1_allocated_addresses, ((long long) snapPointer)%(1<<12));
#endif
        snapPointer->init(dummyTid, numProcesses, recmgr, ds->KEY_MIN, ds->KEY_MAX+1);
        snapPointer->BlockFurtherPointers(dummyTid, recmgr);
        snapPointer->Deactivate(NULL, NULL, NULL);
        snapPointer->BlockFurtherReports();
        
        DEBUG_INIT_RQPROVIDER(numProcesses);
    }

    ~RQProvider() {
        if (pthread_rwlock_destroy(&rwlock)) error("could not destroy rwlock");
        delete[] threadData;
        snapPointer->retire(0 /* dummy tid */, recmgr);
        DEBUG_DEINIT_RQPROVIDER(NUM_PROCESSES);
    }

    // invoke before a given thread can perform any rq_functions
    void initThread(const int tid) {
        if (init[tid]) return; else init[tid] = !init[tid];

        threadData[tid].rq_lin_time = 0;
        threadData[tid].currentSnapCollector = NULL;
        threadData[tid].snapCollectorToRetire = NULL;
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

    /**
     * Added function only for Timnat's SnapCollector.
     * This must be invoked just before the return statement of every search.
     */
    inline void search_report_target_key(const int tid, const K key, NodeType * const node) {
        SnapCollector<NodeType,K> * sc = snapPointer;
        if (sc->IsActive()) {
            ReportType type = ds->isLogicallyDeleted(tid, node) ? ReportType::Remove : ReportType::Add;
            sc->Report(tid, node, type, key, recmgr);
        }
        SOFTWARE_BARRIER;
    }
    
    /**
     * Added function only for Timnat's SnapCollector.
     * This must be invoked just before the return statement of every insertion
     *      that does not modify the data structure.
     */
    inline void insert_readonly_report_target_key(const int tid, NodeType * const node) {
        SnapCollector<NodeType,K> * sc = snapPointer;
        if (sc->IsActive()) {
            if (!ds->isLogicallyDeleted(tid, node)) {
                sc->Report(tid, node, ReportType::Add, node->key, recmgr);
            }
        }
        SOFTWARE_BARRIER;
    }
    
    /**
     * Added function only for Timnat's SnapCollector.
     * This can be invoked to determine if the current SnapCollector is active.
     */
    inline bool traversal_is_active(const int tid) {
        return threadData[tid].currentSnapCollector->IsActive();
    }
    
private:
    inline void delete_report_target_key(const int tid, NodeType * const node) {
        if (node) {
            SnapCollector<NodeType,K> * sc = snapPointer;
            if (sc->IsActive()) {
                sc->Report(tid, node, ReportType::Remove, node->key, recmgr);
            }
            SOFTWARE_BARRIER;
        }
    }
    
public:
    
    // IF DATA STRUCTURE PERFORMS LOGICAL DELETION
    // run some time BEFORE the physical deletion of a node
    // whose key has ALREADY been logically deleted.
    inline void announce_physical_deletion(const int tid, NodeType * const * const deletedNodes) {
        assert(!deletedNodes[0] || !deletedNodes[1]);
        delete_report_target_key(tid, deletedNodes[0]);
    }

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
        
        assert((insertedNodes[0] && !deletedNodes[0])
                || (!insertedNodes[0] && deletedNodes[0]));

#ifdef RQ_USE_TIMESTAMPS
        if (pthread_rwlock_rdlock(&rwlock)) error("could not read-lock rwlock");
        long long ts = timestamp;
#else
        long long ts = 1;
#endif

        *lin_addr = lin_newval; // original linearization point
#ifdef RQ_USE_TIMESTAMPS
        if (pthread_rwlock_unlock(&rwlock)) error("could not read-unlock rwlock");
#endif
        
        if (insertedNodes[0]) insert_readonly_report_target_key(tid, insertedNodes[0]);
        if (deletedNodes[0]) delete_report_target_key(tid, deletedNodes[0]);

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

        assert((insertedNodes[0] && !deletedNodes[0])
                || (!insertedNodes[0] && deletedNodes[0]));

#ifdef RQ_USE_TIMESTAMPS
        if (pthread_rwlock_rdlock(&rwlock)) error("could not read-lock rwlock");
        long long ts = timestamp;
#else
        long long ts = 1;
#endif
        
        T res = __sync_val_compare_and_swap(lin_addr, lin_oldval, lin_newval);
#ifdef RQ_USE_TIMESTAMPS
        if (pthread_rwlock_unlock(&rwlock)) error("could not read-unlock rwlock");
#endif
        
        if (res == lin_oldval){
            if (insertedNodes[0]) insert_readonly_report_target_key(tid, insertedNodes[0]);
            if (deletedNodes[0]) delete_report_target_key(tid, deletedNodes[0]);

#if defined USE_RQ_DEBUGGING
            DEBUG_RECORD_UPDATE_CHECKSUM<K,V>(tid, ts, insertedNodes, deletedNodes, ds);
#endif
        }
        return res;
    }

    // invoke at the start of each traversal
    inline void traversal_start(const int tid) {
#if !defined(RQ_USE_TIMESTAMPS)
        threadData[tid].rq_lin_time = 1;
#endif        

        threadData[tid].currentSnapCollector = snapPointer;
        SOFTWARE_BARRIER;
        if (!threadData[tid].currentSnapCollector->IsActive()) {
            SnapCollector<NodeType,K> * candidate = recmgr->template allocate<SnapCollector<NodeType,K> >(tid);
#ifdef __HANDLE_STATS
            GSTATS_APPEND(tid, extra_type1_allocated_addresses, ((long long) candidate)%(1<<12));
#endif
            candidate->init(tid, NUM_PROCESSES, recmgr, ds->KEY_MIN, ds->KEY_MAX+1);
            if (__sync_bool_compare_and_swap(&snapPointer, threadData[tid].currentSnapCollector, candidate)) {
                // delay retiring until later, because we've started accepting reports,
                // and we don't want to waste time while we are accepting reports,
                // because we don't want to receive many reports...
                threadData[tid].snapCollectorToRetire = threadData[tid].currentSnapCollector;
                threadData[tid].currentSnapCollector = candidate;
            } else {
                candidate->retire(tid, recmgr);
                threadData[tid].currentSnapCollector = snapPointer;
            }
        }
//        usleep(200000);
    }

    inline NodeType * traversal_try_add(const int tid, NodeType * const node, K * const rqResultKeys, V * const rqResultValues, int * const startIndex, const K& lo, const K& hi) {
        SnapCollector<NodeType,K> * sc = threadData[tid].currentSnapCollector;
        return sc->AddNode(tid, node, node->key, recmgr);
    }
    
    // invoke at the end of each traversal:
    // any nodes that were deleted during the traversal,
    // and were consequently missed during the traversal,
    // are placed in rqResult[index]
    void traversal_end(const int tid, K * const rqResultKeys, V * const rqResultValues, int * const startIndex, const K& lo, const K& hi) {
        SnapCollector<NodeType,K> * sc = threadData[tid].currentSnapCollector;

        sc->BlockFurtherPointers(tid, recmgr);
        SOFTWARE_BARRIER;
        sc->Deactivate(NULL, NULL, NULL);
        sc->BlockFurtherReports();
        SOFTWARE_BARRIER;

        sc->Prepare(tid, recmgr);
        
        NodeType * curr = NULL;
        while ((curr = sc->GetNext(tid))) {
            if (curr->key < lo) continue;
            if (curr->key > hi) break;
            rqResultKeys[*startIndex] = curr->key;
            rqResultValues[*startIndex] = curr->val;
            ++*startIndex;
        }
#if defined MICROBENCH
        assert(*startIndex <= RQSIZE);
#endif
        
#ifdef SNAPCOLLECTOR_PRINT_RQS
//        for (int i=0;i<*startIndex;++i) {
//            cout<<" "<<rqResultKeys[i];
//        }
//        cout<<endl;
#endif
        
        DEBUG_RECORD_RQ_SIZE(*startIndex);
        DEBUG_RECORD_RQ_CHECKSUM(tid, threadData[tid].rq_lin_time, rqResultKeys, *startIndex);

        // retire any snap collector that we replaced in this RQ
        if (threadData[tid].snapCollectorToRetire) {
            threadData[tid].snapCollectorToRetire->retire(tid, recmgr);
            threadData[tid].snapCollectorToRetire = NULL;
        }
    }
};

#endif	/* RQ_RWLOCK_H */

