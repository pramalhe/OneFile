/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _OF_LF_LINKED_LIST_QUEUE_H_
#define _OF_LF_LINKED_LIST_QUEUE_H_

#include <atomic>
#include <stdexcept>

#include "stms/OneFileLF.hpp"

/**
 * <h1> A Linked List queue using OneFile STM (Lock-Free) </h1>
 *
 * enqueue algorithm: sequential implementation + OFLF
 * dequeue algorithm: sequential implementation + OFLF
 * Consistency: Linearizable
 * enqueue() progress: lock-free
 * dequeue() progress: lock-free
 * Memory Reclamation: lock-free Hazard Eras (integrated into OFLF)
 * enqueue min ops: 2 DCAS + 1 CAS
 * dequeue min ops: 1 DCAS + 1 CAS
 */
template<typename T>
class OFLFLinkedListQueue : public oflf::tmbase {

private:
    struct Node : oflf::tmbase {
        T* item;
        oflf::tmtype<Node*> next {nullptr};
        Node(T* userItem) : item{userItem} { }
    };

    oflf::tmtype<Node*>  head {nullptr};
    oflf::tmtype<Node*>  tail {nullptr};


public:
    OFLFLinkedListQueue(unsigned int maxThreads=0) {
        Node* sentinelNode = oflf::tmNew<Node>(nullptr);
        head = sentinelNode;
        tail = sentinelNode;
    }


    ~OFLFLinkedListQueue() {
        while (dequeue() != nullptr); // Drain the queue
        Node* lhead = head;
        oflf::tmDelete(lhead);
    }


    static std::string className() { return "OF-LF-LinkedListQueue"; }


    /*
     * Progress Condition: lock-free
     * Always returns true
     */
    bool enqueue(T* item, const int tid=0) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        Node* newNode = oflf::tmNew<Node>(item); // Let's allocate outside the transaction, less overhead
        return oflf::updateTx<bool>([this,newNode] () -> bool {
            tail->next = newNode;
            tail = newNode;
            return true;
        });
    }


    /*
     * Progress Condition: lock-free
     */
    T* dequeue(const int tid=0) {
        return oflf::updateTx<T*>([this] () -> T* {
            Node* lhead = head;
            if (lhead == tail) return nullptr;
            head = lhead->next;
            oflf::tmDelete(lhead);
            return head->item;
        });
    }
};

#endif /* _OF_LF_LINKED_LIST_QUEUE_H_ */
