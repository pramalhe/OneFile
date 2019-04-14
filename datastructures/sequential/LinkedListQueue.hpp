/******************************************************************************
 * Copyright (c) 2014-2017, Pedro Ramalhete, Andreia Correia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Concurrency Freaks nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************
 */

#ifndef _SEQUENTIAL_LINKED_LIST_QUEUE_H_
#define _SEQUENTIAL_LINKED_LIST_QUEUE_H_

/**
 * <h1> A sequential implementation of Linked List Queue </h1>
 *
 * This is meant to be used by the Universal Constructs
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T>
class LinkedListQueue {

private:
    struct Node {
        T* item;
        Node* next {nullptr};
        Node(T* userItem) : item{userItem} { }
    };

    Node*  head {nullptr};
    Node*  tail {nullptr};


public:
    LinkedListQueue(unsigned int maxThreads=0) {
        Node* sentinelNode = new Node(nullptr);
        head = sentinelNode;
        tail = sentinelNode;
    }


    // Universal Constructs need a copy constructor on the underlying data structure
    LinkedListQueue(const LinkedListQueue& other) {
        head = new Node(nullptr);
        Node* node = head;
        Node* onode = other.head->next;
        while (onode != nullptr) {
            node->next = new Node(onode->item);
            node = node->next;
            onode = onode->next;
        }
        tail = node;
    }


    ~LinkedListQueue() {
        while (dequeue(0) != nullptr); // Drain the queue
        Node* lhead = head;
        delete lhead;
    }


    static std::string className() { return "LinkedListQueue"; }


    bool enqueue(T* item, const int tid=0) {
        if (item == nullptr) return false;
        Node* newNode = new Node(item);
        tail->next = newNode;
        tail = newNode;
        return true;
    }


    T* dequeue(const int tid=0) {
        Node* lhead = head;
        if (lhead == tail) return nullptr;
        head = lhead->next;
        delete lhead;
        return head->item;
    }
};

#endif /* _SEQUENTIAL_LINKED_LIST_QUEUE_H_ */
