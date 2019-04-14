/* 
 * Implementation of the lock-free tree of Natarajan and Mittal.
 * 
 * Heavily edited by Trevor Brown (me [at] tbrown [dot] pro).
 * (Late 2017, early 2018.)
 * 
 * Notable changes:
 * - Converted original implementation to a class.
 * - Fixed a bug: atomic_ops types don't contain "volatile," so the original
 *       implementation behaved erroneously under high contention.
 * - Fixed the original implementation's erroneous memory reclamation,
 *       which would leak many nodes.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Created on August 31, 2017, 6:22 PM
 */

#ifndef NATARAJAN_EXT_BST_LF_ADAPTER_H
#define NATARAJAN_EXT_BST_LF_ADAPTER_H

#include <iostream>
#include "errors.h"
#include "natarajan_ext_bst_lf_stage2_impl.h"

#define RECORD_MANAGER_T record_manager<Reclaim, Alloc, Pool, node_t<K, V>>
#define DATA_STRUCTURE_T natarajan_ext_bst_lf<K, V, RECORD_MANAGER_T>

template <class K, class V, class Reclaim = reclaimer_debra<K>, class Alloc = allocator_new<K>, class Pool = pool_none<K>>
class ds_adapter {
private:
    const V NO_VALUE;
    DATA_STRUCTURE_T * const tree;

public:
    ds_adapter(const K& MIN_KEY, const K& MAX_KEY, const V& _NO_VALUE, const int numThreads)
    : NO_VALUE(_NO_VALUE)
    , tree(new DATA_STRUCTURE_T(MAX_KEY, NO_VALUE, numThreads))
    {}
    ~ds_adapter() {
        delete tree;
    }
    
    V getNoValue() {
        return NO_VALUE;
    }
    
    void initThread(const int tid) {
        tree->initThread(tid);
    }
    void deinitThread(const int tid) {
        tree->deinitThread(tid);
    }

    bool contains(const int tid, const K& key) {
        return tree->find(tid, key) != getNoValue();
    }
    V insert(const int tid, const K& key, const V& val) {
        error("insert-replace not implemented for this data structure");
    }
    V insertIfAbsent(const int tid, const K& key, const V& val) {
        return tree->insertIfAbsent(tid, key, val);
    }
    V erase(const int tid, const K& key) {
        return tree->erase(tid, key);
    }
    V find(const int tid, const K& key) {
        return tree->find(tid, key);
    }
    int rangeQuery(const int tid, const K& lo, const K& hi, K * const resultKeys, V * const resultValues) {
        error("rangeQuery not implemented for this data structure");
    }
    /**
     * Sequential operation to get the number of keys in the set
     */
    int getSize() {
        return tree->getSize();
    }
    void printSummary() {
        tree->printSummary();
    }
    long long getKeyChecksum() {
        return tree->getKeyChecksum();
    }
    bool validateStructure() {
        return tree->validateStructure();
    }
    void printObjectSizes() {
        std::cout<<"sizes: node="
                 <<(sizeof(node_t<K, V>))
                 <<std::endl;
    }
};

#endif
