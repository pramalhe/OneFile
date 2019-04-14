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

#ifndef _TINY_STM_ARRAY_LINKED_LIST_QUEUE_H_
#define _TINY_STM_ARRAY_LINKED_LIST_QUEUE_H_

#include <atomic>
#include <stdexcept>

#include "stms/TinySTM.hpp"

/**
 * <h1> An Array Linked List Queue using Tiny STM </h1>
 */
template<typename T>
class TinySTMArrayLinkedListQueue {

private:
    struct Node : tinystm::tmbase {
        static const int ITEM_NUM = 1024;
        tinystm::tmtype<uint64_t> headidx {0};
        tinystm::tmtype<T*>       items[ITEM_NUM];
        tinystm::tmtype<uint64_t> tailidx {0};
        tinystm::tmtype<Node*>    next {nullptr};
        Node(T* item) {
            items[0] = item;
            tailidx = 1;
            headidx = 0;
            for (int i = 1; i < ITEM_NUM; i++) items[i] = nullptr;
        }
    };

    tinystm::tmtype<Node*>  head {nullptr};
    tinystm::tmtype<Node*>  tail {nullptr};


public:
    TinySTMArrayLinkedListQueue(unsigned int maxThreads=0) {
        tinystm::updateTx<bool>([this] () {
            Node* sentinelNode = tinystm::tmNew<Node>(nullptr);
            sentinelNode->tailidx = 0;
            head = sentinelNode;
            tail = sentinelNode;
            return true;
        });
    }


    ~TinySTMArrayLinkedListQueue() {
        while (dequeue() != nullptr); // Drain the queue
        tinystm::updateTx<bool>([this] () {
            Node* lhead = head;
            tinystm::tmDelete(lhead);
            return true;
        });
    }


    static std::string className() { return "TinySTM-ArrayLinkedListQueue"; }


    /*
     * Progress Condition: blocking
     * Always returns true
     */
    bool enqueue(T* item, const int tid=0) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        return tinystm::updateTx<bool>([this,item] () -> bool {
            Node* ltail = tail;
            uint64_t ltailidx = ltail->tailidx;
            if (ltailidx < Node::ITEM_NUM) {
                ltail->items[ltailidx] = item;
                ++ltail->tailidx;
                return true;
            }
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
            uint64_t lheadidx = lhead->headidx;
            // Check if queue is empty
            if (lhead == tail && lheadidx == tail->tailidx) return nullptr;
            if (lheadidx < Node::ITEM_NUM) {
                ++lhead->headidx;
                return lhead->items[lheadidx];
            }
            lhead = lhead->next;
            tinystm::tmDelete(head.load());
            head = lhead;
            ++lhead->headidx;
            return lhead->items[0];
        });
    }
};

#endif /* _TINY_STM_ARRAY_LINKED_LIST_QUEUE_H_ */
