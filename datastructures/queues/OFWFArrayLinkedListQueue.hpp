/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _OF_WF_ARRAY_LINKED_LIST_QUEUE_H_
#define _OF_WF_ARRAY_LINKED_LIST_QUEUE_H_

#include <atomic>
#include <stdexcept>

#include "stms/OneFileWF.hpp"

/**
 * <h1> An Array Linked List Queue using OneFile STM (Wait-Free) </h1>
 *
 * TODO
 *
 *
 * enqueue algorithm: sequential implementation + MWC
 * dequeue algorithm: sequential implementation + MWC
 * Consistency: Linearizable
 * enqueue() progress: lock-free
 * dequeue() progress: lock-free
 * Memory Reclamation: Hazard Eras (integrated into MWC)
 * enqueue min ops: 2 DCAS + 1 CAS
 * dequeue min ops: 1 DCAS + 1 CAS
 */
template<typename T>
class OFWFArrayLinkedListQueue  : public ofwf::tmbase {

private:
    struct Node : ofwf::tmbase {
        static const int ITEM_NUM = 1024;
        ofwf::tmtype<uint64_t> headidx {0};
        ofwf::tmtype<T*>       items[ITEM_NUM];
        ofwf::tmtype<uint64_t> tailidx {0};
        ofwf::tmtype<Node*>    next {nullptr};
        Node(T* item) {
            items[0] = item;
            tailidx = 1;
            headidx = 0;
            for (int i = 1; i < ITEM_NUM; i++) items[i] = nullptr;
        }
    };

    ofwf::tmtype<Node*>  head {nullptr};
    ofwf::tmtype<Node*>  tail {nullptr};


public:
    OFWFArrayLinkedListQueue(unsigned int maxThreads=0) {
        Node* sentinelNode = new Node(nullptr);
        sentinelNode->tailidx = 0;
        head = sentinelNode;
        tail = sentinelNode;
    }


    ~OFWFArrayLinkedListQueue() {
        while (dequeue(0) != nullptr); // Drain the queue
        Node* lhead = head;
        delete lhead;
    }


    static std::string className() { return "OF-WF-ArrayLinkedListQueue"; }


    /*
     * Progress Condition: lock-free
     * Always returns true
     */
    bool enqueue(T* item, const int tid=0) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        return ofwf::updateTx<bool>([this,item] () -> bool {
            Node* ltail = tail;
            uint64_t ltailidx = ltail->tailidx;
            if (ltailidx < Node::ITEM_NUM) {
                ltail->items[ltailidx] = item;
                ++ltail->tailidx;
                return true;
            }
            Node* newNode = ofwf::tmNew<Node>(item);
            tail->next = newNode;
            tail = newNode;
            return true;
        });
    }


    /*
     * Progress Condition: lock-free
     */
    T* dequeue(const int tid=0) {
        return ofwf::updateTx<T*>([this] () -> T* {
            Node* lhead = head;
            uint64_t lheadidx = lhead->headidx;
            // Check if queue is empty
            if (lhead == tail && lheadidx == tail->tailidx) return nullptr;
            if (lheadidx < Node::ITEM_NUM) {
                ++lhead->headidx;
                return lhead->items[lheadidx];
            }
            lhead = lhead->next;
            ofwf::tmDelete<Node>(head);
            head = lhead;
            ++lhead->headidx;
            return lhead->items[0];
        });
    }
};

#endif /* _OF_WF_ARRAY_LINKED_LIST_QUEUE_H_ */
