/******************************************************************************
 * Copyright (c) 2014-2016, Pedro Ramalhete, Andreia Correia
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

#ifndef _CR_SIM_QUEUE_HP_H_
#define _CR_SIM_QUEUE_HP_H_

#include <atomic>
#include <stdexcept>
#include "HazardPointersSimQueue.hpp"


/**
 * <h1> Sim Queue </h1>
 *
 * Based on the SimQueue (FK queue)
 *
 * http://thalis.cs.uoi.gr/tech_reports/publications/TR2011-01.pdf
 *
 * enqueue algorithm: P-Sim
 * dequeue algorithm: P-Sim
 * Consistency: Linearizable
 * enqueue() progress: wait-free bounded O(N_threads)
 * dequeue() progress: wait-free bounded O(N_threads)
 * Memory Reclamation: Hazard Pointers with custom scanner for Nodes. EnqState and DeqState re-usage.
 *
 * <p>
 * The paper on Hazard Pointers is named "Hazard Pointers: Safe Memory
 * Reclamation for Lock-Free objects" and it is available here:
 * http://web.cecs.pdx.edu/~walpole/class/cs510/papers/11.pdf
 *
 */
template<typename T>
class SimQueue {

private:
    static const int MAX_THREADS = 128;

    struct Node {
        T* item;
        std::atomic<Node*> next {nullptr};
        Node(T* item) : item{item} { }
    };


    struct EnqState {
        std::atomic<Node*>  tail {nullptr};         // link_a
        std::atomic<Node*>  nextNode {nullptr};     // link_b
        std::atomic<Node*>  nextTail {nullptr};     // ptr
        std::atomic<bool>   applied[MAX_THREADS];

        EnqState() {
            for(int i=0; i < MAX_THREADS; i++){
                applied[i].store(false, std::memory_order_relaxed);
            }
        }
    };


    struct DeqState {
        std::atomic<Node*>  head {nullptr};
        std::atomic<T*>     items[MAX_THREADS];
        std::atomic<bool>   applied[MAX_THREADS];

        DeqState() {
            for(int i=0; i < MAX_THREADS; i++){
                applied[i].store(false, std::memory_order_relaxed);
                items[i].store(nullptr, std::memory_order_relaxed);
            }
        }
    };


    typedef union pointer_t {
        struct StructData{
            int64_t seq : 48;
            int64_t index: 16;
        } u;           // struct_data
        int64_t raw;   // raw_data
    } pointer_t;


    const int maxThreads;

    alignas(128) std::atomic<pointer_t> enqPointer;
    alignas(128) std::atomic<pointer_t> deqPointer;
    // Enqueue requests
    alignas(128) std::atomic<T*> items[MAX_THREADS];          // Always access relaxed
    alignas(128) std::atomic<bool> enqueuers[MAX_THREADS];
    // Re-usable EnqState instances
    alignas(128) EnqState enqReused[MAX_THREADS*2];
    // Dequeue requests
    alignas(128) std::atomic<bool> dequeuers[MAX_THREADS];
    // Re-usable DeqState instances
    alignas(128) DeqState deqReused[MAX_THREADS*2];
    alignas(128) Node* pool[MAX_THREADS][MAX_THREADS];


    // Passed to Hazard Pointers
    std::function<bool(Node*)> find = [this](Node* ptr) {
        pointer_t lpointer = enqPointer.load();
        if (enqReused[lpointer.u.index].tail.load() == ptr) return true;
        /*
        lpointer = deqPointer.load();
        if (deqReused[lpointer.u.index].head.load() == ptr) return true;
        */
        return false;
    };

    HazardPointersSimQueue<Node>  hp {find, 1, maxThreads};
    const int kHpTail = 0;
    const int kHpNode = 0;

    Node* sentinel = new Node(nullptr);

public:
    SimQueue(int maxThreads=MAX_THREADS) : maxThreads(maxThreads) {
        for (int i = 0; i < maxThreads; i++) {
            enqueuers[i].store(false, std::memory_order_relaxed);
            dequeuers[i].store(false, std::memory_order_relaxed);
            for(int j=0;j<maxThreads;j++){
                pool[i][j]=new Node(nullptr);
            }
        }
        pointer_t temp;
        temp.u.seq = 0;
        temp.u.index = 0;
        enqPointer.store(temp);
        deqPointer.store(temp);
        enqReused[0].tail.store(sentinel, std::memory_order_relaxed);
        enqReused[0].nextTail.store(sentinel, std::memory_order_relaxed);
        deqReused[0].head.store(sentinel, std::memory_order_relaxed);
    }


    ~SimQueue() {
        while (dequeue(0) != nullptr); // Drain the queue
        delete deqReused[deqPointer.load().u.index].head.load();
        for (int i = 0; i < maxThreads; i++) {
            for (int j = 0; j < maxThreads; j++) {
                delete pool[i][j];
            }
        }
    }


    static std::string className() { return "SimQueue"; }


    /**
     * Progress condition: wait-free bounded
     */
    void enqueue(T* item, const int tid) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        // Publish enqueue request
        items[tid].store(item, std::memory_order_relaxed);
        bool newrequest = !enqueuers[tid].load(std::memory_order_relaxed);
        enqueuers[tid].store(newrequest);
        for (int iter = 0; iter < 3; iter++) {
            pointer_t lpointer = enqPointer.load();
            EnqState* const lstate = &enqReused[lpointer.u.index];
            Node* ltail = hp.protectPtr(kHpTail, lstate->tail.load(), tid);
            Node* lnext = lstate->nextNode.load(); // No need for HP because we don't dereference it
            Node* lnextTail = lstate->nextTail.load(); // No need for HP
            if (lpointer.raw != enqPointer.load().raw) continue;

            // Advance the tail if needed
            if (ltail->next.load() != lnext) {
                ltail->next.store(lnext, std::memory_order_release);
            }
            // Check if my request has been done
            if (lstate->applied[tid].load() == newrequest) {
                if (lpointer.raw == enqPointer.load().raw) break;
            }
            // Help opened enqueue requests, starting from zero
            Node* first = nullptr;
            Node* node = nullptr;
            const int myIndex = (lpointer.u.index == 2*tid) ? 2*tid+1 : 2*tid ;
            EnqState* const myState = &enqReused[myIndex];
            int numNodes = 0;
            for (int j = 0; j < maxThreads; j++) {
                // Check if it is an open request
                const bool enqj = enqueuers[j].load();
                myState->applied[j].store(enqj, std::memory_order_relaxed);
                if (enqj == lstate->applied[j].load()) continue;
                Node* prev = node;
                node = pool[tid][numNodes++];
                node->item = items[j].load(std::memory_order_relaxed);
                if (first == nullptr) {
                    first = node;
                } else {
                    prev->next.store(node, std::memory_order_relaxed);
                }
                if (lpointer.raw != enqPointer.load().raw) break;
            }

            // Try to apply the new sublist
            if (lpointer.raw != enqPointer.load().raw) continue;
            node->next.store(nullptr, std::memory_order_relaxed);
            myState->tail.store(lnextTail, std::memory_order_relaxed);
            myState->nextNode.store(first, std::memory_order_relaxed);
            myState->nextTail.store(node, std::memory_order_relaxed);
            pointer_t myPointer;
            myPointer.u.seq = lpointer.u.seq + 1;
            myPointer.u.index = myIndex;
            if (enqPointer.compare_exchange_strong(lpointer, myPointer)) {
                for (int k = 0; k < numNodes; k++) {   // Refill pool
                    pool[tid][k] = new Node(nullptr);
                }
            }
        }
        hp.clear(tid);
    }



    /**
     * Progress condition: wait-free bounded
     *
     * We use just one HP index, but it was though to get there.
     */
    T* dequeue(const int tid) {
        // Publish dequeue request
        bool newrequest = !dequeuers[tid].load(std::memory_order_relaxed);
        dequeuers[tid].store(newrequest);
        for (int iter = 0; iter < 2; iter++) {
            pointer_t lpointer = deqPointer.load();
            DeqState* lstate = &deqReused[lpointer.u.index];
            // Check if my request has been done
            if (lstate->applied[tid].load() == newrequest) {
                if (lpointer.raw == deqPointer.load().raw) break;
            }
            // Help opened dequeue requests, starting from turn+1
            Node* newHead = hp.protectPtr(kHpNode, lstate->head, tid);
            if (lpointer.raw != deqPointer.load().raw) continue;
            const int myIndex = (lpointer.u.index == 2*tid) ? 2*tid+1 : 2*tid ;
            DeqState* const myState = &deqReused[myIndex];
            Node* node = newHead;
            for (int j = 0; j < maxThreads; j++) {
                // Check if it is an open request
                const bool applied = lstate->applied[j].load();
                if (dequeuers[j].load() == applied) {
                    myState->items[j].store(lstate->items[j], std::memory_order_relaxed);
                    myState->applied[j].store(applied, std::memory_order_relaxed);
                    continue;
                }
                myState->applied[j].store(!applied, std::memory_order_relaxed);
                if (node->next.load() == nullptr) {
                    myState->items[j].store(nullptr,std::memory_order_relaxed);
                } else {
                    node = hp.protectPtr(kHpNode, node->next, tid);
                    if (lpointer.raw != deqPointer.load().raw) break;
                    myState->items[j].store(node->item, std::memory_order_relaxed);
                    newHead = node;
                }
            }
            if (lpointer.raw != deqPointer.load().raw) continue;
            pointer_t newDeqIndex;
            newDeqIndex.u.seq = lpointer.u.seq + 1;
            newDeqIndex.u.index = myIndex;
            myState->head.store(newHead, std::memory_order_relaxed);
            node = lstate->head;
            if (deqPointer.compare_exchange_strong(lpointer, newDeqIndex)) {
                while (node != newHead) {
                    Node* next = node->next.load();
                    hp.retire(node,tid);
                    node = next;
                }
                break;
            }
        }
        hp.clear(tid);
        return deqReused[deqPointer.load().u.index].items[tid].load();
    }
};

#endif /* _SIM_QUEUE_HP_H_ */
