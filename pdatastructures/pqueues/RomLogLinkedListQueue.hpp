/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _PERSISTENT_ROMULUS_LOG_LINKED_LIST_QUEUE_H_
#define _PERSISTENT_ROMULUS_LOG_LINKED_LIST_QUEUE_H_

#include <stdexcept>

#include "ptms/romuluslog/RomulusLog.hpp"

/**
 * <h1> A Linked List queue using RomulusLog PTM (blocking) </h1>
 */
template<typename T>
class RomLogLinkedListQueue {

private:
    struct Node {
        romuluslog::persist<T> item;
        romuluslog::persist<Node*> next {nullptr};
        Node(T userItem) : item{userItem} { }
    };

    alignas(128) romuluslog::persist<Node*>  head {nullptr};
    alignas(128) romuluslog::persist<Node*>  tail {nullptr};


public:
    T EMPTY {};

    RomLogLinkedListQueue(unsigned int maxThreads=0) {
        romuluslog::RomulusLog::updateTx([&] () {
            Node* sentinelNode = romuluslog::RomulusLog::tmNew<Node>(EMPTY);
            head = sentinelNode;
            tail = sentinelNode;
        });
    }


    ~RomLogLinkedListQueue() {
        romuluslog::RomulusLog::updateTx([&] () {
            while (dequeue() != EMPTY); // Drain the queue
            Node* lhead = head;
            romuluslog::RomulusLog::tmDelete(lhead);
        });
    }


    static std::string className() { return "RomulusLog-LinkedListQueue"; }


    /*
     * Progress Condition: blocking
     * Always returns true
     */
    bool enqueue(T item, const int tid=0) {
        if (item == EMPTY) throw std::invalid_argument("item can not be nullptr");
        romuluslog::RomulusLog::updateTx([&] () {
            Node* newNode = romuluslog::RomulusLog::tmNew<Node>(item);
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
        romuluslog::RomulusLog::updateTx<T>([&] () {
            Node* lhead = head;
            if (lhead == tail) return;
            head = lhead->next;
            romuluslog::RomulusLog::tmDelete(lhead);
            item = head->item;
        });
        return item;
    }
};

#endif /* _PERSISTENT_ROMULUS_LOG_LINKED_LIST_QUEUE_H_ */
