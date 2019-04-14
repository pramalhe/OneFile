/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _OF_WF_LINKED_LIST_QUEUE_H_
#define _OF_WF_LINKED_LIST_QUEUE_H_

#include <atomic>
#include <stdexcept>

#include "stms/OneFileWF.hpp"

/**
 * <h1> A Linked List queue using OneFile STM (Wait-Free) </h1>
 *
 * enqueue algorithm: sequential implementation + OFWF
 * dequeue algorithm: sequential implementation + OFWF
 * Consistency: Linearizable
 * enqueue() progress: wait-free
 * dequeue() progress: wait-free
 * Memory Reclamation: wait-free Hazard Eras (integrated into OFWF)
 * enqueue min ops: 3 DCAS + 1 CAS
 * dequeue min ops: 2 DCAS + 1 CAS
 */
template<typename T>
class OFWFLinkedListQueue : public ofwf::tmbase {

private:
    struct Node : ofwf::tmbase {
        T* item;
        ofwf::tmtype<Node*> next;
        Node(T* userItem) : item{userItem}, next{nullptr} { }
    };

    ofwf::tmtype<Node*>  head {nullptr};
    ofwf::tmtype<Node*>  tail {nullptr};


public:
    OFWFLinkedListQueue(unsigned int maxThreads=0) {
        Node* sentinelNode = ofwf::tmNew<Node>(nullptr);
        head = sentinelNode;
        tail = sentinelNode;
    }


    ~OFWFLinkedListQueue() {
        while (dequeue() != nullptr); // Drain the queue
        Node* lhead = head;
        ofwf::tmDelete(lhead);
    }


    static std::string className() { return "OF-WF-LinkedListQueue"; }


    /*
     * Progress Condition: wait-free bounded
     * Always returns true
     */
    bool enqueue(T* item, const int tid=0) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        Node* newNode = ofwf::tmNew<Node>(item); // Let's allocate outside the transaction, less overhead
        return ofwf::updateTx<bool>([this,newNode] () -> bool {
            tail->next = newNode;
            tail = newNode;
            return true;
        });
    }


    /*
     * Progress Condition: wait-free bounded
     */
    T* dequeue(const int tid=0) {
        return (T*)ofwf::updateTx<T*>([this] () -> T* {
            Node* lhead = head;
            if (lhead == tail) return nullptr;
            head = lhead->next;
            ofwf::tmDelete(lhead);
            return head->item;
        });
    }
};

#endif /* _OF_WF_LINKED_LIST_QUEUE_H_ */
