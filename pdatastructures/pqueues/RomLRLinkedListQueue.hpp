/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _PERSISTENT_ROMULUS_LR_LINKED_LIST_QUEUE_H_
#define _PERSISTENT_ROMULUS_LR_LINKED_LIST_QUEUE_H_

#include <stdexcept>

#include "ptms/romuluslr/RomulusLR.hpp"

/**
 * <h1> A Linked List queue using Romulus Left-Right PTM (blocking) </h1>
 */
template<typename T>
class RomLRLinkedListQueue {

private:
    struct Node {
        romuluslr::persist<T> item;
        romuluslr::persist<Node*> next {nullptr};
        Node(T userItem) : item{userItem} { }
    };

    alignas(128) romuluslr::persist<Node*>  head {nullptr};
    alignas(128) romuluslr::persist<Node*>  tail {nullptr};


public:
    T EMPTY {};

    RomLRLinkedListQueue(unsigned int maxThreads=0) {
        romuluslr::RomulusLR::updateTx([&] () {
            Node* sentinelNode = romuluslr::RomulusLR::tmNew<Node>(EMPTY);
            head = sentinelNode;
            tail = sentinelNode;
        });
    }


    ~RomLRLinkedListQueue() {
        romuluslr::RomulusLR::updateTx([&] () {
            while (dequeue() != EMPTY); // Drain the queue
            Node* lhead = head;
            romuluslr::RomulusLR::tmDelete(lhead);
        });
    }


    static std::string className() { return "RomulusLR-LinkedListQueue"; }


    /*
     * Progress Condition: lock-free
     * Always returns true
     */
    bool enqueue(T item, const int tid=0) {
        if (item == EMPTY) throw std::invalid_argument("item can not be nullptr");
        romuluslr::RomulusLR::updateTx([&] () {
            Node* newNode = romuluslr::RomulusLR::tmNew<Node>(item);
            tail->next = newNode;
            tail = newNode;
        });
        return true;
    }


    /*
     * Progress Condition: lock-free
     */
    T dequeue(const int tid=0) {
        T item = EMPTY;
        romuluslr::RomulusLR::updateTx<T>([&] () {
            Node* lhead = head;
            if (lhead == tail) return;
            head = lhead->next;
            romuluslr::RomulusLR::tmDelete(lhead);
            item = head->item;
        });
        return item;
    }
};

#endif /* _PERSISTENT_ROMULUS_LR_LINKED_LIST_QUEUE_H_ */
