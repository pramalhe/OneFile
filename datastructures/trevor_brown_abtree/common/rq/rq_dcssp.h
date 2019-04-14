/* 
 * File:   rq_dcssp.h
 * Author: trbot
 *
 * Created on May 9, 2017, 4:30 PM
 */

#ifndef RQ_DCSSP_H
#define	RQ_DCSSP_H

#ifdef ADD_DELAY_BEFORE_DTIME
extern Random rngs[MAX_TID_POW2*PREFETCH_SIZE_WORDS];
#define GET_RAND(tid,n) (rngs[(tid)*PREFETCH_SIZE_WORDS].nextNatural((n)))
#define DELAY_UP_TO(n) { \
    unsigned __r = GET_RAND(tid,(n)); \
    for (int __i=0;__i<__r;++__i) { \
        SOFTWARE_BARRIER; \
    } \
}
#else
#define DELAY_UP_TO(n) 
#endif

#ifdef RQ_LOCKFREE_WAITS_FOR_DTIME
#define WAIT_FOR_DTIME(node) ({ while ((node)->dtime == TIMESTAMP_NOT_SET) ; true; })
#else
#define WAIT_FOR_DTIME(node) ({ false; })
#endif

#include <pthread.h>
#include <hashlist.h>
#include "rq_debugging.h"
#include "dcss_plus_impl.h"

template <typename T>
inline bool contains(T ** nullTerminatedArray, T * element) {
    for (int i=0;nullTerminatedArray[i];++i) {
        if (nullTerminatedArray[i] == element) return true;
    }
    return false;
}

template <typename T>
inline bool contains(T * array, const int numElements, T element) {
    for (int i=0;i<numElements;++i) {
        if (array[i] == element) return true;
    }
    return false;
}

#define SNAPSHOT_CONTAINS_INSERTED_NODE(snap, node) contains((void **) (snap).payload1, (void *) (node))
#define SNAPSHOT_CONTAINS_DELETED_NODE(snap, node) contains((void **) (snap).payload2, (void *) (node))

template <typename K, typename V, typename NodeType, typename DataStructure, typename RecordManager, bool logicalDeletion, bool canRetireNodesLogicallyDeletedByOtherProcesses>
class RQProvider {
private:
    struct __rq_thread_data {
        #define __RQ_THREAD_DATA_SIZE 1024
        #define MAX_NODES_DELETED_ATOMICALLY 8
        #define CODE_COVERAGE_MAX_PATHS 11
        union {
            struct { // anonymous struct inside anonymous union means we don't need to type anything special to access these variables
                long long rq_lin_time;
                HashList<K> * hashlist;
#ifdef COUNT_CODE_PATH_EXECUTIONS
                long long codePathExecutions[CODE_COVERAGE_MAX_PATHS];
#endif
                volatile char padding0[PREFETCH_SIZE_BYTES];
                void * announcements[MAX_NODES_DELETED_ATOMICALLY];
                int numAnnouncements;
            };
            char bytes[__RQ_THREAD_DATA_SIZE]; // avoid false sharing (note: anon struct above contains around 96 bytes)
        };
    } __attribute__((aligned(__RQ_THREAD_DATA_SIZE)));
    
#ifdef COUNT_CODE_PATH_EXECUTIONS
    #define COUNT_CODE_PATH(path) { assert((path) < CODE_COVERAGE_MAX_PATHS); (++threadData[tid].codePathExecutions[(path)]); }
    long long codePathExecutions[CODE_COVERAGE_MAX_PATHS];
#else
    #define COUNT_CODE_PATH(path) 
#endif

    #define TIMESTAMP_NOT_SET 0
    #define HASHLIST_INIT_CAPACITY_POW2 (1<<8)

    const int NUM_PROCESSES;
    volatile char padding0[PREFETCH_SIZE_BYTES];
    volatile long long timestamp = 1;
    volatile char padding1[PREFETCH_SIZE_BYTES];
    __rq_thread_data * threadData;

    #define NODE_DELETED_BEFORE_RQ 0
    #define NODE_DELETED_AFTER_RQ 1
    #define NODE_NOT_DELETED_BY_THREAD -1
    dcsspProvider<void *> * prov;
    
    DataStructure * ds;
    RecordManager * const recmgr;

    int init[MAX_TID_POW2] = {0,};

public:
    RQProvider(const int numProcesses, DataStructure * ds, RecordManager * recmgr) : NUM_PROCESSES(numProcesses), ds(ds), recmgr(recmgr) {
        prov = new dcsspProvider<void *>(numProcesses);
        threadData = new __rq_thread_data[numProcesses];
        DEBUG_INIT_RQPROVIDER(numProcesses);
#ifdef COUNT_CODE_PATH_EXECUTIONS
        for (int i=0;i<CODE_COVERAGE_MAX_PATHS;++i) {
            codePathExecutions[i] = 0;
        }
#endif
    }
    
    ~RQProvider() {
#ifdef COUNT_CODE_PATH_EXECUTIONS
        cout<<"code path executions:";
        for (int i=0;i<CODE_COVERAGE_MAX_PATHS;++i) {
            if (codePathExecutions[i]) {
                cout<<" "<<codePathExecutions[i];
            } else {
                cout<<" .";
            }
        }
        cout<<endl;
#endif
//        for (int tid=0;tid<NUM_PROCESSES;++tid) {
//            prov->deinitThread(tid);
//            threadData[tid].hashlist->destroy();
//            delete threadData[tid].hashlist;
//        }
        
        prov->debugPrint();
        delete prov;
        delete[] threadData;
        DEBUG_DEINIT_RQPROVIDER(NUM_PROCESSES);
    }
    
    // invoke before a given thread can invoke any functions on this object
    void initThread(const int tid) {
        if (init[tid]) return; else init[tid] = !init[tid];

        prov->initThread(tid);
        threadData[tid].hashlist = new HashList<K>();
        threadData[tid].hashlist->init(HASHLIST_INIT_CAPACITY_POW2);
        threadData[tid].numAnnouncements = 0;
        for (int i=0;i<MAX_NODES_DELETED_ATOMICALLY;++i) {
            threadData[tid].announcements[i] = NULL;
        }
#ifdef COUNT_CODE_PATH_EXECUTIONS
        for (int i=0;i<CODE_COVERAGE_MAX_PATHS;++i) {
            threadData[tid].codePathExecutions[i] = 0;
        }
#endif
        DEBUG_INIT_THREAD(tid);
    }

    // invoke once a given thread will no longer invoke any functions on this object
    void deinitThread(const int tid) {
        if (!init[tid]) return; else init[tid] = !init[tid];

        prov->deinitThread(tid);
        threadData[tid].hashlist->destroy();
        delete threadData[tid].hashlist;
#ifdef COUNT_CODE_PATH_EXECUTIONS
        for (int i=0;i<CODE_COVERAGE_MAX_PATHS;++i) {
            __sync_fetch_and_add(&codePathExecutions[i], threadData[tid].codePathExecutions[i]);
        }
#endif
        DEBUG_DEINIT_THREAD(tid);
    }

    // invoke whenever a new node is created/initialized
    inline void init_node(const int tid, NodeType * const node) {
        node->itime = TIMESTAMP_NOT_SET;
        node->dtime = TIMESTAMP_NOT_SET;
    }

    // for each address addr that is modified by rq_linearize_update_at_write
    // or rq_linearize_update_at_cas, you must replace any initialization of addr
    // with invocations of rq_write_addr
    //
    // NOTE: this CANNOT be used on fields that might be concurrently being modified
    // by an invocation of rq_linearize_update_at_write or
    // rq_linearize_update_at_cas
    template <typename T>
    inline void write_addr(const int tid, T volatile * const addr, const T val) {
        if (is_pointer<T>::value) {
            prov->writePtr((casword_t *) addr, (casword_t) val);
        } else {
            prov->writeVal((casword_t *) addr, (casword_t) val);
        }
    }

    // for each address addr that is modified by rq_linearize_update_at_write
    // or rq_linearize_update_at_cas, you must replace any reads of addr with
    // invocations of rq_read_addr
    template <typename T>
    inline T read_addr(const int tid, T volatile * const addr) {
        return (T) ((is_pointer<T>::value)
                ? prov->readPtr(tid, (casword_t *) addr)
                : prov->readVal(tid, (casword_t *) addr));
    }
    
    // IF DATA STRUCTURE PERFORMS LOGICAL DELETION
    // run some time BEFORE the physical deletion of a node
    // whose key has ALREADY been logically deleted.
    void announce_physical_deletion(const int tid, NodeType * const * const deletedNodes) {
        int i;
        for (i=0;deletedNodes[i];++i) {
            threadData[tid].announcements[threadData[tid].numAnnouncements+i] = deletedNodes[i];
        }
        SOFTWARE_BARRIER;
        threadData[tid].numAnnouncements += i;
        assert(threadData[tid].numAnnouncements <= MAX_NODES_DELETED_ATOMICALLY);
        SOFTWARE_BARRIER;
    }

    // IF DATA STRUCTURE PERFORMS LOGICAL DELETION
    // run AFTER performing announce_physical_deletion,
    // if the cas that was trying to physically delete node failed.
    void physical_deletion_failed(const int tid, NodeType * const * const deletedNodes) {
        for (int i=0;deletedNodes[i];++i) {
            --threadData[tid].numAnnouncements;
        }
        assert(threadData[tid].numAnnouncements >= 0);
    }
    
    // IF DATA STRUCTURE PERFORMS LOGICAL DELETION
    // run AFTER performing announce_physical_deletion,
    // if the cas that was trying to physically delete node succeeded.
    void physical_deletion_succeeded(const int tid, NodeType * const * const deletedNodes) {
        int i;
        for (i=0;deletedNodes[i];++i) {
            recmgr->retire(tid, deletedNodes[i]);
        }
        SOFTWARE_BARRIER; // ensure nodes are placed in the epoch bag BEFORE they are removed from announcements.
        threadData[tid].numAnnouncements -= i;
        assert(threadData[tid].numAnnouncements >= 0);
    }
    
private:
    
    inline void set_insertion_timestamps(
            const int tid,
            const long long ts,
            NodeType * const * const insertedNodes,
            NodeType * const * const deletedNodes) {
        
        // set insertion timestamps
        // for each i_node in insertedNodes
        for (int i_nodeix=0;insertedNodes[i_nodeix];++i_nodeix) {
            insertedNodes[i_nodeix]->itime = ts;
        }
    }

    inline void set_deletion_timestamps(
            const int tid,
            const long long ts,
            NodeType * const * const insertedNodes,
            NodeType * const * const deletedNodes) {
        
        // set deletion timestamps
        // for each d_node in deletedNodes
        for (int d_nodeix=0;deletedNodes[d_nodeix];++d_nodeix) {
            deletedNodes[d_nodeix]->dtime = ts;
        }
    }
    
public:
    
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
        
        casword_t old1;
        while (true) {
            old1 = (casword_t) timestamp;

            casword_t old2 = (is_pointer<T>::value)
                    ? (casword_t) prov->readPtr(tid, (casword_t *) lin_addr)
                    : (casword_t) prov->readVal(tid, (casword_t *) lin_addr);
            casword_t new2 = (casword_t) lin_newval;
            dcsspresult_t result = (is_pointer<T>::value)
                    ? prov->dcsspPtr(tid, (casword_t *) &timestamp, old1, (casword_t *) lin_addr, old2, new2, (void **) insertedNodes, (void **) deletedNodes)
                    : prov->dcsspVal(tid, (casword_t *) &timestamp, old1, (casword_t *) lin_addr, old2, new2, (void **) insertedNodes, (void **) deletedNodes);
            if (result.status == DCSSP_SUCCESS) {
                break;
            }
        }
        //DELAY_UP_TO(10000);

        set_insertion_timestamps(tid, old1 /* timestamp */, insertedNodes, deletedNodes);
        set_deletion_timestamps(tid, old1 /* timestamp */, insertedNodes, deletedNodes);
        
        // discard the payloads (insertedNodes and deletedNodes) in this thread's descriptor
        // so other threads can't access them far in the future if we become QUIESCENT and sleep for a long time
        // (must be performed after setting itimes and dtimes, but before enterQuiescentState)
        prov->discardPayloads(tid);

        if (!logicalDeletion) {
            // physical deletion will happen at the same time as logical deletion
            physical_deletion_succeeded(tid, deletedNodes);
        }
        
#if defined USE_RQ_DEBUGGING
        DEBUG_RECORD_UPDATE_CHECKSUM<K,V>(tid, old1 /* timestamp */, insertedNodes, deletedNodes, ds);
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
        
        casword_t old2 = (casword_t) lin_oldval;
        casword_t new2 = (casword_t) lin_newval;
        dcsspresult_t result;
        while (true) {
            casword_t old1 = (casword_t) timestamp;

            result = (is_pointer<T>::value)
                    ? prov->dcsspPtr(tid, (casword_t *) &timestamp, old1 /* timestamp */, (casword_t *) lin_addr, old2, new2, (void **) insertedNodes, (void **) deletedNodes)
                    : prov->dcsspVal(tid, (casword_t *) &timestamp, old1 /* timestamp */, (casword_t *) lin_addr, old2, new2, (void **) insertedNodes, (void **) deletedNodes);
            if (result.status == DCSSP_SUCCESS) {
                //DELAY_UP_TO(1000);

                set_insertion_timestamps(tid, old1 /* timestamp */, insertedNodes, deletedNodes);
                set_deletion_timestamps(tid, old1 /* timestamp */, insertedNodes, deletedNodes);

                // discard the payloads (insertedNodes and deletedNodes) in this thread's descriptor
                // so other threads can't access them far in the future if we become QUIESCENT and sleep for a long time
                // (must be performed after setting itimes and dtimes, but before enterQuiescentState)
                prov->discardPayloads(tid);
                
                if (!logicalDeletion) {
                    // physical deletion will happen at the same time as logical deletion
                    physical_deletion_succeeded(tid, deletedNodes);
                }             
                
#if defined USE_RQ_DEBUGGING
                DEBUG_RECORD_UPDATE_CHECKSUM<K,V>(tid, old1 /* timestamp */, insertedNodes, deletedNodes, ds);
#endif
                return lin_oldval;
            } else if (result.status == DCSSP_FAILED_ADDR2) {
                // failed due to original CAS's failure (NOT due to the timestamp changing)

                if (!logicalDeletion) {
                    // physical deletion will happen at the same time as logical deletion
                    physical_deletion_failed(tid, deletedNodes);
                }
                
                break;
            }
        }
        assert(result.status == DCSSP_FAILED_ADDR2);
        assert(old2 != result.failed_val);
        return (T) result.failed_val;
    }

    // invoke at the start of each traversal
    inline void traversal_start(const int tid) {
        threadData[tid].hashlist->clear();
        threadData[tid].rq_lin_time = __sync_add_and_fetch(&timestamp, 1);      // linearize rq here!
    }

private:
    // invoke each time a traversal visits a node with a key in the desired range:
    // if the node belongs in the range query, it will be placed in rqResult[index]
    inline int __traversal_try_add(const int tid, NodeType * const node, K * const outputKeys, V * const outputValues, const K& lo, const K& hi, bool foundDuringTraversal) {

        // rqResultKeys should have space for MAX_KEYS_PER_NODE keys, AT LEAST
        
        // in the following, rather than having deeply nested if-else blocks,
        // we return asap, and list facts that must be true if we didn't return
        assert(foundDuringTraversal || !logicalDeletion || ds->isLogicallyDeleted(tid, node));

        // TODO: ensure this makes sense when called with announced nodes
        
        long long itime = node->itime;
        if (itime != TIMESTAMP_NOT_SET & node->itime >= threadData[tid].rq_lin_time) return 0; // node was inserted after the range query
        // fact: either itime was not set above, or node was inserted before rq
        
        ///////////////////////// HANDLE UNKNOWN ITIME /////////////////////////

        // TODO: try adding a bit of spinning before falling back to the full lock-free solution
        
        // determine if any other process inserted, or is trying to insert node, and, if so, when
        for (int otherTid=0; (itime = node->itime) == TIMESTAMP_NOT_SET && otherTid<NUM_PROCESSES; ++otherTid) if (otherTid != tid) {
            tagptr_t tagptr = prov->getDescriptorTagptr(otherTid);              // try to get a snapshot of otherTid's dcssp descriptor
            dcsspdesc_t<void *> snap;
            if (!prov->getDescriptorSnapshot(tagptr, &snap)) {                  // we failed to obtain a snapshot, which means that while getDescriptorSnapshot() was running, the process finished one dcssp, and started a new dcssp.
                continue; // goto check next process                            // if the finished dcssp inserted node, then before the next dcssp by the same process, node->itime is set. so, we check whether itime is set.
            }
            // fact: we obtained a snapshot

            if (!SNAPSHOT_CONTAINS_INSERTED_NODE(snap, node)) continue; // goto check next process
            // fact: otherTid is trying/tried to insert node

            int state = MUTABLES_UNPACK_FIELD(snap.mutables, DCSSP_MUTABLES_MASK_STATE, DCSSP_MUTABLES_OFFSET_STATE);
            if (state == DCSSP_STATE_FAILED) {                                  // the operation described by snap did not insert node, so either this process did not insert it, or the process inserted/inserted it in a PREVIOUS operation, so it must have already set itime as appropriate
                continue; // goto check next process
            } else if (state == DCSSP_STATE_SUCCEEDED) {                        // the dcssp operation finished, and inserted node. to determine WHEN it inserted node, we look at the argument old1 to the dcssp, which contains the timestamp when the dcssp took place. (observe that this is the value process otherTid would write to node->itime)
                if (snap.old1 >= threadData[tid].rq_lin_time) return 0;         // node was inserted after rq
                break; // process inserted node at time snap.old1, BEFORE the RQ
            }
            // fact: state is UNDECIDED
            
            // now we try to help
            casword_t addr2 = *snap.addr2;
            if (addr2 == tagptr) {                                              // addr2 indeed points to the dcssp descriptor. the linearization point of the dcssp operation occurs after this step, so the dcssp might have been linearized, but not yet had its state set.
                prov->helpProcess(tid, otherTid);                               // we need to know what its final state will be to determine whether it successfully inserted node. so, we HELP otherTid finish its dcssp.
            }
            // note: the following all happens in BOTH the cases where addr2 == tagptr and where addr2 != tagptr, except there is some extra work if addr2 != tagptr and state2 != SUCCEEDED. i've folded the two cases together simply for compactness / less repetition.

            // then, we reread the state
            bool valid = false;
            dcsspdesc_t<void *> * ptr = prov->getDescriptorPtr(tagptr);
            int state2 = DESC_READ_FIELD(valid, ptr->mutables, tagptr, DCSSP_MUTABLES_MASK_STATE, DCSSP_MUTABLES_OFFSET_STATE);
            if (!valid) continue; // goto check next process                    // the read of the state field was invalid, which means that the dcssp operation has terminated, the next dcssp operation by otherTid has begun. since the next dcssp operation has begun, and each high-level data structure operation performs only one successful dcssp (in a call to linearize_update_at_...), if this dcssp that finished in fact inserted node, then the next dcssp would be part of the next high-level operation. thus, if node->itime was inserted by the finished dcssp, then the high-level operation that performed this dcssp will already have set node->itime.
            // fact: the read of state was valid
            
            if (state2 == DCSSP_STATE_SUCCEEDED) {                              // we are in case (b) described above. the dcssp operation finished, and insert node. to determine WHEN it inserted node, we look at the argument old1 to the dcssp, which contains the timestamp when the dcssp took place. (observe that this is the value process otherTid would write to node->itime)
                if (snap.old1 >= threadData[tid].rq_lin_time) return 0;         // node was inserted after rq
                break; // process inserted node at time snap.old1, BEFORE the RQ
            } else { // undecided or failed
                continue; // goto check next process                            // we are in case (a) or case (c) described above. so, otherTid did NOT delete node.
            }
        }
        if (itime != TIMESTAMP_NOT_SET && itime >= threadData[tid].rq_lin_time) return 0; // node was inserted after rq

        /////////////// HANDLE LOGICAL DELETION AND CHECK DTIME ////////////////
        
        long long dtime = TIMESTAMP_NOT_SET;

        if (!logicalDeletion && foundDuringTraversal) goto tryAddToRQ;          // no logical deletion. since node was inserted before the range query, and the traversal encountered it, it must have been deleted AFTER the traversal encountered it.
        // fact: no logical deletion ==> did not find node during traversal

        dtime = node->dtime;
        if (dtime != TIMESTAMP_NOT_SET && dtime < threadData[tid].rq_lin_time) return 0;
        // fact: either dtime was not set above, or node was deleted after rq

        if (logicalDeletion && !ds->isLogicallyDeleted(tid, node)) goto tryAddToRQ; // if logical deletion is used with marking, the fact that node was inserted before the range query, and that the traversal encountered node, is NOT enough to argue that node was in the data structure when the traversal started. why? when the traversal encountered node, it might have already been marked. so, we check if node is marked. if not, then the node has not yet been deleted.
        // fact: logical deletion ==> node has been logically deleted
        
        ///////////////////////// HANDLE UNKNOWN DTIME /////////////////////////
        
        // determine if any other process is trying/tried to delete node
        for (int otherTid=0; (dtime = node->dtime) == TIMESTAMP_NOT_SET && otherTid<NUM_PROCESSES; ++otherTid) if (otherTid != tid) {
            tagptr_t tagptr = prov->getDescriptorTagptr(otherTid);              // try to get a snapshot of otherTid's dcssp descriptor
            dcsspdesc_t<void *> snap;
            if (!prov->getDescriptorSnapshot(tagptr, &snap)) {                  // we failed to obtain a snapshot, which means that while getDescriptorSnapshot() was running, the process finished one dcssp, and started a new dcssp.
                continue; // goto check next process                            // if the finished dcssp deleted node, then before the next dcssp by the same process, node->dtime is set (and it will be seen after the loop).
            }
            // fact: we obtained a snapshot

            if (!SNAPSHOT_CONTAINS_DELETED_NODE(snap, node)) continue; // goto check next process
            // fact: otherTid is trying/tried to delete node

            // we must determine whether otherTid's dcssp operation (whose descriptor we obtained a snapshot of) has been linearized, and whether it was successful.
            // we use the following facts.
            // (1) the dcssp descriptor has a state that is initially UNDECIDED, and becomes SUCCEEDED or FAILED after the dcssp has been linearized.
            // (2) a dcssp that succeeds or fails changes *snap.addr2 to tagptr, then reads *snap.addr1 and linearizes, then sets its state to SUCCEEDED or FAILED, then changes *snap.addr2 from tagptr to another value.
            // (3) once *snap.addr2 has been changed from tagptr to another value, it can never again contain tagptr.
            // (4) each high-level data structure operation invokes dcssp only via linearize_update_at_write or linearize_update_at_cas, and only performs one invocation of linearize_update_at_write or one /successful/ invocation of linearize_update_at_cas (and possibly many unsuccessful invocations of linearize_update_at_cas).
            // (5) if a dcssp operation deletes node, then before the next dcssp by the same process, node->dtime is set.
            // so, we check the state of the dcssp operation. if it is SUCCEEDED or FAILED, we have our answer. but, if the state is UNDECIDED, the dcssp may or may not have been linearized.
            // in the latter case, to determine whether it has been linearized, we would like to HELP the dcssp operation to complete.
            // note, however, that the help procedure for the dcssp algorithm can be invoked only if otherTid has already changed *snap.addr2 to tagptr.
            // thus, we must determine whether otherTid has changed *snap.addr2 to tagptr, before we can help the dcssp operation.
            // so, we read *snap.addr2. if we see that it contains tagptr, then we can help the dcssp.
            // otherwise, one of the following must be true:
            // (a) otherTid has not yet changed *snap.addr2 to tagptr, or
            // (b) otherTid changed *snap.addr2 to tagptr, then it (or a helper) changed its state to SUCCEEDED, then changed *snap.addr2 to a different value (never again to contain tagptr), or
            // (c) otherTid changed *snap.addr2 to tagptr, then it (or a helper) changed its state to FAILED, then changed *snap.addr2 to a different value (never again to contain tagptr).
            // in case (a), we know that the dcssp has not yet been linearized.
            // in case (b), the dcssp has been linearized, has state SUCCEEDED, and deleted node.
            // in case (c), the dcssp has been linearized, has state FAILED, and did NOT delete node.
            // so, after reading *snap.addr2, we read the dcssp operation's state again to determine which case has occurred.

            int state = MUTABLES_UNPACK_FIELD(snap.mutables, DCSSP_MUTABLES_MASK_STATE, DCSSP_MUTABLES_OFFSET_STATE);
            if (state == DCSSP_STATE_FAILED) {                                  // the operation described by snap did not insert/delete node, so either this process did not insert/delete it, or the process inserted/deleted it in a PREVIOUS operation, so it must have already set itime/dtime as appropriate
                continue; // goto check next process
            } else if (state == DCSSP_STATE_SUCCEEDED) {                        // the dcssp operation finished, and deleted node. to determine WHEN it deleted node, we look at the argument old1 to the dcssp, which contains the timestamp when the dcssp took place. (observe that this is the value process otherTid would write to node->dtime)
                if (WAIT_FOR_DTIME(node)) {
                    // the following assertions are thread safe ONLY if WAIT_FOR_DTIME actually waits! (which is true only if it returns true, which is true only if RQ_LOCKFREE_WAITS_FOR_DTIME is defined)
                    assert(snap.old1 <= node->dtime);
                    assert((snap.old1 >= threadData[tid].rq_lin_time) == (node->dtime >= threadData[tid].rq_lin_time));
                    assert(foundDuringTraversal || node->dtime == snap.old1);
                    assert(!foundDuringTraversal || node->dtime == snap.old1);
                }
                if (snap.old1 < threadData[tid].rq_lin_time) return 0;          // node was deleted before rq
                goto tryAddToRQ; // node was deleted by this process after rq
            }
            // fact: state is UNDECIDED
            
            // maya: in logicalDeletion, since the node is marked the DCSS was successful and only the dtime is not yet set. Thus, UNDECIDED means that otherThread did not mark the node, it was some other thread and we can continue. 
            if (logicalDeletion) continue; // goto check next process

            // now we try to help
            casword_t addr2 = *snap.addr2;
            if (addr2 == tagptr) {                                              // TODO: prove it is impossible to execute this block with logical deletion (idea: since node is marked, either (1) otherTid marked it earlier with a DCSS whose state is SUCCEEDED, or (2) someone else marked node, so otherTid cannot successfully CAS addr2.)
                // addr2 indeed points to the dcssp descriptor. the linearization point of the dcssp operation occurs after this step, so the dcssp might have been linearized, but not yet had its state set.
                prov->helpProcess(tid, otherTid);                       // we need to know what its final state will be to determine whether it successfully deleted node. so, we HELP otherTid finish its dcssp.
            }
            // note: the following all happens in BOTH the cases where addr2 == tagptr and where addr2 != tagptr, except there is some extra work if addr2 != tagptr and state2 != SUCCEEDED. i've folded the two cases together simply for compactness / less repetition.

            // then, we reread the state
            bool valid = false;
            dcsspdesc_t<void *> * ptr = prov->getDescriptorPtr(tagptr);
            int state2 = DESC_READ_FIELD(valid, ptr->mutables, tagptr, DCSSP_MUTABLES_MASK_STATE, DCSSP_MUTABLES_OFFSET_STATE);
            if (!valid) continue; // goto check next process                    // the read of the state field was invalid, which means that the dcssp operation has terminated, the next dcssp operation by otherTid has begun. since the next dcssp operation has begun, and each high-level data structure operation performs only one successful dcssp (in a call to linearize_update_at_...), if this dcssp that finished in fact deleted node, then the next dcssp would be part of the next high-level operation. thus, if node->itime was inserted by the finished dcssp, then the high-level operation that performed this dcssp will already have set node->itime.
            // fact: the read of state was valid

            if (state2 == DCSSP_STATE_SUCCEEDED) {                              // we are in case (b) described above. the dcssp operation finished, and deleted node. to determine WHEN it deleted node, we look at the argument old1 to the dcssp, which contains the timestamp when the dcssp took place. (observe that this is the value process otherTid would write to node->dtime)
                if (snap.old1 >= threadData[tid].rq_lin_time) goto tryAddToRQ;
                return 0; // do not add to rq
            } else { // undecided or failed
                continue; // goto check next process                            // we are in case (a) or case (c) described above. so, otherTid did NOT delete node.
            }
        }
        if(dtime == TIMESTAMP_NOT_SET) {
            assert(!logicalDeletion); 
            assert(!foundDuringTraversal);
            goto tryAddToRQ; // no process deleted node before the range query
        }
        COUNT_CODE_PATH(9);
        if (dtime >= threadData[tid].rq_lin_time) goto tryAddToRQ;
        return 0; // do not add to rq

        ///////////////////// TRY TO ADD NODE'S KEYS TO RQ /////////////////////
        // note: this way of organizing this decision tree favors trees with fat multi-key nodes, because getKeys is delayed as long as possible.
        
tryAddToRQ:
        // fetch the node's keys that are in the set
        int cnt = ds->getKeys(tid, node, outputKeys, outputValues);
        assert(cnt < RQ_DEBUGGING_MAX_KEYS_PER_NODE);
        if (cnt == 0) return 0;                                                 // node doesn't contain any keys that are in the set and in the desired range
        
        // note: in the following loop, we shift keys in the outputKeys array left to eliminate any that ultimately should not be added to the range query
        int numNewKeys = 0;
        for (int i=0;i<cnt;++i) {                                               // decide whether key = outputKeys[i] should be in the range query
            if (!ds->isInRange(outputKeys[i], lo, hi)) goto doNotAddToRQ;       // key is NOT in the desired range
            if (threadData[tid].hashlist->contains(outputKeys[i])) goto doNotAddToRQ; // key is already in the range query
            outputKeys[numNewKeys] = outputKeys[i];                             // save this as a new key added to the RQ
            outputValues[numNewKeys] = outputValues[i];
            ++numNewKeys;

doNotAddToRQ: (0);
        }
        return numNewKeys;
    }

    inline void traversal_try_add(const int tid, NodeType * const node, K * const rqResultKeys, V * const rqResultValues, int * const startIndex, const K& lo, const K& hi, bool foundDuringTraversal) {
//#if defined MICROBENCH && !defined NDEBUG
//        assert(*startIndex < RQSIZE); // note: this assert is a hack. it should be *startIndex < size of rqResultKeys
//        if (*startIndex > RQSIZE) {
//            cout<<"ERROR: *startIndex="<<(*startIndex)<<" is unexpectedly greater than or equal to RQSIZE="<<RQSIZE<<" (lo="<<lo<<" hi="<<hi<<")"<<endl;
//            cout<<"results:";
//            for (int i=0;i<*startIndex;++i) {
//                cout<<" "<<rqResultKeys[i];
//            }
//            cout<<endl;
//            exit(-1);
//        }
//#endif
        int numNewKeys = __traversal_try_add(tid, node, rqResultKeys+(*startIndex), rqResultValues+(*startIndex), lo, hi, foundDuringTraversal);
//#if defined MICROBENCH
//        assert(*startIndex + numNewKeys < 2*RQSIZE); // note: this assert is a hack. it should be *startIndex + numNewKeys < size of rqResultKeys array
//#endif
        for (int i=0;i<numNewKeys;++i) {
            threadData[tid].hashlist->insert(rqResultKeys[(*startIndex)++]);
        }
        // note: the above increments startIndex
#if defined MICROBENCH
        assert(*startIndex <= RQSIZE);
#endif
    }
    
public:
    inline void traversal_try_add(const int tid, NodeType * const node, K * const rqResultKeys, V * const rqResultValues, int * const startIndex, const K& lo, const K& hi) {
        traversal_try_add(tid, node, rqResultKeys, rqResultValues, startIndex, lo, hi, true);
    }
    
    // invoke at the end of each traversal:
    // any nodes that were deleted during the traversal,
    // and were consequently missed during the traversal,
    // are placed in rqResult[index]
    void traversal_end(const int tid, K * const rqResultKeys, V * const rqResultValues, int * const startIndex, const K& lo, const K& hi) {
        // todo: possibly optimize by skipping entire blocks if there are many keys to skip (does not seem to be justifiable for 4 work threads and 4 range query threads)

        SOFTWARE_BARRIER;
        long long end_timestamp = timestamp;
        SOFTWARE_BARRIER;
        
#if 0
        vector<NodeType *> nodes;
        
        // collect nodes announced by other processes
        for (int otherTid=0;otherTid<NUM_PROCESSES;++otherTid) if (otherTid != tid) {
            int sz = threadData[otherTid].numAnnouncements;
            SOFTWARE_BARRIER;
            for (int i=0;i<sz;++i) {
                NodeType * node = (NodeType *) threadData[otherTid].announcements[i];
                assert(node);
                nodes.push_back(node);
//                traversal_try_add(tid, node, rqResultKeys, startIndex, lo, hi, false);
            }
        }
        SOFTWARE_BARRIER;
        
        // collect epoch bags of other processes (MUST be after checking announcements!)
        blockbag<NodeType> * all_bags[NUM_PROCESSES*NUMBER_OF_EPOCH_BAGS+1];
        vector<blockbag_iterator<NodeType>> all_iterators;
        int numIterators = 0;
        for (int otherTid=0;otherTid<NUM_PROCESSES;++otherTid) if (otherTid != tid) {
            blockbag<NodeType> * thread_bags[NUMBER_OF_EPOCH_BAGS+1];
            recmgr->get((NodeType *) NULL)->reclaim->getSafeBlockbags(otherTid, thread_bags);
            for (int i=0;thread_bags[i];++i) {
                all_bags[numIterators] = thread_bags[i];
                all_iterators.push_back(thread_bags[i]->begin());
                ++numIterators;
            }
        }

        // collect nodes in epoch bags
        int numVisitedInEpochBags = 0;
        for (int ix = 0; ix < numIterators; ++ix) {
            for (; all_iterators[ix] != all_bags[ix]->end(); all_iterators[ix]++) {
                NodeType * node = (*all_iterators[ix]);
                nodes.push_back(node);
                
                ++numVisitedInEpochBags;
                
                long long dtime = node->dtime;
                if (dtime != TIMESTAMP_NOT_SET && dtime > end_timestamp) continue;
                
                if (!(logicalDeletion && canRetireNodesLogicallyDeletedByOtherProcesses)) {
                    // if we cannot retire nodes that are logically deleted
                    // by other processes, then we always retire nodes in
                    // order of increasing dtime values.
                    // so, the blockbag will be ordered, which means that,
                    // if dtime is before the RQ, then all remaining nodes
                    // in this bag were deleted before the RQ.
                    // so, in this case, we skip to the next bag.
                    if (dtime != TIMESTAMP_NOT_SET && dtime < threadData[tid].rq_lin_time) break;
                }
            }
        }
        
        // visit collected nodes
        for (auto it = nodes.begin(); it != nodes.end(); it++) {
            NodeType * node = *it;
            traversal_try_add(tid, node, rqResultKeys, startIndex, lo, hi, false);
        }
#else
        // collect nodes announced by other processes
        for (int otherTid=0;otherTid<NUM_PROCESSES;++otherTid) if (otherTid != tid) {
            int sz = threadData[otherTid].numAnnouncements;
            SOFTWARE_BARRIER;
            for (int i=0;i<sz;++i) {
                NodeType * node = (NodeType *) threadData[otherTid].announcements[i];
                assert(node);
                traversal_try_add(tid, node, rqResultKeys, rqResultValues, startIndex, lo, hi, false);
            }
        }
        SOFTWARE_BARRIER;
        
        // collect epoch bags of other processes (MUST be after checking announcements!)
        blockbag<NodeType> * all_bags[NUM_PROCESSES*NUMBER_OF_EPOCH_BAGS+1];
        vector<blockbag_iterator<NodeType>> all_iterators;
        int numIterators = 0;
        for (int otherTid=0;otherTid<NUM_PROCESSES;++otherTid) if (otherTid != tid) {
            blockbag<NodeType> * thread_bags[NUMBER_OF_EPOCH_BAGS+1];
            recmgr->get((NodeType *) NULL)->reclaim->getSafeBlockbags(otherTid, thread_bags);
            for (int i=0;thread_bags[i];++i) {
                all_bags[numIterators] = thread_bags[i];
                all_iterators.push_back(thread_bags[i]->begin());
                ++numIterators;
            }
        }
        
        int numSkippedInEpochBags = 0;
        int numVisitedInEpochBags = 0;
        for (int ix = 0; ix < numIterators; ++ix) {
            for (; all_iterators[ix] != all_bags[ix]->end(); all_iterators[ix]++) {
                NodeType * node = (*all_iterators[ix]);
                assert(node);

                ++numVisitedInEpochBags;
                ++numSkippedInEpochBags;

                long long dtime = node->dtime;
                if (dtime != TIMESTAMP_NOT_SET && dtime > end_timestamp) continue;
                
                --numSkippedInEpochBags;

                if (!(logicalDeletion && canRetireNodesLogicallyDeletedByOtherProcesses)) {
                    // if we cannot retire nodes that are logically deleted
                    // by other processes, then we always retire nodes in
                    // order of increasing dtime values.
                    // so, the blockbag will be ordered, which means that,
                    // if dtime is before the RQ, then all remaining nodes
                    // in this bag were deleted before the RQ.
                    // so, in this case, we skip to the next bag.
                    if (dtime != TIMESTAMP_NOT_SET && dtime < threadData[tid].rq_lin_time) break;
                }

                traversal_try_add(tid, node, rqResultKeys, rqResultValues, startIndex, lo, hi, false);
            }
        }
        
#endif
        
#if defined MICROBENCH && !defined NDEBUG
        if (*startIndex > RQSIZE) {
            cout<<"ERROR: *startIndex="<<(*startIndex)<<" is unexpectedly greater than or equal to RQSIZE="<<RQSIZE<<" (lo="<<lo<<" hi="<<hi<<")"<<endl;
            cout<<"results:";
            for (int i=0;i<*startIndex;++i) {
                cout<<" "<<rqResultKeys[i];
            }
            cout<<endl;
            exit(-1);
        }
#endif

#ifdef __HANDLE_STATS

        GSTATS_ADD_IX(tid, skipped_in_bags, numSkippedInEpochBags, threadData[tid].rq_lin_time);
        GSTATS_ADD_IX(tid, visited_in_bags, numVisitedInEpochBags, threadData[tid].rq_lin_time);
#endif
        DEBUG_RECORD_RQ_VISITED(tid, threadData[tid].rq_lin_time, numVisitedInEpochBags);
        DEBUG_RECORD_RQ_SIZE(*startIndex);
        DEBUG_RECORD_RQ_CHECKSUM(tid, threadData[tid].rq_lin_time, rqResultKeys, *startIndex);
    }
};

#endif	/* RQ_DCSSP_H */

