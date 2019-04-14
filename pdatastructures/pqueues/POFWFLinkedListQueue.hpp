/*
 * Copyright 2017-2019
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _PERSISTENT_OF_WF_LINKED_LIST_QUEUE_H_
#define _PERSISTENT_OF_WF_LINKED_LIST_QUEUE_H_

#include <stdexcept>

#include "ptms/OneFilePTMWF.hpp"

/**
 * <h1> A Linked List queue using OneFile PTM (Wait-Free) </h1>
 *
 * enqueue algorithm: sequential implementation + OF-WF
 * dequeue algorithm: sequential implementation + OF-WF
 * Consistency: Linearizable
 * enqueue() progress: wait-free
 * dequeue() progress: wait-free
 * enqueue min ops: 2 DCAS + 1 CAS
 * dequeue min ops: 1 DCAS + 1 CAS
 */
template<typename T>
class POFWFLinkedListQueue : public pofwf::tmbase {

private:
    struct Node : pofwf::tmbase {
        pofwf::tmtype<T> item;
        pofwf::tmtype<Node*> next {nullptr};
        Node(T userItem) : item{userItem} { }
    };

    pofwf::tmtype<Node*>  head {nullptr};
    pofwf::tmtype<Node*>  tail {nullptr};


public:
    T EMPTY {};

    POFWFLinkedListQueue(unsigned int maxThreads=0) {
        pofwf::updateTx([=] () {
            Node* sentinelNode = pofwf::tmNew<Node>(EMPTY);
            head = sentinelNode;
            tail = sentinelNode;
        });
    }


    ~POFWFLinkedListQueue() {
        pofwf::updateTx([=] () {
            while (dequeue() != EMPTY); // Drain the queue
            Node* lhead = head;
            pofwf::tmDelete(lhead);
        });
    }


    static std::string className() { return "POF-WF-LinkedListQueue"; }


    /*
     * Progress Condition: wait-free
     * Always returns true
     */
    bool enqueue(T item, const int tid=0) {
        if (item == EMPTY) throw std::invalid_argument("item can not be nullptr");
        return pofwf::updateTx<bool>([this,item] () -> bool {
            Node* newNode = pofwf::tmNew<Node>(item);
            tail->next = newNode;
            tail = newNode;
            return true;
        });
    }


    /*
     * Progress Condition: wait-free
     */
    T dequeue(const int tid=0) {
        return pofwf::updateTx<T>([this] () -> T {
            Node* lhead = head;
            if (lhead == tail) return EMPTY;
            head = lhead->next;
            pofwf::tmDelete(lhead);
            return head->item;
        });
    }
};

#endif /* _PERSISTENT_OF_WF_LINKED_LIST_QUEUE_H_ */
