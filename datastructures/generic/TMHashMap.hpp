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

#ifndef _TM_NON_RESIZABLE_HASH_MAP_H_
#define _TM_NON_RESIZABLE_HASH_MAP_H_

#include <atomic>
#include <stdexcept>
#include <mutex>

#include "../../stms/tm.h"               // This header defines the macros for the STM being compiled


/**
 * <h1> A Non-Resizable Hash Map for usage with STMs </h1>
 *
 * Each node contains 4 entries (key/value) so as to provide better cache locality
 *
 *
 * TODO

 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename K, typename V>
class TMHashMap : public TM_BASE_TYPE {

private:
    // One KeyVal is 16+16 bytes, therefore, 4 KeyVals are 2 cache lines in x86 (128 bytes)
    static const int KV_NUM = 4;
    static const unsigned int MAX_THREADS = 128;
    const unsigned int maxThreads;
    const unsigned int capacity;

    struct KeyVal {
        //uint64_t          h;     // Full hash of the key, for faster comparison. TODO: add code to handle h
        TM_TYPE<K*> key {nullptr};
        TM_TYPE<V*> val {nullptr};
        KeyVal() {}
        KeyVal(K* key, V* value) : key{key}, val{value} { }
    };

    struct Node : TM_BASE_TYPE {
        KeyVal            kv[KV_NUM];
        TM_TYPE<Node*>    next {nullptr};
        Node() {}
        Node(K* key, V* value) {
            kv[0].key = key;
            kv[0].val = value;
        }
        bool isEmpty() {
            for (int i = 0; i < KV_NUM; i++) {
                if (kv[i].key != nullptr) return false;
            }
            return true;
        }
    };

    alignas(128) Node* buckets;      // An array of Nodes


    int myhash(K* key) { return 0; }  // Used only for tests

public:
    TMHashMap(unsigned int maxThreads=MAX_THREADS, unsigned int capacity=2*1024*1024) : maxThreads{maxThreads}, capacity{capacity} {
        buckets = new Node[capacity];
    }


    ~TMHashMap() {
        delete[] buckets;
    }


    std::string className() { return TM_NAME() + "-HashMap"; }


    /*
     * Progress Condition: lock-free
     * Adds a node with a key if the key is not present, otherwise replaces the value.
     * Returns the previous value (nullptr by default).
     */
    V* put(K* key, V* value, const int tid) {
        if (key == nullptr) throw std::invalid_argument("key can not be nullptr");
        if (value == nullptr) throw std::invalid_argument("value can not be nullptr");
        V* oldVal = nullptr;
        KeyVal *firstFree = nullptr;
        auto h = std::hash<K>{}(*key);
        Node* node = &buckets[h];
        while (true) {
            for (int i = 0; i < KV_NUM; i++) {
                KeyVal& kv = node->kv[i];
                if (kv.key == nullptr) {
                    // Save the first available entry, in case we need to insert somewhere
                    if (firstFree == nullptr) firstFree = &kv;
                    continue;
                }
                if (*kv.key != *key) continue;
                // Found a matching key, replace the old value with the new
                oldVal = kv.val;
                kv.val = value;
                return oldVal;
            }
            Node* lnext = node->next;
            if (lnext == nullptr) break;
            node = lnext;
        }
        // We got here without a replacement, so insert in the first available
        if (firstFree == nullptr) {
            // No available entry, allocate a node and insert it there
            Node* newNode = TM_ALLOC<Node>(key,value);
            node->next = newNode;
        } else {
            firstFree->key = key;
            firstFree->val = value;
        }
        return oldVal;
    }


    /*
     * Progress Condition: lock-free
     * Removes a key, returning the value associated with it.
     * Returns nullptr if there is no matching key.
     */
    V* removeKey(K* key, const int tid) {
        if (key == nullptr) throw std::invalid_argument("key can not be nullptr");
        auto h = std::hash<K>{}(*key);
        Node* node = &buckets[h];
        Node* prev = node;
        while (true) {
            for (int i = 0; i < KV_NUM; i++) {
                KeyVal& kv = node->kv[i];
                if (kv.key == nullptr || *kv.key != *key) continue;
                // Found a matching key, replace the old value with nullptr
                V* oldVal = kv.val;
                kv.val = nullptr;
                kv.key = nullptr;
                // Check if it's the first node and if it is empty, then unlink it and free it
                if (prev != node && node->isEmpty()) {
                    prev->next = node->next;
                    TM_FREE(node);
                }
                return oldVal;
            }
            prev = node;
            node = node->next;
            // We got to the end without a matching key, return nullptr
            if (node == nullptr) return nullptr;
        }
    }


    /*
     * Progress Condition: lock-free
     * Returns the value of associated with the key, nullptr if there is no mapping
     */
    V* get(K* key, const int tid) {
        if (key == nullptr) throw std::invalid_argument("key can not be nullptr");
        auto h = std::hash<K>{}(*key);
        Node* node = &buckets[h];
        while (true) {
            for (int i = 0; i < KV_NUM; i++) {
                KeyVal& kv = node->kv[i];
                if (kv.key == nullptr || *kv.key != *key) continue;
                return kv.val;
            }
            Node* lnext = node->next;
            if (lnext == nullptr) return nullptr;
            node = lnext;
        }
    }



    //
    // Set methods for running the usual tests and benchmarks
    //

    bool add(K* key, const int tid) {
        return TM_WRITE_TRANSACTION<bool>([&] () -> bool {
            return put(key,key, tid) == nullptr;
        });
    }

    bool remove(K* key, const int tid) {
        return TM_WRITE_TRANSACTION<bool>([this,key,tid] () -> bool {
            return removeKey(key, tid) != nullptr;
        });
    }

    bool contains(K* key, const int tid) {
        return TM_READ_TRANSACTION<bool>([this,key,tid] () -> bool {
            return get(key, tid) != nullptr;
        });
    }

    // Used only for benchmarks. It's single-threaded
    bool addAll(K** keys, const int size, const int tid) {
        for (int i = 0; i < size; i++) add(keys[i], tid);
    }

};

#endif /* _TM_NON_RESIZABLE_HASH_MAP_H_ */
