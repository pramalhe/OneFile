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

#ifndef _TM_LINKED_LIST_QUEUE_H_
#define _TM_LINKED_LIST_QUEUE_H_

#include <atomic>
#include <stdexcept>

#include "../../stms/CRWWPSTM.hpp"
#include "../../stms/LeftRightTM.hpp"
#include "../../stms/tm.h"               // This header defines the macros for the STM being compiled
#include "MWCLF.hpp"
#include "MWCWF.hpp"
#include "CXTM.hpp"

/**
 * <h1> A Linked List queue using STM </h1>
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
class TMLinkedListQueue {

private:
    static const unsigned int MAX_THREADS = 128;
    const unsigned int maxThreads;

    struct Node : TM_BASE_TYPE {
        T* item;
        TM_TYPE<Node*> next;
        Node(T* userItem) : item{userItem}, next{nullptr} { }
    };

    alignas(128) TM_TYPE<Node*>  head {nullptr};
    alignas(128) TM_TYPE<Node*>  tail {nullptr};


public:
    TMLinkedListQueue(unsigned int maxThreads=MAX_THREADS) : maxThreads{maxThreads} {
        Node* sentinelNode = TM_ALLOC<Node>(nullptr);
        head = sentinelNode;
        tail = sentinelNode;
    }


    ~TMLinkedListQueue() {
        // TODO: replace this 0 with the actual tid otherwise we could have issues
        while (dequeue(0) != nullptr); // Drain the queue
        Node* lhead = head;
        delete lhead;
    }


    static std::string className() { return TM_NAME() + "-LinkedListQueue"; }


    /*
     *
     * Always returns true
     */
    bool enqueue(T* item, const int tid) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        return TM_WRITE_TRANSACTION<bool>([this,item] () -> bool {
                Node* newNode = TM_ALLOC<Node>(item);
                tail->next = newNode;
                tail = newNode;
                return true;
            });
    }


    /*
     *
     */
    T* dequeue(const int tid) {
        return TM_WRITE_TRANSACTION<T*>([this] () -> T* {
                Node* lhead = head;
                if (lhead == tail) return nullptr;
                head = lhead->next;
                TM_FREE(lhead);
                return head->item;
            });
    }
};

#endif /* _MWC_LINKED_LIST_QUEUE_H_ */
