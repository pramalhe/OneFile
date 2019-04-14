/* 
 * File:   rq_rwlock.h
 * Author: trbot
 *
 * Created on April 20, 2017, 1:03 PM
 */

#ifndef RQ_RWLOCK_H
#define	RQ_RWLOCK_H

#define MAX_NODES_DELETED_ATOMICALLY 8
#define MAX_KEYS_PER_NODE 32

#include "rq_debugging.h"
#include <hashlist.h>
#include <rwlock.h>
#include <pthread.h>
#include <cassert>

// the following define enables an optimization that i'm not sure is correct.
//#define COLLECT_ANNOUNCEMENTS_FAST

template <typename K, typename V, typename NodeType, typename DataStructure, typename RecordManager, bool logicalDeletion, bool canRetireNodesLogicallyDeletedByOtherProcesses>
class RQProvider {
private:
    struct __rq_thread_data {
        #define __RQ_THREAD_DATA_SIZE 1024
        union {
            struct { // anonymous struct inside anonymous union means we don't need to type anything special to access these variables
                long long rq_lin_time;
                HashList<K> * hashlist;
                volatile char padding0[PREFETCH_SIZE_BYTES];
                void * announcements[MAX_NODES_DELETED_ATOMICALLY+1];
                int numAnnouncements;
            };
            char bytes[__RQ_THREAD_DATA_SIZE]; // avoid false sharing
        };
    } __attribute__((aligned(__RQ_THREAD_DATA_SIZE)));

    #define TIMESTAMP_NOT_SET 0
    #define HASHLIST_INIT_CAPACITY_POW2 (1<<8)

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
//        for (int tid=0;tid<NUM_PROCESSES;++tid) {
//            threadData[tid].hashlist->destroy();
//            delete threadData[tid].hashlist;
//        }
        delete[] threadData;
        DEBUG_DEINIT_RQPROVIDER(NUM_PROCESSES);
    }

    // invoke before a given thread can perform any rq_functions
    void initThread(const int tid) {
        if (init[tid]) return; else init[tid] = !init[tid];

        threadData[tid].hashlist = new HashList<K>();
        threadData[tid].hashlist->init(HASHLIST_INIT_CAPACITY_POW2);
        threadData[tid].numAnnouncements = 0;
        for (int i=0;i<MAX_NODES_DELETED_ATOMICALLY+1;++i) {
            threadData[tid].announcements[i] = NULL;
        }
        DEBUG_INIT_THREAD(tid);
    }

    // invoke once a given thread will no longer perform any rq_ functions
    void deinitThread(const int tid) {
        if (!init[tid]) return; else init[tid] = !init[tid];

        threadData[tid].hashlist->destroy();
        delete threadData[tid].hashlist;
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
#ifdef COLLECT_ANNOUNCEMENTS_FAST
            threadData[tid].announcements[threadData[tid].numAnnouncements] = NULL;
#endif
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
        
        rwlock.readLock();
        long long ts = timestamp;
        *lin_addr = lin_newval; // original linearization point
        rwlock.readUnlock();

        set_insertion_timestamps(tid, ts, insertedNodes, deletedNodes);
        set_deletion_timestamps(tid, ts, insertedNodes, deletedNodes);
        
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
        
        rwlock.readLock();
        long long ts = timestamp;
        T res = __sync_val_compare_and_swap(lin_addr, lin_oldval, lin_newval);
        rwlock.readUnlock();
        
        if (res == lin_oldval){
            set_insertion_timestamps(tid, ts, insertedNodes, deletedNodes);
            set_deletion_timestamps(tid, ts, insertedNodes, deletedNodes);
            
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
        threadData[tid].hashlist->clear();
        rwlock.writeLock();
        threadData[tid].rq_lin_time = ++timestamp; // linearization point of range query (at the write to timestamp)
        rwlock.writeUnlock();
    }

private:
    // invoke each time a traversal visits a node with a key in the desired range:
    // if the node belongs in the range query, it will be placed in rqResult[index]
    inline int __traversal_try_add(const int tid, NodeType * const node, NodeType ** const nodeSource, K * const outputKeys, V * const outputValues, const K& lo, const K& hi, bool foundDuringTraversal) {
        
        // rqResultKeys should have space for MAX_KEYS_PER_NODE keys, AT LEAST
        
        // in the following, rather than having deeply nested if-else blocks,
        // we return asap, and list facts that must be true if we didn't return
        assert(foundDuringTraversal || !logicalDeletion || ds->isLogicallyDeleted(tid, node));
        
        long long itime = TIMESTAMP_NOT_SET;
        while (itime == TIMESTAMP_NOT_SET) { itime = node->itime; }
        if (node->itime >= threadData[tid].rq_lin_time) return 0;               // node was inserted after the range query
        // fact: node was inserted before the range query
        
        bool logicallyDeleted = (logicalDeletion && ds->isLogicallyDeleted(tid, node));
        long long dtime = TIMESTAMP_NOT_SET;

        if (!logicalDeletion && foundDuringTraversal) goto tryAddToRQ;          // no logical deletion. since node was inserted before the range query, and the traversal encountered it, it must have been deleted AFTER the traversal encountered it.
        // fact: no logical deletion ==> did not find node during traversal

        dtime = node->dtime;
        if (dtime != TIMESTAMP_NOT_SET) {
            if (dtime < threadData[tid].rq_lin_time) return 0;                  // node was deleted before the range query
            goto tryAddToRQ;
        }

        // fact: dtime was not set above
        if (logicalDeletion && !logicallyDeleted) goto tryAddToRQ;              // if logical deletion is used with marking, the fact that node was inserted before the range query, and that the traversal encountered node, is NOT enough to argue that node was in the data structure when the traversal started. why? when the traversal encountered node, it might have already been marked. so, we check if node is marked. if not, then the node has not yet been deleted.
        // fact: if there is logical deletion, then the node has now been deleted

        ///////////////////////// HANDLE UNKNOWN DTIME /////////////////////////
        // if we are executing this because node was ANNOUNCED by a process,
        // as something that MIGHT soon be deleted (if nodeSource != NULL),
        // then node might not ever actually be deleted,
        // so we can't spin forever on dtime.
        if (nodeSource != NULL) {
            while (dtime == TIMESTAMP_NOT_SET && *nodeSource == node) { dtime = node->dtime; }
            if (dtime == TIMESTAMP_NOT_SET) {
                // above loop exited because the process removed its announcement to this node!
                // if the process deleted the node, then it removed the
                // announcement AFTER setting dtime.
                // so we reread dtime one more time, to figure out whether
                // the process actually deleted the node.
                SOFTWARE_BARRIER; // prevent read of dtime from happening before last read of *nodeSource
                dtime = node->dtime;
                if (dtime == TIMESTAMP_NOT_SET) {
                    // since dtime is not set, the process did NOT delete the node.
                    // so, either a DIFFERENT process deleted it,
                    // or it was found during the data structure traversal.
                    // if another process deleted it, then we will find it
                    // either in that process' announcements, or in a limbo bag.
                    return 0;
                }
                // the node has been deleted, and dtime is set, so we check dtime below.
            }
        } else {
            while (dtime == TIMESTAMP_NOT_SET) { dtime = node->dtime; }
        }
        if (dtime < threadData[tid].rq_lin_time) return 0;                      // node was deleted before the range query
        // fact: node was inserted before the rq and deleted after it
        
        ///////////////////// TRY TO ADD NODE'S KEYS TO RQ /////////////////////
        // note: this way of organizing this decision tree favors trees with fat multi-key nodes, because getKeys is delayed as long as possible.
        
tryAddToRQ:
        // fetch the node's keys that are in the set
        int cnt = ds->getKeys(tid, node, outputKeys, outputValues);
        assert(cnt < RQ_DEBUGGING_MAX_KEYS_PER_NODE);
        if (cnt == 0) return 0;                                                 // node doesn't contain any keys that are in the set
        // TODO: properly assert that getKeys doesn't run out of bounds on outputKeys[...] (i'm quite certain it doesn't, currently, though.)
        
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
    
    inline void traversal_try_add(const int tid, NodeType * const node, NodeType ** const nodeSource, K * const rqResultKeys, V * const rqResultValues, int * const startIndex, const K& lo, const K& hi, bool foundDuringTraversal) {
//#if defined MICROBENCH && !defined NDEBUG
//        assert(*startIndex < 2*RQSIZE); // note: this assert is a hack. it should be *startIndex < size of rqResultKeys
//        if (*startIndex >= RQSIZE) {
//            cout<<"ERROR: *startIndex="<<(*startIndex)<<" is unexpectedly greater than or equal to RQSIZE="<<RQSIZE<<" (lo="<<lo<<" hi="<<hi<<")"<<endl;
//            cout<<"results:";
//            for (int i=0;i<*startIndex;++i) {
//                cout<<" "<<rqResultKeys[i];
//            }
//            cout<<endl;
//            exit(-1);
//        }
//#endif
        int numNewKeys = __traversal_try_add(tid, node, nodeSource, rqResultKeys+(*startIndex), rqResultValues+(*startIndex), lo, hi, foundDuringTraversal);
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
        traversal_try_add(tid, node, NULL, rqResultKeys, rqResultValues, startIndex, lo, hi, true);
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
        
        // collect nodes announced by other processes
#ifdef COLLECT_ANNOUNCEMENTS_FAST
        int numCollected = 0;
        NodeType * collectedAnnouncement[NUM_PROCESSES*MAX_NODES_DELETED_ATOMICALLY];
        NodeType ** announcementSource[NUM_PROCESSES*MAX_NODES_DELETED_ATOMICALLY];
#endif
        for (int otherTid=0;otherTid<NUM_PROCESSES;++otherTid) if (otherTid != tid) {
            int sz = threadData[otherTid].numAnnouncements;
            SOFTWARE_BARRIER;
            for (int i=0;i<sz;++i) {
                NodeType * node = (NodeType *) threadData[otherTid].announcements[i];
                assert(node);
#ifdef COLLECT_ANNOUNCEMENTS_FAST
                collectedAnnouncement[numCollected] = node;
                announcementSource[numCollected] = (NodeType **) &threadData[otherTid].announcements[i];
                ++numCollected;
#else
                traversal_try_add(tid, node, (NodeType **) &threadData[otherTid].announcements[i], rqResultKeys, rqResultValues, startIndex, lo, hi, false);
#endif
            }
        }
        SOFTWARE_BARRIER;
        
        // TODO: add ability to bail out of waiting for dtime if node is no longer announced! (needed when COLLECT_ANNOUNCEMENTS_FAST is NOT defined)
        
        // collect epoch bags of other processes (MUST be after collecting announcements!)
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
        
#ifdef COLLECT_ANNOUNCEMENTS_FAST
        // try to add nodes collected from process announcements to the RQ
        for (int i=0;i<numCollected;++i) {
            traversal_try_add(tid, collectedAnnouncement[i], announcementSource[i], rqResultKeys, rqResultValues, startIndex, lo, hi, false);
        }
#endif
        
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

                traversal_try_add(tid, node, NULL, rqResultKeys, rqResultValues, startIndex, lo, hi, false);
            }
        }

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

#endif	/* RQ_RWLOCK_H */

