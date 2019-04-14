/*
 * Copyright 2017-2019
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _PERSISTENT_OF_LF_MP_LINKED_LIST_QUEUE_H_
#define _PERSISTENT_OF_LF_MP_LINKED_LIST_QUEUE_H_

#include <stdexcept>

#include "ptms/OneFilePTMLFMultiProcess.hpp"

/**
 * <h1> A Linked List queue using OneFile Multi-Process PTM (Lock-Free) </h1>
 *
 * enqueue algorithm: sequential implementation + OF-LF
 * dequeue algorithm: sequential implementation + OF-LF
 * Consistency: Linearizable
 * enqueue() progress: lock-free
 * dequeue() progress: lock-free
 * Memory Reclamation: Hazard Eras (integrated into OF-LF)
 * enqueue min ops: 2 DCAS + 1 CAS
 * dequeue min ops: 1 DCAS + 1 CAS
 */
template<typename T>
class POFLFMPLinkedListQueue : public onefileptmlfmp::tmbase {

private:
    struct Node : onefileptmlfmp::tmbase {
        onefileptmlfmp::tmtype<T> item;
        onefileptmlfmp::tmtype<Node*> next {nullptr};
        Node(T userItem) : item{userItem} { }
    };

    onefileptmlfmp::tmtype<Node*>  head {nullptr};
    onefileptmlfmp::tmtype<Node*>  tail {nullptr};


public:
    T EMPTY {};

    POFLFMPLinkedListQueue(unsigned int maxThreads=0) {
        onefileptmlfmp::updateTx([=] () {
            Node* sentinelNode = onefileptmlfmp::tmNew<Node>(EMPTY);
            head = sentinelNode;
            tail = sentinelNode;
        });
    }


    ~POFLFMPLinkedListQueue() {
        onefileptmlfmp::updateTx([=] () {
            while (dequeue() != EMPTY); // Drain the queue
            Node* lhead = head;
            onefileptmlfmp::tmDelete(lhead);
        });
    }


    static std::string className() { return "POF-LF-MP-LinkedListQueue"; }


    /*
     * Progress Condition: lock-free
     * Always returns true
     */
    bool enqueue(T item, const int tid=0) {
        if (item == EMPTY) throw std::invalid_argument("item can not be nullptr");
        return onefileptmlfmp::updateTx<bool>([this,item] () -> bool {
            Node* newNode = onefileptmlfmp::tmNew<Node>(item);
            tail->next = newNode;
            tail = newNode;
            return true;
        });
    }


    /*
     * Progress Condition: lock-free
     */
    T dequeue(const int tid=0) {
        return onefileptmlfmp::updateTx<T>([this] () -> T {
            Node* lhead = head;
            if (lhead == tail) return EMPTY;
            head = lhead->next;
            onefileptmlfmp::tmDelete(lhead);
            return head->item;
        });
    }
};

#endif /* _PERSISTENT_OF_LF_LINKED_LIST_QUEUE_H_ */
