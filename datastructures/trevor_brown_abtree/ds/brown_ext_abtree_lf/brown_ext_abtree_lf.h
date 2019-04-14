/**
 * Implementation of the dictionary ADT with a lock-free relaxed (a,b)-tree.
 * Copyright (C) 2016 Trevor Brown
 * Contact (me [at] tbrown [dot] pro) with questions or comments.
 *
 * Details of the algorithm appear in Trevor's thesis:
 *    Techniques for Constructing Efficient Lock-free Data Structures. 2017.
 * 
 * The paper leaves it up to the implementer to decide when and how to perform
 * rebalancing steps. In this implementation, we keep track of violations and
 * fix them using a recursive cleanup procedure, which is designed as follows.
 * After performing a rebalancing step that replaced a set R of nodes,
 * recursive invocations are made for every violation that appears at a newly
 * created node. Thus, any violations that were present at nodes in R are either
 * eliminated by the rebalancing step, or will be fixed by recursive calls.
 * This way, if an invocation I of this cleanup procedure is trying to fix a
 * violation at a node that has been replaced by another invocation I' of
 * cleanup, then I can hand off responsibility for fixing the violation to I'.
 * Designing the rebalancing procedure to allow responsibility to be handed
 * off in this manner is not difficult; it simply requires going through each
 * rebalancing step S and determining which nodes involved in S can have
 * violations after S (and then making a recursive call for each violation).
 * 
 * -----------------------------------------------------------------------------
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ABTREE_H
#define	ABTREE_H

#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <unistd.h>
#include <sys/types.h>
#include "record_manager.h"
#include "descriptors.h"
#include "rq_provider.h"

namespace abtree_ns {

    #ifndef TRACE
    #define TRACE if(0)
    #endif
    #ifndef DEBUG
    #define DEBUG if(0)
    #endif
    #ifndef DEBUG1
    #define DEBUG1 if(0)
    #endif
    #ifndef DEBUG2
    #define DEBUG2 if(0)
    #endif

    #define ABTREE_ENABLE_DESTRUCTOR

    using namespace std;
    
    template <int DEGREE, typename K>
    struct Node;
    
    template <int DEGREE, typename K>
    struct SCXRecord;
    
    template <int DEGREE, typename K>
    class wrapper_info {
    public:
        const static int MAX_NODES = DEGREE+2;
        Node<DEGREE,K> * nodes[MAX_NODES];
        SCXRecord<DEGREE,K> * scxPtrs[MAX_NODES];
        Node<DEGREE,K> * newNode;
        Node<DEGREE,K> * volatile * field;
        int state;
        char numberOfNodes;
        char numberOfNodesToFreeze;
        char numberOfNodesAllocated;

        // for rqProvider
        Node<DEGREE,K> * insertedNodes[MAX_NODES+1];
        Node<DEGREE,K> * deletedNodes[MAX_NODES+1];
    };

    template <int DEGREE, typename K>
    struct SCXRecord {
        const static int STATE_INPROGRESS = 0;
        const static int STATE_COMMITTED = 1;
        const static int STATE_ABORTED = 2;
        union {
            struct {
                volatile mutables_t mutables;

                int numberOfNodes;
                int numberOfNodesToFreeze;

                Node<DEGREE,K> * newNode;
                Node<DEGREE,K> * volatile * field;
                Node<DEGREE,K> * nodes[wrapper_info<DEGREE,K>::MAX_NODES];            // array of pointers to nodes
                SCXRecord<DEGREE,K> * scxPtrsSeen[wrapper_info<DEGREE,K>::MAX_NODES]; // array of pointers to scx records

                // for rqProvider
                Node<DEGREE,K> * insertedNodes[wrapper_info<DEGREE,K>::MAX_NODES+1];
                Node<DEGREE,K> * deletedNodes[wrapper_info<DEGREE,K>::MAX_NODES+1];
            } __attribute__((packed)) c; // WARNING: be careful with atomicity because of packed attribute!!! (this means no atomic vars smaller than word size, and all atomic vars must start on a word boundary when fields are packed tightly)
            char bytes[2*PREFETCH_SIZE_BYTES];
        };
        const static int size = sizeof(c);
    };
        
    template <int DEGREE, typename K>
    struct Node {
        SCXRecord<DEGREE,K> * volatile scxPtr;
        int leaf; // 0 or 1
        volatile int marked; // 0 or 1
        int weight; // 0 or 1
        int size; // degree of node
        K searchKey;
#if defined(RQ_LOCKFREE) || defined(RQ_RWLOCK) || defined(HTM_RQ_RWLOCK)
        volatile long long itime; // for use by range query algorithm
        volatile long long dtime; // for use by range query algorithm
#endif
        K keys[DEGREE];
        Node<DEGREE,K> * volatile ptrs[DEGREE];

        inline bool isLeaf() {
            return leaf;
        }
        inline int getKeyCount() {
            return isLeaf() ? size : size-1;
        }
        inline int getABDegree() {
            return size;
        }
        template <class Compare>
        inline int getChildIndex(const K& key, Compare cmp) {
            int nkeys = getKeyCount();
            int retval = 0;
            while (retval < nkeys && !cmp(key, (const K&) keys[retval])) {
                ++retval;
            }
            return retval;
        }
        template <class Compare>
        inline int getKeyIndex(const K& key, Compare cmp) {
            int nkeys = getKeyCount();
            int retval = 0;
            while (retval < nkeys && cmp((const K&) keys[retval], key)) {
                ++retval;
            }
            return retval;
        }
    };

    template <int DEGREE, typename K, class Compare, class RecManager>
    class abtree {

        // the following bool determines whether the optimization to guarantee
        // amortized constant rebalancing (at the cost of decreasing average degree
        // by at most one) is used.
        // if it is false, then an amortized logarithmic number of rebalancing steps
        // may be performed per operation, but average degree increases slightly.
        char padding0[PREFETCH_SIZE_BYTES];
        const bool ALLOW_ONE_EXTRA_SLACK_PER_NODE;

        const int b;
        const int a;

        RecManager * const recordmgr;
        RQProvider<K, void *, Node<DEGREE,K>, abtree<DEGREE,K,Compare,RecManager>, RecManager, false, false> * const rqProvider;
        char padding1[PREFETCH_SIZE_BYTES];
        Compare cmp;

        // descriptor reduction algorithm
        #ifndef comma
            #define comma ,
        #endif
        #define DESC1_ARRAY records
        #define DESC1_T SCXRecord<DEGREE comma K>
        #define MUTABLES1_OFFSET_ALLFROZEN 0
        #define MUTABLES1_OFFSET_STATE 1
        #define MUTABLES1_MASK_ALLFROZEN 0x1
        #define MUTABLES1_MASK_STATE 0x6
        #define MUTABLES1_NEW(mutables) \
            ((((mutables)&MASK1_SEQ)+(1<<OFFSET1_SEQ)) \
            | (SCXRecord<DEGREE comma K>::STATE_INPROGRESS<<MUTABLES1_OFFSET_STATE))
        #define MUTABLES1_INIT_DUMMY SCXRecord<DEGREE comma K>::STATE_COMMITTED<<MUTABLES1_OFFSET_STATE | MUTABLES1_MASK_ALLFROZEN<<MUTABLES1_OFFSET_ALLFROZEN
        #include "../descriptors/descriptors_impl.h"
        char __padding_desc[PREFETCH_SIZE_BYTES];
        DESC1_T DESC1_ARRAY[LAST_TID1+1] __attribute__ ((aligned(64)));

        char padding2[PREFETCH_SIZE_BYTES];
        Node<DEGREE,K> * entry;
        char padding3[PREFETCH_SIZE_BYTES];

        #define DUMMY       ((SCXRecord<DEGREE,K>*) (void*) TAGPTR1_STATIC_DESC(0))
        #define FINALIZED   ((SCXRecord<DEGREE,K>*) (void*) TAGPTR1_DUMMY_DESC(1))
        #define FAILED      ((SCXRecord<DEGREE,K>*) (void*) TAGPTR1_DUMMY_DESC(2))

        #define arraycopy(src, srcStart, dest, destStart, len) \
            for (int ___i=0;___i<(len);++___i) { \
                (dest)[(destStart)+___i] = (src)[(srcStart)+___i]; \
            }
        #define arraycopy_ptrs(src, srcStart, dest, destStart, len) \
            for (int ___i=0;___i<(len);++___i) { \
                rqProvider->write_addr(tid, &(dest)[(destStart)+___i], \
                        rqProvider->read_addr(tid, &(src)[(srcStart)+___i])); \
            }

    private:
        void* doInsert(const int tid, const K& key, void * const value, const bool replace);

        // returns true if the invocation of this method
        // (and not another invocation of a method performed by this method)
        // performed an scx, and false otherwise
        bool fixWeightViolation(const int tid, Node<DEGREE,K>* viol);

        // returns true if the invocation of this method
        // (and not another invocation of a method performed by this method)
        // performed an scx, and false otherwise
        bool fixDegreeViolation(const int tid, Node<DEGREE,K>* viol);

        bool llx(const int tid, Node<DEGREE,K>* r, Node<DEGREE,K> ** snapshot, const int i, SCXRecord<DEGREE,K> ** ops, Node<DEGREE,K> ** nodes);
        SCXRecord<DEGREE,K>* llx(const int tid, Node<DEGREE,K>* r, Node<DEGREE,K> ** snapshot);
        bool scx(const int tid, wrapper_info<DEGREE,K> * info);
        void helpOther(const int tid, tagptr_t tagptr);
        int help(const int tid, const tagptr_t tagptr, SCXRecord<DEGREE,K> const * const snap, const bool helpingOther);

        SCXRecord<DEGREE,K>* createSCXRecord(const int tid, wrapper_info<DEGREE,K> * info);
        Node<DEGREE,K>* allocateNode(const int tid);

        void freeSubtree(Node<DEGREE,K>* node, int* nodes) {
            const int tid = 0;
            if (node == NULL) return;
            if (!node->isLeaf()) {
                for (int i=0;i<node->getABDegree();++i) {
                    freeSubtree(node->ptrs[i], nodes);
                }
            }
            ++(*nodes);
            recordmgr->retire(tid, node);
        }

        int init[MAX_TID_POW2] = {0,};
public:
        void * const NO_VALUE;
        const int NUM_PROCESSES;
    #ifdef USE_DEBUGCOUNTERS
        debugCounters * const counters; // debug info
    #endif

        /**
         * This function must be called once by each thread that will
         * invoke any functions on this class.
         * 
         * It must be okay that we do this with the main thread and later with another thread!
         */
        void initThread(const int tid) {
            if (init[tid]) return; else init[tid] = !init[tid];
            
            recordmgr->initThread(tid);
            rqProvider->initThread(tid);
        }
        void deinitThread(const int tid) {
            if (!init[tid]) return; else init[tid] = !init[tid];

            rqProvider->deinitThread(tid);
            recordmgr->deinitThread(tid);
        }

        /**
         * Creates a new relaxed (a,b)-tree wherein: <br>
         *      each internal node has up to <code>DEGREE</code> child pointers, and <br>
         *      each leaf has up to <code>DEGREE</code> key/value pairs, and <br>
         *      keys are ordered according to the provided comparator.
         */
        abtree(const int numProcesses, 
                const K anyKey,
                int suspectedCrashSignal = SIGQUIT)
        : ALLOW_ONE_EXTRA_SLACK_PER_NODE(true)
        , b(DEGREE)
        , a(DEGREE/2 - 2)
        , recordmgr(new RecManager(numProcesses, suspectedCrashSignal))
        , rqProvider(new RQProvider<K, void *, Node<DEGREE,K>, abtree<DEGREE,K,Compare,RecManager>, RecManager, false, false>(numProcesses, this, recordmgr))
        , NO_VALUE((void *) -1LL)
        , NUM_PROCESSES(numProcesses) 
        {
            cmp = Compare();
            
            const int tid = 0;
            initThread(tid);

            recordmgr->enterQuiescentState(tid);
            
            DESC1_INIT_ALL(numProcesses);

            SCXRecord<DEGREE,K> *dummy = TAGPTR1_UNPACK_PTR(DUMMY);
            dummy->c.mutables = MUTABLES1_INIT_DUMMY;
            TRACE COUTATOMICTID("DUMMY mutables="<<dummy->c.mutables<<endl);

            // initial tree: entry is a sentinel node (with one pointer and no keys)
            //               that points to an empty node (no pointers and no keys)
            Node<DEGREE,K>* _entryLeft = allocateNode(tid);
            _entryLeft->scxPtr = DUMMY;
            _entryLeft->leaf = true;
            _entryLeft->marked = false;
            _entryLeft->weight = true;
            _entryLeft->size = 0;
            _entryLeft->searchKey = anyKey;

            Node<DEGREE,K>* _entry = allocateNode(tid);
            _entry = allocateNode(tid);
            _entry->scxPtr = DUMMY;
            _entry->leaf = false;
            _entry->marked = false;
            _entry->weight = true;
            _entry->size = 1;
            _entry->searchKey = anyKey;
            _entry->ptrs[0] = _entryLeft;
            
            // need to simulate real insertion of root and the root's child,
            // since range queries will actually try to add these nodes,
            // and we don't want blocking rq providers to spin forever
            // waiting for their itimes to be set to a positive number.
            Node<DEGREE,K>* insertedNodes[] = {_entry, _entryLeft, NULL};
            Node<DEGREE,K>* deletedNodes[] = {NULL};
            rqProvider->linearize_update_at_write(tid, &entry, _entry, insertedNodes, deletedNodes);
        }

    #ifdef ABTREE_ENABLE_DESTRUCTOR    
        ~abtree() {
            int nodes = 0;
            freeSubtree(entry, &nodes);
//            COUTATOMIC("main thread: deleted tree containing "<<nodes<<" nodes"<<endl);
            delete rqProvider;
//            recordmgr->printStatus();
            delete recordmgr;
        }
    #endif

        Node<DEGREE,K> * debug_getEntryPoint() { return entry; }

    private:
        /*******************************************************************
         * Utility functions for integration with the test harness
         *******************************************************************/

        int sequentialSize(Node<DEGREE,K>* node) {
            if (node->isLeaf()) {
                return node->getKeyCount();
            }
            int retval = 0;
            for (int i=0;i<node->getABDegree();++i) {
                Node<DEGREE,K>* child = node->ptrs[i];
                retval += sequentialSize(child);
            }
            return retval;
        }
        int sequentialSize() {
            return sequentialSize(entry->ptrs[0]);
        }

        int getNumberOfLeaves(Node<DEGREE,K>* node) {
            if (node == NULL) return 0;
            if (node->isLeaf()) return 1;
            int result = 0;
            for (int i=0;i<node->getABDegree();++i) {
                result += getNumberOfLeaves(node->ptrs[i]);
            }
            return result;
        }
        const int getNumberOfLeaves() {
            return getNumberOfLeaves(entry->ptrs[0]);
        }
        int getNumberOfInternals(Node<DEGREE,K>* node) {
            if (node == NULL) return 0;
            if (node->isLeaf()) return 0;
            int result = 1;
            for (int i=0;i<node->getABDegree();++i) {
                result += getNumberOfInternals(node->ptrs[i]);
            }
            return result;
        }
        const int getNumberOfInternals() {
            return getNumberOfInternals(entry->ptrs[0]);
        }
        const int getNumberOfNodes() {
            return getNumberOfLeaves() + getNumberOfInternals();
        }

        int getSumOfKeyDepths(Node<DEGREE,K>* node, int depth) {
            if (node == NULL) return 0;
            if (node->isLeaf()) return depth * node->getKeyCount();
            int result = 0;
            for (int i=0;i<node->getABDegree();i++) {
                result += getSumOfKeyDepths(node->ptrs[i], 1+depth);
            }
            return result;
        }
        const int getSumOfKeyDepths() {
            return getSumOfKeyDepths(entry->ptrs[0], 0);
        }
        const double getAverageKeyDepth() {
            long sz = sequentialSize();
            return (sz == 0) ? 0 : getSumOfKeyDepths() / sz;
        }

        int getHeight(Node<DEGREE,K>* node, int depth) {
            if (node == NULL) return 0;
            if (node->isLeaf()) return 0;
            int result = 0;
            for (int i=0;i<node->getABDegree();i++) {
                int retval = getHeight(node->ptrs[i], 1+depth);
                if (retval > result) result = retval;
            }
            return result+1;
        }
        const int getHeight() {
            return getHeight(entry->ptrs[0], 0);
        }

        int getKeyCount(Node<DEGREE,K>* entry) {
            if (entry == NULL) return 0;
            if (entry->isLeaf()) return entry->getKeyCount();
            int sum = 0;
            for (int i=0;i<entry->getABDegree();++i) {
                sum += getKeyCount(entry->ptrs[i]);
            }
            return sum;
        }
        int getTotalDegree(Node<DEGREE,K>* entry) {
            if (entry == NULL) return 0;
            int sum = entry->getKeyCount();
            if (entry->isLeaf()) return sum;
            for (int i=0;i<entry->getABDegree();++i) {
                sum += getTotalDegree(entry->ptrs[i]);
            }
            return 1+sum; // one more children than keys
        }
        int getNodeCount(Node<DEGREE,K>* entry) {
            if (entry == NULL) return 0;
            if (entry->isLeaf()) return 1;
            int sum = 1;
            for (int i=0;i<entry->getABDegree();++i) {
                sum += getNodeCount(entry->ptrs[i]);
            }
            return sum;
        }
        double getAverageDegree() {
            return getTotalDegree(entry) / (double) getNodeCount(entry);
        }
        double getSpacePerKey() {
            return getNodeCount(entry)*2*b / (double) getKeyCount(entry);
        }

        long long getSumOfKeys(Node<DEGREE,K>* node) {
            TRACE COUTATOMIC("  getSumOfKeys("<<node<<"): isLeaf="<<node->isLeaf()<<endl);
            long long sum = 0;
            if (node->isLeaf()) {
                TRACE COUTATOMIC("      leaf sum +=");
                for (int i=0;i<node->getKeyCount();++i) {
                    sum += (long long) node->keys[i];
                    TRACE COUTATOMIC(node->keys[i]);
                }
                TRACE COUTATOMIC(endl);
            } else {
                for (int i=0;i<node->getABDegree();++i) {
                    sum += getSumOfKeys(node->ptrs[i]);
                }
            }
            TRACE COUTATOMIC("  getSumOfKeys("<<node<<"): sum="<<sum<<endl);
            return sum;
        }
        long long getSumOfKeys() {
            TRACE COUTATOMIC("getSumOfKeys()"<<endl);
            return getSumOfKeys(entry);
        }

        void abtree_error(string s) {
            cerr<<"ERROR: "<<s<<endl;
            exit(-1);
        }

        void debugPrint() {
            cout<<"averageDegree="<<getAverageDegree()<<endl;
            cout<<"averageDepth="<<getAverageKeyDepth()<<endl;
            cout<<"height="<<getHeight()<<endl;
            cout<<"internalNodes="<<getNumberOfInternals()<<endl;
            cout<<"leafNodes="<<getNumberOfLeaves()<<endl;
        }

    public:
        void * const insert(const int tid, const K& key, void * const val) {
            return doInsert(tid, key, val, true);
        }
        void * const insertIfAbsent(const int tid, const K& key, void * const val) {
            return doInsert(tid, key, val, false);
        }
        const pair<void*,bool> erase(const int tid, const K& key);
        const pair<void*,bool> find(const int tid, const K& key);
        bool contains(const int tid, const K& key);
        int rangeQuery(const int tid, const K& low, const K& hi, K * const resultKeys, void ** const resultValues);
        bool validate(const long long keysum, const bool checkkeysum) {
            if (checkkeysum) {
                long long treekeysum = getSumOfKeys();
                if (treekeysum != keysum) {
                    cerr<<"ERROR: tree keysum "<<treekeysum<<" did not match thread keysum "<<keysum<<endl;
                    return false;
                }
            }
            return true;
        }

        /**
         * BEGIN FUNCTIONS FOR RANGE QUERY SUPPORT
         */

        inline bool isLogicallyDeleted(const int tid, Node<DEGREE,K> * node) {
            return false;
        }

        inline int getKeys(const int tid, Node<DEGREE,K> * node, K * const outputKeys, void ** const outputValues) {
            if (node->isLeaf()) {
                // leaf ==> its keys are in the set.
                const int sz = node->getKeyCount();
                for (int i=0;i<sz;++i) {
                    outputKeys[i] = node->keys[i];
                    outputValues[i] = (void *) node->ptrs[i];
                }
                return sz;
            }
            // note: internal ==> its keys are NOT in the set
            return 0;
        }

        bool isInRange(const K& key, const K& lo, const K& hi) {
            return (!cmp(key, lo) && !cmp(hi, key));
        }

        /**
         * END FUNCTIONS FOR RANGE QUERY SUPPORT
         */

        long long getSizeInNodes() {
            return getNumberOfNodes();
        }
        string getSizeString() {
            stringstream ss;
            int preallocated = wrapper_info<DEGREE,K>::MAX_NODES * recordmgr->NUM_PROCESSES;
            ss<<getSizeInNodes()<<" nodes in tree";
            return ss.str();
        }
        long long getSize(Node<DEGREE,K> * node) {
            return sequentialSize(node);
        }
        long long getSize() {
            return sequentialSize();
        }
        RecManager * const debugGetRecMgr() {
            return recordmgr;
        }
        long long debugKeySum() {
            return getSumOfKeys();
        }
    };
} // namespace

#endif	/* ABTREE_H */

