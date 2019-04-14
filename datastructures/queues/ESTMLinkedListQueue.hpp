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

#ifndef _ELASTIC_STM_LINKED_LIST_QUEUE_H_
#define _ELASTIC_STM_LINKED_LIST_QUEUE_H_

#include <atomic>
#include <stdexcept>

#include "stms/ESTM.hpp"

/**
 * <h1> A Linked List queue using Elastic STM (blocking) </h1>
 *
 *
 * TODO
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T>
class ESTMLinkedListQueue {

private:
    struct Node : estm::tmbase {
        T* item;
        estm::tmtype<Node*> next {nullptr};
        Node(T* userItem) : item{userItem} { }
    };

    alignas(128) estm::tmtype<Node*>  head {nullptr};
    alignas(128) estm::tmtype<Node*>  tail {nullptr};


public:
    ESTMLinkedListQueue(unsigned int maxThreads=0) {
        estm::updateTx<bool>([this] () {
            Node* sentinelNode = estm::tmNew<Node>(nullptr);
            head = sentinelNode;
            tail = sentinelNode;
            return true;
        });
    }


    ~ESTMLinkedListQueue() {
        while (dequeue() != nullptr); // Drain the queue
        estm::updateTx<bool>([this] () {
            Node* lhead = head;
            estm::tmDelete(lhead);
            return true;
        });
    }


    static std::string className() { return "ESTM-LinkedListQueue"; }


    /*
     * Progress Condition: blocking
     * Always returns true
     */
    bool enqueue(T* item, const int tid=0) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        return estm::updateTx<bool>([this,item] () -> bool {
            Node* newNode = estm::tmNew<Node>(item);
            tail->next = newNode;
            tail = newNode;
            return true;
        });
    }


    /*
     * Progress Condition: blocking
     */
    T* dequeue(const int tid=0) {
        return estm::updateTx<T*>([this] () -> T* {
            Node* lhead = head;
            if (lhead == tail) return nullptr;
            head = lhead->next;
            estm::tmDelete(lhead);
            return head->item;
        });
    }
};

#endif /* _ESTM_LINKED_LIST_QUEUE_H_ */
