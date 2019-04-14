/* 
 * File:   snapcollector.h
 * Author: trbot
 *
 * Created on June 21, 2017, 4:57 PM
 */

#ifndef SNAPCOLLECTOR_H
#define SNAPCOLLECTOR_H

#include <limits>
#include <vector>
#include <algorithm>
#include "reportitem.h"
#include <plaf.h>

template <typename NodeType, typename K>
class SnapCollector {
public:
    int NUM_THREADS;
    
    class NodeWrapper {
    public:
        NodeType * node;
        NodeWrapper * volatile next;
        K key;
        
        NodeWrapper() {}
        void init(K key) {
            this->node = NULL;
            this->next = NULL;
            this->key = key;
        }
        void init(NodeType * node, K key) {
            this->node = node;
            this->next = NULL;
            this->key = key;
        }
    };
    
private:
    ReportItem * volatile * reportHeads;
    ReportItem * volatile * reportTails;
    
    NodeWrapper * volatile head;
    NodeWrapper * volatile tail;
    ReportItem * blocker;
    volatile bool active;
    
    // variables used for aggregating reports after they are collected
    void ** currLocations;
    int * currRepLocations;
    std::vector<CompactReportItem *> * volatile gAllReports;
    
    K KEY_MAX;
    K KEY_MIN;

private:
    
    inline bool isBlocker(NodeWrapper const * const wrapper) {
        if (wrapper) {
            K key = wrapper->key;
            NodeType * node = wrapper->node;
            K key2 = KEY_MAX;
            return (key == key2 && node == NULL);
        }
        return false;
    }
    
    template <typename RecordManager>
    inline void __retireAllReports(const int tid, std::vector<CompactReportItem *> * v, RecordManager * recmgr) {
        if (v == NULL) return;
        for (auto it = v->begin(); it != v->end(); it++) {
            // retire compact report items
            recmgr->retire(tid, *it);
        }
    }
    
    template <typename RecordManager>
    inline void __deallocateAllReports(const int tid, std::vector<CompactReportItem *> * v, RecordManager * recmgr) {
        if (v == NULL) return;
        for (auto it = v->begin(); it != v->end(); it++) {
            // deallocate compact report items
            recmgr->deallocate(tid, *it);
        }
        delete v;
    }
    
public:
    
    template <typename RecordManager>
    void init(const int tid, const int numProcesses, RecordManager * const recmgr, const K _KEY_MIN, const K _KEY_MAX) {
        this->KEY_MIN = _KEY_MIN;
        this->KEY_MAX = _KEY_MAX;
        
        this->NUM_THREADS = numProcesses;
        this->reportHeads = new ReportItem * volatile [NUM_THREADS*PREFETCH_SIZE_WORDS];
        this->reportTails = new ReportItem * volatile [NUM_THREADS*PREFETCH_SIZE_WORDS];
        // head = new NodeWrapper(std::numeric_limits<int>::min())
        this->head = recmgr->template allocate<NodeWrapper>(tid);
#ifdef __HANDLE_STATS
        GSTATS_APPEND(tid, extra_type2_allocated_addresses, ((long long) head)%(1<<12));
#endif
        this->head->init(this->KEY_MIN);
        this->tail = this->head;
//        oldTail = NULL;
        // blocker = new ReportItem(NULL, ReportType::Add, -1)
        this->blocker = recmgr->template allocate<ReportItem>(tid);
#ifdef __HANDLE_STATS
        GSTATS_APPEND(tid, extra_type3_allocated_addresses, ((long long) blocker)%(1<<12));
#endif
        this->blocker->init(NULL, ReportType::Add, -1);
        this->active = true;
        this->currLocations = new void * [NUM_THREADS*PREFETCH_SIZE_WORDS];
        this->currRepLocations = new int[NUM_THREADS*PREFETCH_SIZE_WORDS];
        this->gAllReports = NULL;
        for (int i=0;i<NUM_THREADS;++i) {
            this->reportHeads[i*PREFETCH_SIZE_WORDS] = recmgr->template allocate<ReportItem>(tid);
#ifdef __HANDLE_STATS
            GSTATS_APPEND(tid, extra_type3_allocated_addresses, ((long long) reportHeads[i*PREFETCH_SIZE_WORDS])%(1<<12));
#endif
            this->reportHeads[i*PREFETCH_SIZE_WORDS]->init(NULL, ReportType::Add, -1); // sentinel head.
            this->reportTails[i*PREFETCH_SIZE_WORDS] = this->reportHeads[i*PREFETCH_SIZE_WORDS];
            
            this->currLocations[i*PREFETCH_SIZE_WORDS] = NULL;
            this->currRepLocations[i*PREFETCH_SIZE_WORDS] = 0;
        }
    }
    
    ~SnapCollector() {
        if (reportHeads) delete[] reportHeads;
        if (reportTails) delete[] reportTails;
        if (currLocations) delete[] currLocations;
        if (currRepLocations) delete[] currRepLocations;
        if (gAllReports) delete gAllReports;
    }
    
    template <typename RecordManager>
    void retire(const int tid, RecordManager * recmgr) {
        // retire report items
        for (int i=0;i<NUM_THREADS;++i) {
            ReportItem * curr = reportHeads[i*PREFETCH_SIZE_WORDS];
            while (curr != NULL) {
                if (curr != blocker) recmgr->retire(tid, curr); // blocker can exist in many per-thread lists, but we only want to retire it once, below.
                curr = curr->next;
            }
        }
        // retire blocker
        recmgr->retire(tid, blocker);
        // if a thread has changed tail to point to a "blocker," then
        // threads may have appended node wrappers to the blocker,
        // so we have to retire any such node wrappers
        NodeWrapper * curr = this->tail;
        if (isBlocker(curr)) {
            while (curr != NULL) {
                recmgr->retire(tid, curr);
                curr = curr->next;
            }
        }
        // retire node wrappers
        curr = head;
        while (curr != NULL) { // && curr != tail /*&& curr != oldTail*/) {
            recmgr->retire(tid, curr);
            curr = curr->next;
        }
        // retire the contents of gAllReports
        __retireAllReports(tid, gAllReports, recmgr);
        // retire snap collector
        recmgr->retire(tid, this);
    }
    
    // TIMNAT: Implemented according to the optimization in A.3:
    // TIMNAT: Only accept nodes whose key is higher than the last, and return the last node. 
    template <typename RecordManager>
    NodeType * AddNode(const int tid, NodeType * node, K key, RecordManager * recmgr) {
        NodeWrapper * last = tail;
        if (last->key >= key) // TIMNAT: trying to add an out of place node.
            return last->node;

        // advance tail pointer if needed
        if (last->next != NULL) {
            if (last == tail) __sync_bool_compare_and_swap(&tail, last, last->next);
            return tail->node;
        }
        
        NodeWrapper * newNode = recmgr->template allocate<NodeWrapper>(tid);
#ifdef __HANDLE_STATS
        GSTATS_APPEND(tid, extra_type2_allocated_addresses, ((long long) newNode)%(1<<12));
#endif
        newNode->init(node, key);
        if (__sync_bool_compare_and_swap(&last->next, NULL, newNode)) {
            __sync_bool_compare_and_swap(&tail, last, newNode);
            return node;
        } else {
            recmgr->deallocate(tid, newNode);
            return tail->node;
        }
    }
    
    template <typename RecordManager>
    void Report(int tid, NodeType * Node, ReportType t, K key, RecordManager * recmgr) {
        ReportItem * reportTail = reportTails[tid*PREFETCH_SIZE_WORDS];
        ReportItem * newItem = recmgr->template allocate<ReportItem>(tid);
#ifdef __HANDLE_STATS
        GSTATS_APPEND(tid, extra_type3_allocated_addresses, ((long long) newItem)%(1<<12));
#endif
        newItem->init(Node, t, key);
        if (__sync_bool_compare_and_swap(&reportTail->next, NULL, newItem)) {
            reportTails[tid*PREFETCH_SIZE_WORDS] = newItem;
        } else {
            recmgr->deallocate(tid, newItem);
        }
    }
    
    bool IsActive() {
//        __sync_synchronize();
//        SOFTWARE_BARRIER;
        bool result = active;
//        SOFTWARE_BARRIER;
        return result;
    }
    
    template <typename RecordManager>
    void BlockFurtherPointers(const int tid, RecordManager * recmgr) {
        NodeWrapper * blocker = recmgr->template allocate<NodeWrapper>(tid);
#ifdef __HANDLE_STATS
        GSTATS_APPEND(tid, extra_type2_allocated_addresses, ((long long) blocker)%(1<<12));
#endif
        blocker->init(NULL, KEY_MAX);
        
#if 1
        while (true) {
            NodeWrapper * old = this->tail;
            if (isBlocker(old)) { // old is a blocker, so no need to add our own blocker
                recmgr->deallocate(tid, blocker);
                return;
            }
            if (__sync_bool_compare_and_swap(&this->tail, old, blocker)) {
                return;
            }
        }
#else
        tail = blocker;
#endif
    }
    
    /**
     * note: the parameters are used for the timestamping mechanism of the
     *       test harness. they are NOT inherently needed by the snap collector.
     */
    void Deactivate(pthread_rwlock_t * const rwlock, volatile long long * timestamp, long long * rq_lin_time) {
#ifdef RQ_USE_TIMESTAMPS
        if (pthread_rwlock_wrlock(rwlock)) error("could not write-lock rwlock");
        active = false; // range query is linearized here
        *timestamp = *timestamp + 1;
        *rq_lin_time = *timestamp; //++(*timestamp);
        //std::cout<<"timestamp="<<*timestamp<<std::endl;
        if (pthread_rwlock_unlock(rwlock)) error("could not write-unlock rwlock");
#else
        active = false; // range query is linearized here (no memory barrier needed, since we don't care about read/write re-ordering--just when this hits main memory)
        //__sync_synchronize();
#endif
    }

    void BlockFurtherReports() {
        for (int i = 0; i<NUM_THREADS; i++) {
            ReportItem * reportTail = reportTails[i*PREFETCH_SIZE_WORDS];
            
            // TODO: SAVE OLD TAILS, HERE!!!
            
            if (reportTail->next == NULL)
                __sync_bool_compare_and_swap(&reportTail->next, NULL, blocker);
            // assert cas succeeded OR reportTail->next == blocker
        }
    }

private:
    
    // TIMNAT: What follows is functions that are used to work with the snapshot while it is
    // TIMNAT: already taken. These functions are used to iterate over the nodes of the snapshot.

    template <typename RecordManager>
    void AddReports(const int tid, std::vector<CompactReportItem *> * allReports, ReportItem * curr, RecordManager * recmgr) {
        curr = curr->next;
        while (curr != NULL && curr != blocker) {
            CompactReportItem * newItem = recmgr->template allocate<CompactReportItem>(tid);
#ifdef __HANDLE_STATS
            GSTATS_APPEND(tid, extra_type4_allocated_addresses, ((long long) newItem)%(1<<12));
#endif
            newItem->init(curr->node, curr->t, curr->key);
            allReports->push_back(newItem);
            curr = curr->next;
        }
    }

public:
    // An optimization: sort the reports and nodes.
    template <typename RecordManager>
    void Prepare(int tid, RecordManager * recmgr) {
        currLocations[tid*PREFETCH_SIZE_WORDS] = head;
        currRepLocations[tid*PREFETCH_SIZE_WORDS] = 0;
        if (gAllReports != NULL) return;

        std::vector<CompactReportItem *> * allReports = new std::vector<CompactReportItem *>();
        for (int i = 0; i < NUM_THREADS; i++) {
            AddReports(tid, allReports, reportHeads[i*PREFETCH_SIZE_WORDS], recmgr);
            if (gAllReports != NULL) {
                // failed to publish allReports -- clean it up
                __deallocateAllReports(tid, allReports, recmgr);
                return;
            }
        }
        assert(!active);
#ifdef SNAPCOLLECTOR_PRINT_RQS
        std::cout<<"this="<<(long long) this<<" allReports size="<<allReports->size()<<std::endl;
#endif
        std::sort(allReports->begin(), allReports->end(), compareCRI);
        if (__sync_bool_compare_and_swap(&gAllReports, NULL, allReports)) {
            // published allReports
        } else {
            // failed to publish allReports -- clean it up
            __deallocateAllReports(tid, allReports, recmgr);
        }
    }
    
    NodeType * GetNext(int tid) {
        NodeWrapper * currLoc = (NodeWrapper *) currLocations[tid*PREFETCH_SIZE_WORDS];
        int currRepLoc = currRepLocations[tid*PREFETCH_SIZE_WORDS];
        std::vector<CompactReportItem *> * allReports = gAllReports;

        while (true) {
            CompactReportItem * rep = NULL;
            K repKey = KEY_MAX;
            if (allReports->size() > currRepLoc) {
                rep = (*allReports)[currRepLoc];
                repKey = rep->key;
            }
            K nodeKey = KEY_MAX;
            NodeWrapper * next = currLoc->next;
            if (next != NULL) {
                nodeKey = next->key;
            }

            // Option 1: node key < rep key. Return node.
            if (nodeKey < repKey) {
                currLocations[tid*PREFETCH_SIZE_WORDS] = next;
                currRepLocations[tid*PREFETCH_SIZE_WORDS] = currRepLoc;
                return next->node;
            }

            // Option 2: node key == rep key 
            if (nodeKey == repKey) {
                // 2.a - both are infinity - iteration done.
                if (nodeKey == KEY_MAX) {
                    currLocations[tid*PREFETCH_SIZE_WORDS] = currLoc;
                    currRepLocations[tid*PREFETCH_SIZE_WORDS] = currRepLoc;
                    return NULL;
                }
                // node and report with the same key ::

                // skip not-needed reports
                while (currRepLoc + 1 < allReports->size()) {
                    CompactReportItem * nextRep = (*allReports)[currRepLoc + 1];
                    // dismiss a duplicate, or an insert followed by a matching delete:
                    if (rep->key == nextRep->key && rep->node == nextRep->node) {
                        currRepLoc++;
                        rep = nextRep;
                    } else {
                        break;
                    }
                }
                // standing on an insert report to a node I am holding:
                // 1. Return the current node.
                // 2. Skip over rest of reports for that key.
                if (rep->t == ReportType::Add && (NodeType *) rep->node == next->node) {
                    while (currRepLoc < allReports->size()
                            && (*allReports)[currRepLoc]->key == rep->key) {
                        currRepLoc++;
                    }
                    currRepLocations[tid*PREFETCH_SIZE_WORDS] = currRepLoc;
                    currLocations[tid*PREFETCH_SIZE_WORDS] = next;
                    return next->node;
                }
                // standing on an insert report to a different node than I hold:
                // 1. Return the reported node.
                // 2. Skip over rest of reports for that key.
                if (rep->t == ReportType::Add && (NodeType *) rep->node != next->node) {
                    NodeType * returnValue = (NodeType *) rep->node;
                    while (currRepLoc < allReports->size()
                            && (*allReports)[currRepLoc]->key == rep->key) {
                        currRepLoc++;
                    }
                    currRepLocations[tid*PREFETCH_SIZE_WORDS] = currRepLoc;
                    currLocations[tid*PREFETCH_SIZE_WORDS] = next;
                    return returnValue;
                }
                // standing on a delete report to a different node than I hold:
                // skip over it and continue the big loop.
                if (rep->t == ReportType::Remove && (NodeType *) rep->node != next->node) {
                    currRepLoc++;
                    continue;
                }
                // standing on a delete report to the node that I hold:
                // 1. advance over the node that I hold.
                // 2. advance with the report.
                // 3. continue the bigloop
                currLoc = next;
                currRepLoc++;
                continue;
            }

            // Option 3: node key > rep key
            if (nodeKey > repKey) {
                // skip not-needed reports
                while (currRepLoc + 1 < allReports->size()) {
                    CompactReportItem * nextRep = (*allReports)[currRepLoc + 1];
                    // dismiss a duplicate, or an insert followed by a matching delete:
                    if (rep->key == nextRep->key && rep->node == nextRep->node) {
                        currRepLoc++;
                        rep = nextRep;
                    } else {
                        break;
                    }
                }
                // a delete report - skip over it.
                if (rep->t == ReportType::Remove) {
                    currRepLoc++;
                    continue;
                }

                // an insert report:
                // 1. skip over rest of the reports for the same key.
                // 2. return the node.
                if (rep->t == ReportType::Add) {
                    NodeType * returnValue = (NodeType *) rep->node;
                    while (currRepLoc < allReports->size()
                            && (*allReports)[currRepLoc]->key == rep->key) {
                        currRepLoc++;
                    }
                    currRepLocations[tid*PREFETCH_SIZE_WORDS] = currRepLoc;
                    currLocations[tid*PREFETCH_SIZE_WORDS] = currLoc;
                    return returnValue;
                }
            }
        }
    }

};

#endif /* SNAPCOLLECTOR_H */

