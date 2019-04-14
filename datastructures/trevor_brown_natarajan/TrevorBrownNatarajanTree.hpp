#ifndef _TREVOR_BROWN_NATARAJAN_TREE_HP_H_
#define _TREVOR_BROWN_NATARAJAN_TREE_HP_H_

#include <cassert>
#include <stdexcept>
#include <algorithm>
#include <limits>
#include "common/ThreadRegistry.hpp"
#include "ds/natarajan_ext_bst_lf/natarajan_ext_bst_lf_adapter.h"

/*
 * This is a wrapper to Trevor Brown's implementation of Naratajan's lock=free Tree so we can use it in our benchmarks
 */

template<typename K>
class TrevorBrownNatarajanTree {
    const int NUM_THREADS = 128;
    //ds_adapter<K, K, reclaimer_hazardptr<K>>* tree;
    ds_adapter<K, K>* tree;

public:
    TrevorBrownNatarajanTree(int numThreads) {
        const int minValue = 0;
        const int maxValue = std::numeric_limits<int>::max();
        const int noValue = -1;
        //tree = new ds_adapter<K, K, reclaimer_hazardptr<K>>(minValue, maxValue, noValue, NUM_THREADS);
        tree = new ds_adapter<K, K>(minValue, maxValue, noValue, NUM_THREADS);
    }

    ~TrevorBrownNatarajanTree() {
        // TODO: deinit threads?
        delete tree;
    }

    // Inserts a key only if it's not already present
    bool add(K key, const int tid=0) {
        int threadID = tl_tcico.tid;
        if (threadID == ThreadCheckInCheckOut::NOT_ASSIGNED) {
            threadID = ThreadRegistry::getTID();
            tree->initThread(threadID);
        }
        return tree->insertIfAbsent(threadID, key, 1) != tree->getNoValue();
    }

    // Returns true only if the key was present
    bool remove(K key, const int tid=0) {
        int threadID = tl_tcico.tid;
        if (threadID == ThreadCheckInCheckOut::NOT_ASSIGNED) {
            threadID = ThreadRegistry::getTID();
            tree->initThread(threadID);
        }
        return tree->erase(threadID, key) != tree->getNoValue();
    }

    bool contains(K key, const int tid=0) {
        int threadID = tl_tcico.tid;
        if (threadID == ThreadCheckInCheckOut::NOT_ASSIGNED) {
            threadID = ThreadRegistry::getTID();
            tree->initThread(threadID);
        }
        return tree->contains(threadID, key);
    }

    // This is not fully transactionally but it's ok because we use it only on initialization.
    // We could make it fully transactionally, but we would have to increase the size of allocation/store logs.
    void addAll(K** keys, int size, const int tid=0) {
        for (int i = 0; i < size; i++) add(*keys[i], tid);
    }

    static std::string className() { return "TrevorBrown-Natarajan-Tree"; }

};

#endif   // _TREVOR_BROWN_NATARAJAN_TREE_HP_H_
