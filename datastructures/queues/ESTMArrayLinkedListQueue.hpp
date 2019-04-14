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

#ifndef _ELASTIC_STM_ARRAY_LINKED_LIST_QUEUE_H_
#define _ELASTIC_STM_ARRAY_LINKED_LIST_QUEUE_H_

#include <atomic>
#include <stdexcept>

#include "stms/ESTM.hpp"

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
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T>
class ESTMArrayLinkedListQueue {

private:
    struct Node : estm::tmbase {
        static const int ITEM_NUM = 1024;
        estm::tmtype<uint64_t> headidx {0};
        estm::tmtype<T*>       items[ITEM_NUM];
        estm::tmtype<uint64_t> tailidx {0};
        estm::tmtype<Node*>    next {nullptr};
        Node(T* item) {
            items[0] = item;
            tailidx = 1;
            headidx = 0;
            for (int i = 1; i < ITEM_NUM; i++) items[i] = nullptr;
        }
    };

    alignas(128) estm::tmtype<Node*>  head {nullptr};
    alignas(128) estm::tmtype<Node*>  tail {nullptr};


public:
    ESTMArrayLinkedListQueue(unsigned int maxThreads=0) {
        estm::updateTx<bool>([this] () {
            Node* sentinelNode = estm::tmNew<Node>(nullptr);
            sentinelNode->tailidx = 0;
            head = sentinelNode;
            tail = sentinelNode;
            return true;
        });
    }


    ~ESTMArrayLinkedListQueue() {
        while (dequeue() != nullptr); // Drain the queue
        estm::updateTx<bool>([this] () {
            Node* lhead = head;
            estm::tmDelete(lhead);
            return true;
        });
    }


    static std::string className() { return "ESTM-ArrayLinkedListQueue"; }


    /*
     * Progress Condition: blocking
     * Always returns true
     */
    bool enqueue(T* item, const int tid=0) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        return estm::updateTx<bool>([this,item] () -> bool {
            Node* ltail = tail;
            uint64_t ltailidx = ltail->tailidx;
            if (ltailidx < Node::ITEM_NUM) {
                ltail->items[ltailidx] = item;
                ++ltail->tailidx;
                return true;
            }
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
            uint64_t lheadidx = lhead->headidx;
            // Check if queue is empty
            if (lhead == tail && lheadidx == tail->tailidx) return nullptr;
            if (lheadidx < Node::ITEM_NUM) {
                ++lhead->headidx;
                return lhead->items[lheadidx];
            }
            lhead = lhead->next;
            estm::tmDelete(head.load());
            head = lhead;
            ++lhead->headidx;
            return lhead->items[0];
        });
    }
};

#endif /* _OF_WF_ARRAY_LINKED_LIST_QUEUE_H_ */
