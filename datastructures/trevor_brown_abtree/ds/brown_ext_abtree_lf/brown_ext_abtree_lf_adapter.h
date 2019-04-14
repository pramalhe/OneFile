/* 
 * File:   bst_adapter.h
 * Author: trbot
 *
 * Created on August 31, 2017, 6:53 PM
 */

#ifndef BST_ADAPTER_H
#define BST_ADAPTER_H

#include <iostream>
#include "brown_ext_abtree_lf_impl.h"
#include "errors.h"
using namespace abtree_ns;

#define RECORD_MANAGER_T record_manager<Reclaim, Alloc, Pool, Node<DEGREE, K>>
#define DATA_STRUCTURE_T abtree<DEGREE, K, std::less<K>, RECORD_MANAGER_T>

template <int DEGREE, typename K, class Reclaim = reclaimer_debra<K>, class Alloc = allocator_new<K>, class Pool = pool_none<K>>
class ds_adapter {
private:
    const void * NO_VALUE;
    DATA_STRUCTURE_T * const ds;

public:
    ds_adapter(const int numThreads, const K ANY_KEY)
    : ds(new DATA_STRUCTURE_T(numThreads, ANY_KEY))
    {}
    ~ds_adapter() {
        delete ds;
    }
    
    void * getNoValue() {
        return ds->NO_VALUE;
    }
    
    void initThread(const int tid) {
        ds->initThread(tid);
    }
    void deinitThread(const int tid) {
        ds->deinitThread(tid);
    }

    bool contains(const int tid, const K& key) {
        return ds->contains(tid, key);
    }
    void * const insert(const int tid, const K& key, void * const val) {
        return ds->insert(tid, key, val);
    }
    void * const insertIfAbsent(const int tid, const K& key, void * const val) {
        return ds->insertIfAbsent(tid, key, val);
    }
    void * const erase(const int tid, const K& key) {
        return ds->erase(tid, key).first;
    }
    void * find(const int tid, const K& key) {
        return ds->find(tid, key).first;
    }
    int rangeQuery(const int tid, const K& lo, const K& hi, K * const resultKeys, void ** const resultValues) {
        return ds->rangeQuery(tid, lo, hi, resultKeys, resultValues);
    }
    /**
     * Sequential operation to get the number of keys in the set
     */
    int getSize() {
        return ds->getSize();
    }
    void printSummary() {
        stringstream ss;
        ss<<ds->getSizeInNodes()<<" nodes in tree";
        cout<<ss.str()<<endl;
        
        auto recmgr = ds->debugGetRecMgr();
        recmgr->printStatus();
    }
    long long getKeyChecksum() {
        return ds->debugKeySum();
    }
    bool validateStructure() {
        return true;
    }
    void printObjectSizes() {
        std::cout<<"sizes: node="
                 <<(sizeof(Node<DEGREE, K>))
                 <<" descriptor="<<(sizeof(SCXRecord<DEGREE, K>))<<" (statically allocated)"
                 <<std::endl;
    }
};

#endif
