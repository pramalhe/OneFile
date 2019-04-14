/******************************************************************************
 * Copyright (c) 2014-2018, Pedro Ramalhete, Andreia Correia
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

#ifndef _TINY_STM_LINKED_LIST_QUEUE_H_
#define _TINY_STM_LINKED_LIST_QUEUE_H_

#include <atomic>
#include <stdexcept>

#include "stms/TinySTM.hpp"

/**
 * <h1> A Linked List queue using Tiny STM</h1>

 */
template<typename T>
class TinySTMLinkedListQueue : public tinystm::tmbase {

private:
    struct Node : tinystm::tmbase {
        T* item;
        tinystm::tmtype<Node*> next {nullptr};
        Node(T* userItem) : item{userItem} { }
    };

    tinystm::tmtype<Node*>  head {nullptr};
    tinystm::tmtype<Node*>  tail {nullptr};


public:
    TinySTMLinkedListQueue(unsigned int maxThreads=0) {
        tinystm::updateTx<bool>([this] () {
            Node* sentinelNode = tinystm::tmNew<Node>(nullptr);
            head = sentinelNode;
            tail = sentinelNode;
            return true;
        });
    }


    ~TinySTMLinkedListQueue() {
        while (dequeue() != nullptr); // Drain the queue
        tinystm::updateTx<bool>([this] () {
            Node* lhead = head;
            tinystm::tmDelete(lhead);
            return true;
        });
    }


    static std::string className() { return "TinySTM-LinkedListQueue"; }


    /*
     * Progress Condition: blocking
     * Always returns true
     */
    bool enqueue(T* item, const int tid=0) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        return tinystm::updateTx<bool>([this,item] () -> bool {
            Node* newNode = tinystm::tmNew<Node>(item);
            tail->next = newNode;
            tail = newNode;
            return true;
        });
    }


    /*
     * Progress Condition: blocking
     */
    T* dequeue(const int tid=0) {
        return tinystm::updateTx<T*>([this] () -> T* {
            Node* lhead = head;
            if (lhead == tail) return nullptr;
            head = lhead->next;
            tinystm::tmDelete(lhead);
            return head->item;
        });
    }
};

#endif /* _TINY_STM_LINKED_LIST_QUEUE_H_ */
