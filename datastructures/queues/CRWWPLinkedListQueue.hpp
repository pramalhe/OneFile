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

#ifndef _CRWWP_LINKED_LIST_QUEUE_H_
#define _CRWWP_LINKED_LIST_QUEUE_H_

#include <atomic>
#include <stdexcept>

#include "../../stms/CRWWPSTM.hpp"

/**
 * <h1> A Linked List queue using C-RW-WP STM </h1>
 *
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
class CRWWPLinkedListQueue {

private:
    struct Node : crwwpstm::tmbase {
        T* item;
        crwwpstm::tmtype<Node*> next;
        Node(T* userItem) : item{userItem}, next{nullptr} { }
    };

    alignas(128) crwwpstm::tmtype<Node*>  head {nullptr};
    alignas(128) crwwpstm::tmtype<Node*>  tail {nullptr};


public:
    CRWWPLinkedListQueue(unsigned int maxThreads=0) {
        Node* sentinelNode = new Node(nullptr);
        head = sentinelNode;
        tail = sentinelNode;
    }


    ~CRWWPLinkedListQueue() {
        while (dequeue() != nullptr); // Drain the queue
        Node* lhead = head;
        delete lhead;
    }


    static std::string className() { return "CRWWP-LinkedListQueue"; }


    /*
     * Progress Condition: lock-free
     * Always returns true
     */
    bool enqueue(T* item, const int tid=0) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        Node* newNode = crwwpstm::tmNew<Node>(item); // Let's allocate outside the transaction, less overhead
        return crwwpstm::updateTx<bool>([this,&newNode] () -> bool {
            tail->next = newNode;
            tail = newNode;
            return true;
        });
    }


    /*
     * Progress Condition: lock-free
     */
    T* dequeue(const int tid=0) {
        return crwwpstm::updateTx<T*>([this] () -> T* {
            Node* lhead = head;
            if (lhead == tail) return nullptr;
            head = lhead->next;
            crwwpstm::tmDelete(lhead);
            return head->item;
        });
    }
};

#endif /* _CRWWP_TM_LINKED_LIST_QUEUE_H_ */
