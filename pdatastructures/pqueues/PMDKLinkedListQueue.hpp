/*
 * Copyright 2017-2019
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _PERSISTENT_PMDK_LINKED_LIST_QUEUE_H_
#define _PERSISTENT_PMDK_LINKED_LIST_QUEUE_H_

#include <stdexcept>

#include "ptms/PMDKTM.hpp"

/**
 * <h1> A Linked List queue using PMDK PTM (blocking) </h1>
 */
template<typename T>
class PMDKLinkedListQueue {

private:
    struct Node {
        pmdk::persist<T> item;
        pmdk::persist<Node*> next {nullptr};
        Node(T userItem) : item{userItem} { }
    };

    alignas(128) pmdk::persist<Node*>  head {nullptr};
    alignas(128) pmdk::persist<Node*>  tail {nullptr};


public:
    T EMPTY {};

    PMDKLinkedListQueue(unsigned int maxThreads=0) {
        pmdk::PMDKTM::updateTx([&] () {
            Node* sentinelNode = pmdk::PMDKTM::tmNew<Node>(EMPTY);
            head = sentinelNode;
            tail = sentinelNode;
        });
    }


    ~PMDKLinkedListQueue() {
        pmdk::PMDKTM::updateTx([&] () {
            while (dequeue() != EMPTY); // Drain the queue
            Node* lhead = head;
            pmdk::PMDKTM::tmDelete(lhead);
        });
    }


    static std::string className() { return "PMDK-LinkedListQueue"; }


    /*
     * Progress Condition: blocking
     * Always returns true
     */
    bool enqueue(T item, const int tid=0) {
        if (item == EMPTY) throw std::invalid_argument("item can not be nullptr");
        pmdk::PMDKTM::updateTx([&] () {
            Node* newNode = pmdk::PMDKTM::tmNew<Node>(item);
            tail->next = newNode;
            tail = newNode;
        });
        return true;
    }


    /*
     * Progress Condition: blocking
     */
    T dequeue(const int tid=0) {
        T item = EMPTY;
        pmdk::PMDKTM::updateTx<T*>([&] () {
            Node* lhead = head;
            if (lhead == tail) return;
            head = lhead->next;
            pmdk::PMDKTM::tmDelete(lhead);
            item = head->item;
        });
        return item;
    }
};

#endif /* _PERSISTENT_ROMULUS_LOG_LINKED_LIST_QUEUE_H_ */
