#ifndef _TREVOR_BROWN_AB_TREE_HP_H_
#define _TREVOR_BROWN_AB_TREE_HP_H_

#include <cassert>
#include <stdexcept>
#include <algorithm>
#include "common/ThreadRegistry.hpp"
#include "ds/brown_ext_abtree_lf/brown_ext_abtree_lf_adapter.h"

/*
 * This is a wrapper to Trevor Brown's AB-Tree so we can use it in our benchmarks
 * TODO: We've enabled Hazard Pointers as memory reclamation
 */

template<typename K>
class TrevorBrownABTree {

    static const int NODE_DEGREE = 16;
    const int ANY_KEY = 0;
    const int NUM_THREADS = 128;

    //ds_adapter<NODE_DEGREE, K, reclaimer_hazardptr<K>>* tree;
    ds_adapter<NODE_DEGREE, K>* tree;

public:
    TrevorBrownABTree(int numThreads) {
        //tree = new ds_adapter<NODE_DEGREE, K, reclaimer_hazardptr<K>>(NUM_THREADS, ANY_KEY);
        tree = new ds_adapter<NODE_DEGREE, K>(NUM_THREADS, ANY_KEY);
    }

    ~TrevorBrownABTree() {
        delete tree;
    }

    // Inserts a key only if it's not already present
    bool add(K key, const int tid=0) {
        int threadID = tl_tcico.tid;
        if (threadID == ThreadCheckInCheckOut::NOT_ASSIGNED) {
            threadID = ThreadRegistry::getTID();
            tree->initThread(threadID);
        }
        return tree->insert(threadID, key, (void *) 1) != tree->getNoValue();
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

    static std::string className() { return "TrevorBrown-AB-Tree"; }

};

#endif   // _TREVOR_BROWN_AB_TREE_HP_H_
