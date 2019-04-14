/* 
 * File:   test.cpp
 * Author: trbot
 *
 * Created on June 21, 2017, 5:25 PM
 */

#include <iostream>
#include <cstdlib>
#include <cassert>
#include "snapcollector.h"
#include "rq_snapcollector.h"

using namespace std;

class Node {
public:
    int key;
    
    volatile bool marked;
    volatile long long itime;
    volatile long long dtime;
    
    Node(int key) : key(key) {}
};

class DataStructure {
public:
    inline bool isLogicallyDeleted(const int tid, Node * node) {
        return node->marked;
    }
    
    inline int getKeys(const int tid, Node * node, int * const outputKeys) {
        outputKeys[0] = node->key;
        return 1;
    }
    
    bool isInRange(const int& key, const int& lo, const int& hi) {
        return lo <= key && key <= hi;
    }
};

/*
 * 
 */
int main(int argc, char** argv) {
    DataStructure ds;
    Node node (17);
    const int numProcessors = 1;
    SnapCollector<Node> sc (numProcessors);
    sc.AddNode(&node, node.key);
    
    RQProvider<int, Node, DataStructure, true, true> prov (numProcessors, &ds);
    Node * inserted[] = {NULL};
    Node * deleted[] = {NULL};
    prov.linearize_update_at_cas(1, &node.key, 17, 18, inserted, deleted, (void *) NULL);
    
    return 0;
}

