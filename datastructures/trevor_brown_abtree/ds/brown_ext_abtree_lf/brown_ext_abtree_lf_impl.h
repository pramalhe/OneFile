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

/**
 * Implementation note:
 * The ptrs arrays of internal nodes may be modified by calls to
 * rqProvider->linearize_update_at_cas or ->linearize_update_at_write.
 * Consequently, we must access access entries in the ptrs arrays of INTERNAL
 * nodes by performing calls to read_addr and write_addr (and linearize_...).
 * 
 * However, the ptrs arrays of leaves represent fundamentally different data:
 * specifically values, or pointers to values, and NOT pointers to nodes.
 * Thus, the ptrs arrays of leaves CANNOT be modified by such calls.
 * So, we do NOT use these functions to access entries in leaves' ptrs arrays.
 */

#ifndef ABTREE_IMPL_H
#define	ABTREE_IMPL_H

#include "brown_ext_abtree_lf.h"

#define eassert(x, y) if ((x) != (y)) { cout<<"ERROR: "<<#x<<" != "<<#y<<" :: "<<#x<<"="<<x<<" "<<#y<<"="<<y<<endl; exit(-1); }

template <int DEGREE, typename K, class Compare, class RecManager>
abtree_ns::SCXRecord<DEGREE,K> * abtree_ns::abtree<DEGREE,K,Compare,RecManager>::createSCXRecord(const int tid, wrapper_info<DEGREE,K> * info) {
    
    SCXRecord<DEGREE,K> * result = DESC1_NEW(tid);
    result->c.newNode = info->newNode;
    for (int i=0;i<info->numberOfNodes;++i) {
        result->c.nodes[i] = info->nodes[i];
    }
    for (int i=0;i<info->numberOfNodesToFreeze;++i) {
        result->c.scxPtrsSeen[i] = info->scxPtrs[i];
    }
    
    int i;
    for (i=0;info->insertedNodes[i];++i) result->c.insertedNodes[i] = info->insertedNodes[i];
    result->c.insertedNodes[i] = NULL;
    for (i=0;info->deletedNodes[i];++i) result->c.deletedNodes[i] = info->deletedNodes[i];
    result->c.deletedNodes[i] = NULL;
    
    result->c.field = info->field;
    result->c.numberOfNodes = info->numberOfNodes;
    result->c.numberOfNodesToFreeze = info->numberOfNodesToFreeze;
    DESC1_INITIALIZED(tid);
    return result;
}

template <int DEGREE, typename K, class Compare, class RecManager>
abtree_ns::Node<DEGREE,K> * abtree_ns::abtree<DEGREE,K,Compare,RecManager>::allocateNode(const int tid) {
    Node<DEGREE,K> *newnode = recordmgr->template allocate<Node<DEGREE,K> >(tid);
    if (newnode == NULL) {
        COUTATOMICTID("ERROR: could not allocate node"<<endl);
        exit(-1);
    }
    rqProvider->init_node(tid, newnode);
#ifdef __HANDLE_STATS
    GSTATS_APPEND(tid, node_allocated_addresses, ((long long) newnode)%(1<<12));
#endif
    return newnode;
}

/**
 * Returns the value associated with key, or NULL if key is not present.
 */
template <int DEGREE, typename K, class Compare, class RecManager>
const pair<void*,bool> abtree_ns::abtree<DEGREE,K,Compare,RecManager>::find(const int tid, const K& key) {
    pair<void*,bool> result;
    this->recordmgr->leaveQuiescentState(tid);
    Node<DEGREE,K> * l = rqProvider->read_addr(tid, &entry->ptrs[0]);
    while (!l->isLeaf()) {
        int ix = l->getChildIndex(key, cmp);
        l = rqProvider->read_addr(tid, &l->ptrs[ix]);
    }
    int index = l->getKeyIndex(key, cmp);
    if (index < l->getKeyCount() && l->keys[index] == key) {
        result.first = l->ptrs[index]; // this is a value, not a pointer, so it cannot be modified by rqProvider->linearize_update_at_..., so we do not use read_addr
        result.second = true;
    } else {
        result.first = NO_VALUE;
        result.second = false;
    }
    this->recordmgr->enterQuiescentState(tid);
    return result;
}

template <int DEGREE, typename K, class Compare, class RecManager>
bool abtree_ns::abtree<DEGREE,K,Compare,RecManager>::contains(const int tid, const K& key) {
    return find(tid, key).second;
}

template<int DEGREE, typename K, class Compare, class RecManager>
int abtree_ns::abtree<DEGREE,K,Compare,RecManager>::rangeQuery(const int tid, const K& lo, const K& hi, K * const resultKeys, void ** const resultValues) {
    block<Node<DEGREE,K>> stack (NULL);
    recordmgr->leaveQuiescentState(tid);
    rqProvider->traversal_start(tid);

    // depth first traversal (of interesting subtrees)
    int size = 0;
    TRACE COUTATOMICTID("rangeQuery(lo="<<lo<<", hi="<<hi<<", size="<<(hi-lo+1)<<")"<<endl);

    stack.push(entry);
    while (!stack.isEmpty()) {
        Node<DEGREE,K> * node = stack.pop();
        assert(node);
        
        // if leaf node, check if we should add its keys to the traversal
        if (node->isLeaf()) {
            rqProvider->traversal_try_add(tid, node, resultKeys, resultValues, &size, lo, hi);
            
        // else if internal node, explore its children
        } else {
            // find right-most sub-tree that could contain a key in [lo, hi]
            int nkeys = node->getKeyCount();
            int r = nkeys;
            while (r > 0 && cmp(hi, (const K&) node->keys[r-1])) --r;           // subtree rooted at node->ptrs[r] contains only keys > hi

            // find left-most sub-tree that could contain a key in [lo, hi]
            int l = 0;
            while (l < nkeys && !cmp(lo, (const K&) node->keys[l])) ++l;        // subtree rooted at node->ptrs[l] contains only keys < lo

            // perform DFS from left to right (so push onto stack from right to left)
            for (int i=r;i>=l; --i) stack.push(rqProvider->read_addr(tid, &node->ptrs[i]));

//            // simply explore EVERYTHING
//            for (int i=0;i<node->getABDegree();++i) {
//                stack.push(rqProvider->read_addr(tid, &node->ptrs[i]));
//            }
        }
    }
    
    // success
    rqProvider->traversal_end(tid, resultKeys, resultValues, &size, lo, hi);
    recordmgr->enterQuiescentState(tid);
    return size;
}


template <int DEGREE, typename K, class Compare, class RecManager>
void* abtree_ns::abtree<DEGREE,K,Compare,RecManager>::doInsert(const int tid, const K& key, void * const value, const bool replace) {
    wrapper_info<DEGREE,K> _info;
    wrapper_info<DEGREE,K>* info = &_info;
    while (true) {
        /**
         * search
         */
        this->recordmgr->leaveQuiescentState(tid);
        Node<DEGREE,K>* gp = NULL;
        Node<DEGREE,K>* p = entry;
        Node<DEGREE,K>* l = rqProvider->read_addr(tid, &p->ptrs[0]);
        int ixToP = -1;
        int ixToL = 0;
        while (!l->isLeaf()) {
            ixToP = ixToL;
            ixToL = l->getChildIndex(key, cmp);
            gp = p;
            p = l;
            l = rqProvider->read_addr(tid, &l->ptrs[ixToL]);
        }

        /**
         * do the update
         */
        int keyIndex = l->getKeyIndex(key, cmp);
        if (keyIndex < l->getKeyCount() && l->keys[keyIndex] == key) {
            /**
             * if l already contains key, replace the existing value
             */
            void* const oldValue = l->ptrs[keyIndex]; // this is a value, not a pointer, so it cannot be modified by rqProvider->linearize_update_at_..., so we do not use read_addr
            if (!replace) {
                this->recordmgr->enterQuiescentState(tid);
                return oldValue;
            }
            
            // perform LLXs
            if (!llx(tid, p, NULL, 0, info->scxPtrs, info->nodes)
                     || rqProvider->read_addr(tid, &p->ptrs[ixToL]) != l) {
                this->recordmgr->enterQuiescentState(tid);
                continue;    // retry the search
            }
            info->nodes[1] = l;
            
            // create new node(s)
            Node<DEGREE,K>* n = allocateNode(tid);
            arraycopy(l->keys, 0, n->keys, 0, l->getKeyCount());
            arraycopy(l->ptrs, 0, n->ptrs, 0, l->getABDegree());    // although we are copying l->ptrs, since l is a leaf, l->ptrs CANNOT contain modified by rqProvider->linearize_update_at_..., so we do not use arraycopy_ptrs.
            n->ptrs[keyIndex] = (Node<DEGREE,K>*) value;            // similarly, we don't use write_addr here
            n->leaf = true;
            n->marked = false;
            n->scxPtr = DUMMY;
            n->searchKey = l->searchKey;
            n->size = l->size;
            n->weight = true;
            
            // construct info record to pass to SCX
            info->numberOfNodes = 2;
            info->numberOfNodesAllocated = 1;
            info->numberOfNodesToFreeze = 1;
            info->field = &p->ptrs[ixToL];
            info->newNode = n;
            info->insertedNodes[0] = n;
            info->insertedNodes[1] = NULL;
            info->deletedNodes[0] = l;
            info->deletedNodes[1] = NULL;

            if (scx(tid, info)) {
                TRACE COUTATOMICTID("replace pair ("<<key<<", "<<value<<"): SCX succeeded"<<endl);
                fixDegreeViolation(tid, n);
                this->recordmgr->enterQuiescentState(tid);
                return oldValue;
            }
            TRACE COUTATOMICTID("replace pair ("<<key<<", "<<value<<"): SCX FAILED"<<endl);
            this->recordmgr->enterQuiescentState(tid);
            this->recordmgr->deallocate(tid, n);

        } else {
            /**
             * if l does not contain key, we have to insert it
             */

            // perform LLXs
            if (!llx(tid, p, NULL, 0, info->scxPtrs, info->nodes) || rqProvider->read_addr(tid, &p->ptrs[ixToL]) != l) {
                this->recordmgr->enterQuiescentState(tid);
                continue;    // retry the search
            }
            info->nodes[1] = l;
            
            if (l->getKeyCount() < b) {
                /**
                 * Insert pair
                 */
                
                // create new node(s)
                Node<DEGREE,K>* n = allocateNode(tid);
                arraycopy(l->keys, 0, n->keys, 0, keyIndex);
                arraycopy(l->keys, keyIndex, n->keys, keyIndex+1, l->getKeyCount()-keyIndex);
                n->keys[keyIndex] = key;
                arraycopy(l->ptrs, 0, n->ptrs, 0, keyIndex); // although we are copying the ptrs array, since the source node is a leaf, ptrs CANNOT contain modified by rqProvider->linearize_update_at_..., so we do not use arraycopy_ptrs.
                arraycopy(l->ptrs, keyIndex, n->ptrs, keyIndex+1, l->getABDegree()-keyIndex);
                n->ptrs[keyIndex] = (Node<DEGREE,K>*) value; // similarly, we don't use write_addr here
                n->leaf = l->leaf;
                n->marked = false;
                n->scxPtr = DUMMY;
                n->searchKey = l->searchKey;
                n->size = l->size+1;
                n->weight = l->weight;

                // construct info record to pass to SCX
                info->numberOfNodes = 2;
                info->numberOfNodesAllocated = 1;
                info->numberOfNodesToFreeze = 1;
                info->field = &p->ptrs[ixToL];
                info->newNode = n;
                info->insertedNodes[0] = n;
                info->insertedNodes[1] = NULL;
                info->deletedNodes[0] = l;
                info->deletedNodes[1] = NULL;
                
                if (scx(tid, info)) {
                    TRACE COUTATOMICTID("insert pair ("<<key<<", "<<value<<"): SCX succeeded"<<endl);
                    fixDegreeViolation(tid, n);
                    this->recordmgr->enterQuiescentState(tid);
                    return NO_VALUE;
                }
                TRACE COUTATOMICTID("insert pair ("<<key<<", "<<value<<"): SCX FAILED"<<endl);
                this->recordmgr->enterQuiescentState(tid);
                this->recordmgr->deallocate(tid, n);
                
            } else { // assert: l->getKeyCount() == DEGREE == b)
                /**
                 * Overflow
                 */
                
                // first, we create a pair of large arrays
                // containing too many keys and pointers to fit in a single node
                K keys[DEGREE+1];
                Node<DEGREE,K>* ptrs[DEGREE+1];
                arraycopy(l->keys, 0, keys, 0, keyIndex);
                arraycopy(l->keys, keyIndex, keys, keyIndex+1, l->getKeyCount()-keyIndex);
                keys[keyIndex] = key;
                arraycopy(l->ptrs, 0, ptrs, 0, keyIndex); // although we are copying the ptrs array, since the source node is a leaf, ptrs CANNOT contain modified by rqProvider->linearize_update_at_..., so we do not use arraycopy_ptrs.
                arraycopy(l->ptrs, keyIndex, ptrs, keyIndex+1, l->getABDegree()-keyIndex);
                ptrs[keyIndex] = (Node<DEGREE,K>*) value;

                // create new node(s):
                // since the new arrays are too big to fit in a single node,
                // we replace l by a new subtree containing three new nodes:
                // a parent, and two leaves;
                // the array contents are then split between the two new leaves

                const int size1 = (DEGREE+1)/2;
                Node<DEGREE,K>* left = allocateNode(tid);
                arraycopy(keys, 0, left->keys, 0, size1);
                arraycopy(ptrs, 0, left->ptrs, 0, size1); // although we are copying the ptrs array, since the node is a leaf, ptrs CANNOT contain modified by rqProvider->linearize_update_at_..., so we do not use arraycopy_ptrs.
                left->leaf = true;
                left->marked = false;
                left->scxPtr = DUMMY;
                left->searchKey = keys[0];
                left->size = size1;
                left->weight = true;

                const int size2 = (DEGREE+1) - size1;
                Node<DEGREE,K>* right = allocateNode(tid);
                arraycopy(keys, size1, right->keys, 0, size2);
                arraycopy(ptrs, size1, right->ptrs, 0, size2); // although we are copying the ptrs array, since the node is a leaf, ptrs CANNOT contain modified by rqProvider->linearize_update_at_..., so we do not use arraycopy_ptrs.
                right->leaf = true;
                right->marked = false;
                right->scxPtr = DUMMY;
                right->searchKey = keys[size1];
                right->size = size2;
                right->weight = true;
                
                Node<DEGREE,K>* n = allocateNode(tid);
                n->keys[0] = keys[size1];
                rqProvider->write_addr(tid, &n->ptrs[0], left);
                rqProvider->write_addr(tid, &n->ptrs[1], right);
                n->leaf = false;
                n->marked = false;
                n->scxPtr = DUMMY;
                n->searchKey = keys[size1];
                n->size = 2;
                n->weight = p == entry;
                
                // note: weight of new internal node n will be zero,
                //       unless it is the root; this is because we test
                //       p == entry, above; in doing this, we are actually
                //       performing Root-Zero at the same time as this Overflow
                //       if n will become the root (of the B-slack tree)
                
                // construct info record to pass to SCX
                info->numberOfNodes = 2;
                info->numberOfNodesAllocated = 3;
                info->numberOfNodesToFreeze = 1;
                info->field = &p->ptrs[ixToL];
                info->newNode = n;
                info->insertedNodes[0] = n;
                info->insertedNodes[1] = left;
                info->insertedNodes[2] = right;
                info->insertedNodes[3] = NULL;
                info->deletedNodes[0] = l;
                info->deletedNodes[1] = NULL;

                if (scx(tid, info)) {
                    TRACE COUTATOMICTID("insert overflow ("<<key<<", "<<value<<"): SCX succeeded"<<endl);

                    // after overflow, there may be a weight violation at n,
                    // and there may be a slack violation at p
                    fixWeightViolation(tid, n);
                    this->recordmgr->enterQuiescentState(tid);
                    return NO_VALUE;
                }
                TRACE COUTATOMICTID("insert overflow ("<<key<<", "<<value<<"): SCX FAILED"<<endl);
                this->recordmgr->enterQuiescentState(tid);
                this->recordmgr->deallocate(tid, n);
                this->recordmgr->deallocate(tid, left);
                this->recordmgr->deallocate(tid, right);
            }
        }
    }
}

template <int DEGREE, typename K, class Compare, class RecManager>
const pair<void*,bool> abtree_ns::abtree<DEGREE,K,Compare,RecManager>::erase(const int tid, const K& key) {
    wrapper_info<DEGREE,K> _info;
    wrapper_info<DEGREE,K>* info = &_info;
    while (true) {
        /**
         * search
         */
        this->recordmgr->leaveQuiescentState(tid);
        Node<DEGREE,K>* gp = NULL;
        Node<DEGREE,K>* p = entry;
        Node<DEGREE,K>* l = rqProvider->read_addr(tid, &p->ptrs[0]);
        int ixToP = -1;
        int ixToL = 0;
        while (!l->isLeaf()) {
            ixToP = ixToL;
            ixToL = l->getChildIndex(key, cmp);
            gp = p;
            p = l;
            l = rqProvider->read_addr(tid, &l->ptrs[ixToL]);
        }

        /**
         * do the update
         */
        const int keyIndex = l->getKeyIndex(key, cmp);
        if (keyIndex == l->getKeyCount() || l->keys[keyIndex] != key) {
            /**
             * if l does not contain key, we are done.
             */
            this->recordmgr->enterQuiescentState(tid);
            return pair<void*,bool>(NO_VALUE,false);
        } else {
            /**
             * if l contains key, replace l by a new copy that does not contain key.
             */

            // perform LLXs
            if (!llx(tid, p, NULL, 0, info->scxPtrs, info->nodes) || rqProvider->read_addr(tid, &p->ptrs[ixToL]) != l) {
                this->recordmgr->enterQuiescentState(tid);
                continue;    // retry the search
            }
            info->nodes[1] = l;
            // create new node(s)
            Node<DEGREE,K>* n = allocateNode(tid);
            //printf("keyIndex=%d getABDegree-keyIndex=%d\n", keyIndex, l->getABDegree()-keyIndex);
            arraycopy(l->keys, 0, n->keys, 0, keyIndex);
            arraycopy(l->keys, keyIndex+1, n->keys, keyIndex, l->getKeyCount()-(keyIndex+1));
            arraycopy(l->ptrs, 0, n->ptrs, 0, keyIndex); // although we are copying the ptrs array, since the node is a leaf, ptrs CANNOT contain modified by rqProvider->linearize_update_at_..., so we do not use arraycopy_ptrs.
            arraycopy(l->ptrs, keyIndex+1, n->ptrs, keyIndex, l->getABDegree()-(keyIndex+1));
            n->leaf = true;
            n->marked = false;
            n->scxPtr = DUMMY;
            n->searchKey = l->keys[0]; // NOTE: WE MIGHT BE DELETING l->keys[0], IN WHICH CASE newL IS EMPTY. HOWEVER, newL CAN STILL BE LOCATED BY SEARCHING FOR l->keys[0], SO WE USE THAT AS THE searchKey FOR newL.
            n->size = l->size-1;
            n->weight = true;

            // construct info record to pass to SCX
            info->numberOfNodes = 2;
            info->numberOfNodesAllocated = 1;
            info->numberOfNodesToFreeze = 1;
            info->field = &p->ptrs[ixToL];
            info->newNode = n;
            info->insertedNodes[0] = n;
            info->insertedNodes[1] = NULL;
            info->deletedNodes[0] = l;
            info->deletedNodes[1] = NULL;

            void* oldValue = l->ptrs[keyIndex]; // since the node is a leaf, ptrs is not modified by any call to rqProvider->linearize_update_at_..., so we do not need to use read_addr to access it
            if (scx(tid, info)) {
                TRACE COUTATOMICTID("delete pair ("<<key<<", "<<oldValue<<"): SCX succeeded"<<endl);

                /**
                 * Compress may be needed at p after removing key from l.
                 */
                fixDegreeViolation(tid, n);
                this->recordmgr->enterQuiescentState(tid);
                return pair<void*,bool>(oldValue, true);
            }
            TRACE COUTATOMICTID("delete pair ("<<key<<", "<<oldValue<<"): SCX FAILED"<<endl);
            this->recordmgr->enterQuiescentState(tid);
            this->recordmgr->deallocate(tid, n);
        }
    }
}

/**
 * 
 * 
 * IMPLEMENTATION OF REBALANCING
 * 
 * 
 */

template <int DEGREE, typename K, class Compare, class RecManager>
bool abtree_ns::abtree<DEGREE,K,Compare,RecManager>::fixWeightViolation(const int tid, Node<DEGREE,K>* viol) {
    if (viol->weight) return false;

    // assert: viol is internal (because leaves always have weight = 1)
    // assert: viol is not entry or root (because both always have weight = 1)

    // do an optimistic check to see if viol was already removed from the tree
    if (llx(tid, viol, NULL) == FINALIZED) {
        // recall that nodes are finalized precisely when
        // they are removed from the tree
        // we hand off responsibility for any violations at viol to the
        // process that removed it.
        return false;
    }

    wrapper_info<DEGREE,K> _info;
    wrapper_info<DEGREE,K>* info = &_info;

    // try to locate viol, and fix any weight violation at viol
    while (true) {

        const K k = viol->searchKey;
        Node<DEGREE,K>* gp = NULL;
        Node<DEGREE,K>* p = entry;
        Node<DEGREE,K>* l = rqProvider->read_addr(tid, &p->ptrs[0]);
        int ixToP = -1;
        int ixToL = 0;
        while (!l->isLeaf() && l != viol) {
            ixToP = ixToL;
            ixToL = l->getChildIndex(k, cmp);
            gp = p;
            p = l;
            l = rqProvider->read_addr(tid, &l->ptrs[ixToL]);
        }

        if (l != viol) {
            // l was replaced by another update.
            // we hand over responsibility for viol to that update.
            return false;
        }

        // we cannot apply this update if p has a weight violation
        // so, we check if this is the case, and, if so, try to fix it
        if (!p->weight) {
            fixWeightViolation(tid, p);
            continue;
        }
        
        // perform LLXs
        if (!llx(tid, gp, NULL, 0, info->scxPtrs, info->nodes) || rqProvider->read_addr(tid, &gp->ptrs[ixToP]) != p) continue;    // retry the search
        if (!llx(tid, p, NULL, 1, info->scxPtrs, info->nodes) || rqProvider->read_addr(tid, &p->ptrs[ixToL]) != l) continue;      // retry the search
        if (!llx(tid, l, NULL, 2, info->scxPtrs, info->nodes)) continue;                             // retry the search

        const int c = p->getABDegree() + l->getABDegree();
        const int size = c-1;

        if (size <= b) {
            /**
             * Absorb
             */

            // create new node(s)
            // the new arrays are small enough to fit in a single node,
            // so we replace p by a new internal node.
            Node<DEGREE,K>* n = allocateNode(tid);
            arraycopy_ptrs(p->ptrs, 0, n->ptrs, 0, ixToL); // p and l are both internal, so we use arraycopy_ptrs
            arraycopy_ptrs(l->ptrs, 0, n->ptrs, ixToL, l->getABDegree());
            arraycopy_ptrs(p->ptrs, ixToL+1, n->ptrs, ixToL+l->getABDegree(), p->getABDegree()-(ixToL+1));
            arraycopy(p->keys, 0, n->keys, 0, ixToL);
            arraycopy(l->keys, 0, n->keys, ixToL, l->getKeyCount());
            arraycopy(p->keys, ixToL, n->keys, ixToL+l->getKeyCount(), p->getKeyCount()-ixToL);
            n->leaf = false; assert(!l->isLeaf());
            n->marked = false;
            n->scxPtr = DUMMY;
            n->searchKey = n->keys[0];
            n->size = size;
            n->weight = true;
            
            // construct info record to pass to SCX
            info->numberOfNodes = 3;
            info->numberOfNodesAllocated = 1;
            info->numberOfNodesToFreeze = 3;
            info->field = &gp->ptrs[ixToP];
            info->newNode = n;
//            info->insertedNodes[0] = info->deletedNodes[0] = NULL;
            info->insertedNodes[0] = n;
            info->insertedNodes[1] = NULL;
            info->deletedNodes[0] = p;
            info->deletedNodes[1] = l;
            info->deletedNodes[2] = NULL;
            
            if (scx(tid, info)) {
                TRACE COUTATOMICTID("absorb: SCX succeeded"<<endl);

                //    absorb [check: slack@n]
                //        no weight at pi(u)
                //        degree at pi(u) -> eliminated
                //        slack at pi(u) -> eliminated or slack at n
                //        weight at u -> eliminated
                //        no degree at u
                //        slack at u -> slack at n

                /**
                 * Compress may be needed at the new internal node we created
                 * (since we move grandchildren from two parents together).
                 */
                fixDegreeViolation(tid, n);
                return true;
            }
            TRACE COUTATOMICTID("absorb: SCX FAILED"<<endl);
            this->recordmgr->deallocate(tid, n);

        } else {
            /**
             * Split
             */

            // merge keys of p and l into one big array (and similarly for children)
            // (we essentially replace the pointer to l with the contents of l)
            K keys[2*DEGREE];
            Node<DEGREE,K>* ptrs[2*DEGREE];
            arraycopy_ptrs(p->ptrs, 0, ptrs, 0, ixToL); // p and l are both internal, so we use arraycopy_ptrs
            arraycopy_ptrs(l->ptrs, 0, ptrs, ixToL, l->getABDegree());
            arraycopy_ptrs(p->ptrs, ixToL+1, ptrs, ixToL+l->getABDegree(), p->getABDegree()-(ixToL+1));
            arraycopy(p->keys, 0, keys, 0, ixToL);
            arraycopy(l->keys, 0, keys, ixToL, l->getKeyCount());
            arraycopy(p->keys, ixToL, keys, ixToL+l->getKeyCount(), p->getKeyCount()-ixToL);

            // the new arrays are too big to fit in a single node,
            // so we replace p by a new internal node and two new children.
            //
            // we take the big merged array and split it into two arrays,
            // which are used to create two new children u and v.
            // we then create a new internal node (whose weight will be zero
            // if it is not the root), with u and v as its children.
            
            // create new node(s)
            const int size1 = size / 2;
            Node<DEGREE,K>* left = allocateNode(tid);
            arraycopy(keys, 0, left->keys, 0, size1-1);
            arraycopy_ptrs(ptrs, 0, left->ptrs, 0, size1);
            left->leaf = false; assert(!l->isLeaf());
            left->marked = false;
            left->scxPtr = DUMMY;
            left->searchKey = keys[0];
            left->size = size1;
            left->weight = true;

            const int size2 = size - size1;
            Node<DEGREE,K>* right = allocateNode(tid);
            arraycopy(keys, size1, right->keys, 0, size2-1);
            arraycopy_ptrs(ptrs, size1, right->ptrs, 0, size2);
            right->leaf = false;
            right->marked = false;
            right->scxPtr = DUMMY;
            right->searchKey = keys[size1];
            right->size = size2;
            right->weight = true;

            Node<DEGREE,K>* n = allocateNode(tid);
            n->keys[0] = keys[size1-1];
            rqProvider->write_addr(tid, &n->ptrs[0], left);
            rqProvider->write_addr(tid, &n->ptrs[1], right);
            n->leaf = false;
            n->marked = false;
            n->scxPtr = DUMMY;
            n->searchKey = keys[size1-1]; // note: should be the same as n->keys[0]
            n->size = 2;
            n->weight = (gp == entry);

            // note: weight of new internal node n will be zero,
            //       unless it is the root; this is because we test
            //       gp == entry, above; in doing this, we are actually
            //       performing Root-Zero at the same time as this Overflow
            //       if n will become the root (of the B-slack tree)

            // construct info record to pass to SCX
            info->numberOfNodes = 3;
            info->numberOfNodesAllocated = 3;
            info->numberOfNodesToFreeze = 3;
            info->field = &gp->ptrs[ixToP];
            info->newNode = n;
//            info->insertedNodes[0] = info->deletedNodes[0] = NULL;
            info->insertedNodes[0] = n;
            info->insertedNodes[1] = left;
            info->insertedNodes[2] = right;
            info->insertedNodes[3] = NULL;
            info->deletedNodes[0] = p;
            info->deletedNodes[1] = l;
            info->deletedNodes[2] = NULL;

            if (scx(tid, info)) {
                TRACE COUTATOMICTID("split: SCX succeeded"<<endl);

                fixWeightViolation(tid, n);
                fixDegreeViolation(tid, n);
                return true;
            }
            TRACE COUTATOMICTID("split: SCX FAILED"<<endl);
            this->recordmgr->deallocate(tid, n);
            this->recordmgr->deallocate(tid, left);
            this->recordmgr->deallocate(tid, right);
        }
    }
}

template <int DEGREE, typename K, class Compare, class RecManager>
bool abtree_ns::abtree<DEGREE,K,Compare,RecManager>::fixDegreeViolation(const int tid, Node<DEGREE,K>* viol) {
    if (viol->getABDegree() >= a || viol == entry || viol == rqProvider->read_addr(tid, &entry->ptrs[0])) {
        return false; // no degree violation at viol
    }
    
    // do an optimistic check to see if viol was already removed from the tree
    if (llx(tid, viol, NULL) == FINALIZED) {
        // recall that nodes are finalized precisely when
        // they are removed from the tree.
        // we hand off responsibility for any violations at viol to the
        // process that removed it.
        return false;
    }

    wrapper_info<DEGREE,K> _info;
    wrapper_info<DEGREE,K>* info = &_info;

    // we search for viol and try to fix any violation we find there
    // this entails performing AbsorbSibling or Distribute.
    while (true) {
        /**
         * search for viol
         */
        const K k = viol->searchKey;
        Node<DEGREE,K>* gp = NULL;
        Node<DEGREE,K>* p = entry;
        Node<DEGREE,K>* l = rqProvider->read_addr(tid, &p->ptrs[0]);
        int ixToP = -1;
        int ixToL = 0;
        while (!l->isLeaf() && l != viol) {
            ixToP = ixToL;
            ixToL = l->getChildIndex(k, cmp);
            gp = p;
            p = l;
            l = rqProvider->read_addr(tid, &l->ptrs[ixToL]);
        }

        if (l != viol) {
            // l was replaced by another update.
            // we hand over responsibility for viol to that update.
            return false;
        }
        
        // assert: gp != NULL (because if AbsorbSibling or Distribute can be applied, then p is not the root)
        
        // perform LLXs
        if (!llx(tid, gp, NULL, 0, info->scxPtrs, info->nodes)
                 || rqProvider->read_addr(tid, &gp->ptrs[ixToP]) != p) continue;   // retry the search
        if (!llx(tid, p, NULL, 1, info->scxPtrs, info->nodes) 
                 || rqProvider->read_addr(tid, &p->ptrs[ixToL]) != l) continue;     // retry the search

        int ixToS = (ixToL > 0 ? ixToL-1 : 1);
        Node<DEGREE,K>* s = rqProvider->read_addr(tid, &p->ptrs[ixToS]);
        
        // we can only apply AbsorbSibling or Distribute if there are no
        // weight violations at p, l or s.
        // so, we first check for any weight violations,
        // and fix any that we see.
        bool foundWeightViolation = false;
        if (!p->weight) {
            foundWeightViolation = true;
            fixWeightViolation(tid, p);
        }
        if (!l->weight) {
            foundWeightViolation = true;
            fixWeightViolation(tid, l);
        }
        if (!s->weight) {
            foundWeightViolation = true;
            fixWeightViolation(tid, s);
        }
        // if we see any weight violations, then either we fixed one,
        // removing one of these nodes from the tree,
        // or one of the nodes has been removed from the tree by another
        // rebalancing step, so we retry the search for viol
        if (foundWeightViolation) continue;

        // assert: there are no weight violations at p, l or s
        // assert: l and s are either both leaves or both internal nodes
        //         (because there are no weight violations at these nodes)
        
        // also note that p->size >= a >= 2
        
        Node<DEGREE,K>* left;
        Node<DEGREE,K>* right;
        int leftindex;
        int rightindex;

        if (ixToL < ixToS) {
            if (!llx(tid, l, NULL, 2, info->scxPtrs, info->nodes)) continue; // retry the search
            if (!llx(tid, s, NULL, 3, info->scxPtrs, info->nodes)) continue; // retry the search
            left = l;
            right = s;
            leftindex = ixToL;
            rightindex = ixToS;
        } else {
            if (!llx(tid, s, NULL, 2, info->scxPtrs, info->nodes)) continue; // retry the search
            if (!llx(tid, l, NULL, 3, info->scxPtrs, info->nodes)) continue; // retry the search
            left = s;
            right = l;
            leftindex = ixToS;
            rightindex = ixToL;
        }
        
        int sz = left->getABDegree() + right->getABDegree();
        assert(left->weight && right->weight);
        
        if (sz < 2*a) {
            /**
             * AbsorbSibling
             */
            
            // create new node(s))
            Node<DEGREE,K>* newl = allocateNode(tid);
            int k1=0, k2=0;
            for (int i=0;i<left->getKeyCount();++i) {
                newl->keys[k1++] = left->keys[i];
            }
            for (int i=0;i<left->getABDegree();++i) {
                if (left->isLeaf()) {
                    newl->ptrs[k2++] = left->ptrs[i];
                } else {
                    //assert(left->getKeyCount() != left->getABDegree());
                    rqProvider->write_addr(tid, &newl->ptrs[k2++], rqProvider->read_addr(tid, &left->ptrs[i]));
                }
            }
            if (!left->isLeaf()) newl->keys[k1++] = p->keys[leftindex];
            for (int i=0;i<right->getKeyCount();++i) {
                newl->keys[k1++] = right->keys[i];
            }
            for (int i=0;i<right->getABDegree();++i) {
                if (right->isLeaf()) {
                    newl->ptrs[k2++] = right->ptrs[i];
                } else {
                    rqProvider->write_addr(tid, &newl->ptrs[k2++], rqProvider->read_addr(tid, &right->ptrs[i]));
                }
            }
            newl->leaf = left->isLeaf();
            newl->marked = false;
            newl->scxPtr = DUMMY;
            newl->searchKey = l->searchKey;
            newl->size = l->getABDegree() + s->getABDegree();
            newl->weight = true; assert(left->weight && right->weight && p->weight);
            
            // now, we atomically replace p and its children with the new nodes.
            // if appropriate, we perform RootAbsorb at the same time.
            if (gp == entry && p->getABDegree() == 2) {
            
                // construct info record to pass to SCX
                info->numberOfNodes = 4; // gp + p + l + s
                info->numberOfNodesAllocated = 1; // newl
                info->numberOfNodesToFreeze = 4; // gp + p + l + s
                info->field = &gp->ptrs[ixToP];
                info->newNode = newl;
                info->insertedNodes[0] = newl;
                info->insertedNodes[1] = NULL;
                info->deletedNodes[0] = p;
                info->deletedNodes[1] = l;
                info->deletedNodes[2] = s;
                info->deletedNodes[3] = NULL;
                
                if (scx(tid, info)) {
                    TRACE COUTATOMICTID("absorbsibling AND rootabsorb: SCX succeeded"<<endl);

                    fixDegreeViolation(tid, newl);
                    return true;
                }
                TRACE COUTATOMICTID("absorbsibling AND rootabsorb: SCX FAILED"<<endl);
                this->recordmgr->deallocate(tid, newl);
                
            } else {
                assert(gp != entry || p->getABDegree() > 2);
                
                // create n from p by:
                // 1. skipping the key for leftindex and child pointer for ixToS
                // 2. replacing l with newl
                Node<DEGREE,K>* n = allocateNode(tid);
                for (int i=0;i<leftindex;++i) {
                    n->keys[i] = p->keys[i];
                }
                for (int i=0;i<ixToS;++i) {
                    rqProvider->write_addr(tid, &n->ptrs[i], rqProvider->read_addr(tid, &p->ptrs[i]));      // n and p are internal, so their ptrs arrays might have entries that are being modified by rqProvider->linearize_update_at_..., so we use read_addr and write_addr
                }
                for (int i=leftindex+1;i<p->getKeyCount();++i) {
                    n->keys[i-1] = p->keys[i];
                }
                for (int i=ixToL+1;i<p->getABDegree();++i) {
                    rqProvider->write_addr(tid, &n->ptrs[i-1], rqProvider->read_addr(tid, &p->ptrs[i]));    // n and p are internal, so their ptrs arrays might have entries that are being modified by rqProvider->linearize_update_at_..., so we use read_addr and write_addr
                }
                // replace l with newl
                rqProvider->write_addr(tid, &n->ptrs[ixToL - (ixToL > ixToS)], newl);
                n->leaf = false;
                n->marked = false;
                n->scxPtr = DUMMY;
                n->searchKey = p->searchKey;
                n->size = p->getABDegree()-1;
                n->weight = true;

                // construct info record to pass to SCX
                info->numberOfNodes = 4; // gp + p + l + s
                info->numberOfNodesAllocated = 2; // n + newl
                info->numberOfNodesToFreeze = 4; // gp + p + l + s
                info->field = &gp->ptrs[ixToP];
                info->newNode = n;
                info->insertedNodes[0] = n;
                info->insertedNodes[1] = newl;
                info->insertedNodes[2] = NULL;
                info->deletedNodes[0] = p;
                info->deletedNodes[1] = l;
                info->deletedNodes[2] = s;
                info->deletedNodes[3] = NULL;
                
                if (scx(tid, info)) {
                    TRACE COUTATOMICTID("absorbsibling: SCX succeeded"<<endl);

                    fixDegreeViolation(tid, newl);
                    fixDegreeViolation(tid, n);
                    return true;
                }
                TRACE COUTATOMICTID("absorbsibling: SCX FAILED"<<endl);
                this->recordmgr->deallocate(tid, newl);
                this->recordmgr->deallocate(tid, n);
            }
            
        } else {
            /**
             * Distribute
             */
            
            int leftsz = sz/2;
            int rightsz = sz-leftsz;
            
            // create new node(s))
            Node<DEGREE,K>* n = allocateNode(tid);
            Node<DEGREE,K>* newleft = allocateNode(tid);
            Node<DEGREE,K>* newright = allocateNode(tid);
            
            // combine the contents of l and s (and one key from p if l and s are internal)
            K keys[2*DEGREE];
            Node<DEGREE,K>* ptrs[2*DEGREE];
            int k1=0, k2=0;
            for (int i=0;i<left->getKeyCount();++i) {
                keys[k1++] = left->keys[i];
            }
            for (int i=0;i<left->getABDegree();++i) {
                if (left->isLeaf()) {
                    ptrs[k2++] = left->ptrs[i];
                } else {
                    ptrs[k2++] = rqProvider->read_addr(tid, &left->ptrs[i]);
                }
            }
            if (!left->isLeaf()) keys[k1++] = p->keys[leftindex];
            for (int i=0;i<right->getKeyCount();++i) {
                keys[k1++] = right->keys[i];
            }
            for (int i=0;i<right->getABDegree();++i) {
                if (right->isLeaf()) {
                    ptrs[k2++] = right->ptrs[i];
                } else {
                    ptrs[k2++] = rqProvider->read_addr(tid, &right->ptrs[i]);
                }
            }
            
            // distribute contents between newleft and newright
            k1=0;
            k2=0;
            for (int i=0;i<leftsz - !left->isLeaf();++i) {
                newleft->keys[i] = keys[k1++];
            }
            for (int i=0;i<leftsz;++i) {
                if (left->isLeaf()) {
                    newleft->ptrs[i] = ptrs[k2++];
                } else {
                    rqProvider->write_addr(tid, &newleft->ptrs[i], ptrs[k2++]);
                }
            }
            newleft->leaf = left->isLeaf();
            newleft->marked = false;
            newleft->scxPtr = DUMMY;
            newleft->searchKey = newleft->keys[0];
            newleft->size = leftsz;
            newleft->weight = true;
            
            // reserve one key for the parent (to go between newleft and newright)
            K keyp = keys[k1];
            if (!left->isLeaf()) ++k1;
            for (int i=0;i<rightsz - !left->isLeaf();++i) {
                newright->keys[i] = keys[k1++];
            }
            for (int i=0;i<rightsz;++i) {
                if (right->isLeaf()) {
                    newright->ptrs[i] = ptrs[k2++];
                } else {
                    rqProvider->write_addr(tid, &newright->ptrs[i], ptrs[k2++]);
                }
            }
            newright->leaf = right->isLeaf();
            newright->marked = false;
            newright->scxPtr = DUMMY;
            newright->searchKey = newright->keys[0];
            newright->size = rightsz;
            newright->weight = true;
            
            // create n from p by replacing left with newleft and right with newright,
            // and replacing one key (between these two pointers)
            for (int i=0;i<p->getKeyCount();++i) {
                n->keys[i] = p->keys[i];
            }
            for (int i=0;i<p->getABDegree();++i) {
                rqProvider->write_addr(tid, &n->ptrs[i], rqProvider->read_addr(tid, &p->ptrs[i])); // n and p are internal, so their ptrs arrays might have entries that are being modified by rqProvider->linearize_update_at_..., so we use read_addr and write_addr
            }
            n->keys[leftindex] = keyp;
            rqProvider->write_addr(tid, &n->ptrs[leftindex], newleft);
            rqProvider->write_addr(tid, &n->ptrs[rightindex], newright);
            n->leaf = false;
            n->marked = false;
            n->scxPtr = DUMMY;
            n->searchKey = p->searchKey;
            n->size = p->size;
            n->weight = true;
            
            // construct info record to pass to SCX
            info->numberOfNodes = 4; // gp + p + l + s
            info->numberOfNodesAllocated = 3; // n + newleft + newright
            info->numberOfNodesToFreeze = 4; // gp + p + l + s
            info->field = &gp->ptrs[ixToP];
            info->newNode = n;
            info->insertedNodes[0] = n;
            info->insertedNodes[1] = newleft;
            info->insertedNodes[2] = newright;
            info->insertedNodes[3] = NULL;
            info->deletedNodes[0] = p;
            info->deletedNodes[1] = l;
            info->deletedNodes[2] = s;
            info->deletedNodes[3] = NULL;
            
            if (scx(tid, info)) {
                TRACE COUTATOMICTID("distribute: SCX succeeded"<<endl);

                fixDegreeViolation(tid, n);
                return true;
            }
            TRACE COUTATOMICTID("distribute: SCX FAILED"<<endl);
            this->recordmgr->deallocate(tid, n);
            this->recordmgr->deallocate(tid, newleft);
            this->recordmgr->deallocate(tid, newright);
        }
    }
}

/**
 * 
 * IMPLEMENTATION OF LLX AND SCX
 * 
 * 
 */

template <int DEGREE, typename K, class Compare, class RecManager>
bool abtree_ns::abtree<DEGREE,K,Compare,RecManager>::llx(const int tid, Node<DEGREE,K>* r, Node<DEGREE,K> ** snapshot, const int i, SCXRecord<DEGREE,K> ** ops, Node<DEGREE,K> ** nodes) {
    SCXRecord<DEGREE,K>* result = llx(tid, r, snapshot);
    if (result == FAILED || result == FINALIZED) return false;
    ops[i] = result;
    nodes[i] = r;
    return true;
}

template <int DEGREE, typename K, class Compare, class RecManager>
abtree_ns::SCXRecord<DEGREE,K>* abtree_ns::abtree<DEGREE,K,Compare,RecManager>::llx(const int tid, Node<DEGREE,K>* r, Node<DEGREE,K> ** snapshot) {
    const bool marked = r->marked;
    SOFTWARE_BARRIER;
    tagptr_t tagptr = (tagptr_t) r->scxPtr;
    
    // read mutable state field of descriptor
    bool succ;
    TRACE COUTATOMICTID("tagged ptr seq="<<UNPACK1_SEQ(tagptr)<<" descriptor seq="<<UNPACK1_SEQ(TAGPTR1_UNPACK_PTR(tagptr)->c.mutables)<<endl);
    int state = DESC1_READ_FIELD(succ, TAGPTR1_UNPACK_PTR(tagptr)->c.mutables, tagptr, MUTABLES1_MASK_STATE, MUTABLES1_OFFSET_STATE);
    if (!succ) state = SCXRecord<DEGREE,K>::STATE_COMMITTED;
    TRACE { mutables_t debugmutables = TAGPTR1_UNPACK_PTR(tagptr)->c.mutables; COUTATOMICTID("llx scxrecord succ="<<succ<<" state="<<state<<" mutables="<<debugmutables<<" desc-seq="<<UNPACK1_SEQ(debugmutables)<<endl); }
    // note: special treatment for alg in the case where the descriptor has already been reallocated (impossible before the transformation, assuming safe memory reclamation)
    SOFTWARE_BARRIER;
    
    if (state == SCXRecord<DEGREE,K>::STATE_ABORTED || ((state == SCXRecord<DEGREE,K>::STATE_COMMITTED) && !r->marked)) {
        // read snapshot fields
        if (snapshot != NULL) {
            if (r->isLeaf()) {
                arraycopy(r->ptrs, 0, snapshot, 0, r->getABDegree());
            } else {
                arraycopy_ptrs(r->ptrs, 0, snapshot, 0, r->getABDegree());
            }
        }
        if ((tagptr_t) r->scxPtr == tagptr) return (SCXRecord<DEGREE,K> *) tagptr; // we have a snapshot
    }

    if (state == SCXRecord<DEGREE,K>::STATE_INPROGRESS) {
        helpOther(tid, tagptr);
    }
    return (marked ? FINALIZED : FAILED);
}

template<int DEGREE, typename K, class Compare, class RecManager>
bool abtree_ns::abtree<DEGREE,K,Compare,RecManager>::scx(const int tid, wrapper_info<DEGREE,K> * info) {
    const int init_state = SCXRecord<DEGREE,K>::STATE_INPROGRESS;
    SCXRecord<DEGREE,K> * newdesc = createSCXRecord(tid, info);
    tagptr_t tagptr = TAGPTR1_NEW(tid, newdesc->c.mutables);
    info->state = help(tid, tagptr, newdesc, false);
    return info->state & SCXRecord<DEGREE,K>::STATE_COMMITTED;
}

// returns true if we executed help, and false otherwise
template<int DEGREE, typename K, class Compare, class RecManager>
void abtree_ns::abtree<DEGREE,K,Compare,RecManager>::helpOther(const int tid, tagptr_t tagptr) {
    if ((void*) tagptr == DUMMY) {
        return; // deal with the dummy descriptor
    }
    SCXRecord<DEGREE,K> snap;
    if (DESC1_SNAPSHOT(&snap, tagptr, SCXRecord<DEGREE comma K>::size)) {
        help(tid, tagptr, &snap, true);
    }
}

template<int DEGREE, typename K, class Compare, class RecManager>
int abtree_ns::abtree<DEGREE,K,Compare,RecManager>::help(const int tid, const tagptr_t tagptr, SCXRecord<DEGREE,K> const * const snap, const bool helpingOther) {
#ifdef NO_HELPING
    int IGNORED_RETURN_VALUE = -1;
    if (helpingOther) return IGNORED_RETURN_VALUE;
#endif
//    TRACE COUTATOMICTID("help "<<tagptrToString(tagptr)<<" helpingOther="<<helpingOther<<" numNodes="<<snap->c.numberOfNodes<<" numToFreeze="<<snap->c.numberOfNodesToFreeze<<endl);
    SCXRecord<DEGREE,K> *ptr = TAGPTR1_UNPACK_PTR(tagptr);
    //if (helpingOther) { eassert(UNPACK1_SEQ(snap->c.mutables), UNPACK1_SEQ(tagptr)); /*assert(UNPACK1_SEQ(snap->c.mutables) == UNPACK1_SEQ(tagptr));*/ }
    // freeze sub-tree
    for (int i=helpingOther; i<snap->c.numberOfNodesToFreeze; ++i) {
        if (snap->c.nodes[i]->isLeaf()) {
            TRACE COUTATOMICTID((helpingOther?"    ":"")<<"help "<<"nodes["<<i<<"]@"<<"0x"<<((uintptr_t)(snap->c.nodes[i]))<<" is a leaf\n");
            assert(i > 0); // nodes[0] cannot be a leaf...
            continue; // do not freeze leaves
        }
        
        bool successfulCAS = __sync_bool_compare_and_swap(&snap->c.nodes[i]->scxPtr, snap->c.scxPtrsSeen[i], tagptr);
        SCXRecord<DEGREE,K> *exp = snap->c.nodes[i]->scxPtr;
//        TRACE if (successfulCAS) COUTATOMICTID((helpingOther?"    ":"")<<"help froze nodes["<<i<<"]@0x"<<((uintptr_t)snap->c.nodes[i])<<" with tagptr="<<tagptrToString((tagptr_t) snap->c.nodes[i]->scxPtr)<<endl);
        if (successfulCAS || exp == (void*) tagptr) continue; // if node is already frozen for our operation

        // note: we can get here only if:
        // 1. the state is inprogress, and we just failed a cas, and every helper will fail that cas (or an earlier one), so the scx must abort, or
        // 2. the state is committed or aborted
        // (this suggests that it might be possible to get rid of the allFrozen bit)
        
        // read mutable allFrozen field of descriptor
        bool succ;
        bool allFrozen = DESC1_READ_FIELD(succ, ptr->c.mutables, tagptr, MUTABLES1_MASK_ALLFROZEN, MUTABLES1_OFFSET_ALLFROZEN);
        if (!succ) return SCXRecord<DEGREE,K>::STATE_ABORTED;
        
        if (allFrozen) {
            TRACE COUTATOMICTID((helpingOther?"    ":"")<<"help return state "<<SCXRecord<DEGREE comma K>::STATE_COMMITTED<<" after failed freezing cas on nodes["<<i<<"]"<<endl);
            return SCXRecord<DEGREE,K>::STATE_COMMITTED;
        } else {
            const int newState = SCXRecord<DEGREE,K>::STATE_ABORTED;
            TRACE COUTATOMICTID((helpingOther?"    ":"")<<"help return state "<<newState<<" after failed freezing cas on nodes["<<i<<"]"<<endl);
            MUTABLES1_WRITE_FIELD(ptr->c.mutables, snap->c.mutables, newState, MUTABLES1_MASK_STATE, MUTABLES1_OFFSET_STATE);
            return newState;
        }
    }
    
    MUTABLES1_WRITE_BIT(ptr->c.mutables, snap->c.mutables, MUTABLES1_MASK_ALLFROZEN);
    SOFTWARE_BARRIER;
    for (int i=1; i<snap->c.numberOfNodesToFreeze; ++i) {
        if (snap->c.nodes[i]->isLeaf()) continue; // do not mark leaves
        snap->c.nodes[i]->marked = true; // finalize all but first node
    }

    // CAS in the new sub-tree (update CAS)
    rqProvider->linearize_update_at_cas(tid, snap->c.field, snap->c.nodes[1], snap->c.newNode, snap->c.insertedNodes, snap->c.deletedNodes);
//    __sync_bool_compare_and_swap(snap->c.field, snap->c.nodes[1], snap->c.newNode);
    TRACE COUTATOMICTID((helpingOther?"    ":"")<<"help CAS'ed to newNode@0x"<<((uintptr_t)snap->c.newNode)<<endl);

    MUTABLES1_WRITE_FIELD(ptr->c.mutables, snap->c.mutables, SCXRecord<DEGREE comma K>::STATE_COMMITTED, MUTABLES1_MASK_STATE, MUTABLES1_OFFSET_STATE);
    
    TRACE COUTATOMICTID((helpingOther?"    ":"")<<"help return COMMITTED after performing update cas"<<endl);
    return SCXRecord<DEGREE,K>::STATE_COMMITTED; // success
}

#endif	/* ABTREE_IMPL_H */
