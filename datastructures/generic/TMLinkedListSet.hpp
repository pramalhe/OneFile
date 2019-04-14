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

#ifndef _TM_LINKED_LIST_SET_H_
#define _TM_LINKED_LIST_SET_H_

#include <atomic>
#include <stdexcept>

#include "../../stms/tm.h"               // This header defines the macros for the STM being compiled


/**
 * <h1> A Linked List Set for usage with STMs </h1>
 *
 * TODO
 *
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T>
class TMLinkedListSet : public TM_BASE_TYPE {

private:
    static const unsigned int MAX_THREADS = 128;
    const unsigned int maxThreads;

    struct Node : public TM_BASE_TYPE {
        T* key;
        TM_TYPE<Node*> next;
        Node(T* key) : key{key}, next{nullptr} { }
    };

    alignas(128) TM_TYPE<Node*>  head {nullptr};
    alignas(128) TM_TYPE<Node*>  tail {nullptr};


public:
    TMLinkedListSet(unsigned int maxThreads=MAX_THREADS) : maxThreads{maxThreads} {
        Node* lhead = new Node(nullptr);
        Node* ltail = new Node(nullptr);
        head = lhead;
        head->next = ltail;
        tail = ltail;
    }


    ~TMLinkedListSet() {
        // Delete all the nodes in the list
        Node* prev = head;
        Node* node = prev->next;
        while (node != tail) {
            delete prev;
            prev = node;
            node = node->next;
        }
        delete prev;
        delete tail;
    }


    static std::string className() { return TM_NAME() + "-LinkedListSet"; }

#ifdef TINY_STM
    /*
     * Progress Condition: lock-free
     * Adds a node with a key, returns false if the key is already in the set
     */
    bool add(T* key, const int tid) {
        if (key == nullptr) throw std::invalid_argument("key can not be nullptr");
        bool retval = false;
        WRITE_TX_BEGIN
        Node* newNode = TM_ALLOC<Node>(key);
        Node* prev = head;
        Node* node = prev->next;
        while (true) {
            if (node == tail) {
                prev->next = newNode;
                newNode->next = node;
                retval = true;
                break;
            }
            if (*key == *node->key) {
                TM_FREE(newNode); // If the key was already in the set, free the node that was never used
                break;
            }
            if (*(node->key) < *key) {
                prev->next = newNode;
                newNode->next = node;
                retval = true;
                break;
            }
            prev = node;
            node = node->next;
        }
        WRITE_TX_END
        return retval;
    }


    /*
     * Progress Condition: lock-free
     * Removes a node with an key, returns false if the key is not in the set
     */
    bool remove(T* key, const int tid) {
        if (key == nullptr) throw std::invalid_argument("key can not be nullptr");
        bool retval = false;
        WRITE_TX_BEGIN
        Node* prev = head;
        Node* node = prev->next;
        while (true) {
            if (node == tail) break;
            if (*key == *node->key) {
                prev->next = node->next;
                TM_FREE(node);
                retval = true;
                break;
            }
            if (*(node->key) < *key) break;
            prev = node;
            node = node->next;
        }
        WRITE_TX_END
        return retval;
    }


    /*
     * Progress Condition: lock-free
     * Returns true if it finds a node with a matching key
     */
    bool contains(T* key, const int tid) {
        if (key == nullptr) throw std::invalid_argument("key can not be nullptr");
        bool retval = false;
        READ_TX_BEGIN
        Node* node = head->next;
        while (true) {
            if (node == tail) break;
            if (*key == *node->key) {retval = true; break; }
            if (*(node->key) < *key) break;
            node = node->next;
        }
        READ_TX_END
        return retval;
    }

#else

    /*
     * Progress Condition: lock-free
     * Adds a node with a key, returns false if the key is already in the set
     */
    bool add(T* key, const int tid) {
        if (key == nullptr) throw std::invalid_argument("key can not be nullptr");
        return TM_WRITE_TRANSACTION<bool>([this,key] () -> bool {
                Node* newNode = TM_ALLOC<Node>(key);
                Node* prev = head;
                Node* node = prev->next;
                while (true) {
                    if (node == tail) break;
                    if (*key == *node->key) {
                        TM_FREE(newNode); // If the key was already in the set, free the node that was never used
                        return false;
                    }
                    if (*(node->key) < *key) break;
                    prev = node;
                    node = node->next;
                }
                prev->next = newNode;
                newNode->next = node;
                return true;
            });
    }


    /*
     * Progress Condition: lock-free
     * Removes a node with an key, returns false if the key is not in the set
     */
    bool remove(T* key, const int tid) {
        if (key == nullptr) throw std::invalid_argument("key can not be nullptr");
        return TM_WRITE_TRANSACTION<bool>([this,key] () -> bool {
                Node* prev = head;
                Node* node = prev->next;
                while (true) {
                    if (node == tail) return false;
                    if (*key == *node->key) {
                        prev->next = node->next;
                        TM_FREE(node);
                        return true;
                    }
                    if (*(node->key) < *key) return false;
                    prev = node;
                    node = node->next;
                }
            });
    }


    /*
     * Progress Condition: lock-free
     * Returns true if it finds a node with a matching key
     */
    bool contains(T* key, const int tid) {
        if (key == nullptr) throw std::invalid_argument("key can not be nullptr");
        return TM_READ_TRANSACTION<bool>([this,key] () -> bool {
                Node* node = head->next;
                while (true) {
                    if (node == tail) return false;
                    if (*key == *node->key) return true;
                    if (*(node->key) < *key) return false;
                    node = node->next;
                }
            });
    }
#endif

    bool addAll(T** keys, int size, const int tid) {
        for (int i = 0; i < size; i++) add(keys[i], tid);
    }
};

#endif /* _TM_LINKED_LIST_SET_H_ */
