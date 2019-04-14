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

#ifndef _CRWWP_STM_LINKED_LIST_SET_H_
#define _CRWWP_STM_LINKED_LIST_SET_H_

#include <atomic>
#include <stdexcept>
#include "stms/CRWWPSTM.hpp"


/**
 * <h1> A Linked List Set for CRWWP STM (blocking) </h1>
 *
 * TODO
 *
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T>
class CRWWPLinkedListSet : public crwwpstm::tmbase {

private:
    struct Node : public crwwpstm::tmbase {
        T key;
        crwwpstm::tmtype<Node*> next {nullptr};
        Node() {}
        Node(T key) : key{key} { }
    };

    alignas(128) crwwpstm::tmtype<Node*>  head {nullptr};
    alignas(128) crwwpstm::tmtype<Node*>  tail {nullptr};


public:
    CRWWPLinkedListSet(unsigned int maxThreads=0) {
        Node* lhead = new Node();
        Node* ltail = new Node();
        head = lhead;
        head->next = ltail;
        tail = ltail;
    }


    ~CRWWPLinkedListSet() {
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


    static std::string className() { return crwwpstm::CRWWPSTM::className() + "-LinkedListSet"; }


    /*
     * Progress Condition: blocking
     * Adds a node with a key, returns false if the key is already in the set
     */
    bool add(T key, const int tid=0) {
        return crwwpstm::updateTx<bool>([&] () -> bool {
                Node* newNode = crwwpstm::tmNew<Node>(key);
                Node* prev = head;
                Node* node = prev->next;
                while (true) {
                    if (node == tail) break;
                    if (key == node->key) {
                        crwwpstm::tmDelete(newNode); // If the key was already in the set, free the node that was never used
                        return false;
                    }
                    if (node->key < key) break;
                    prev = node;
                    node = node->next;
                }
                prev->next = newNode;
                newNode->next = node;
                return true;
            });
    }


    /*
     * Progress Condition: blocking
     * Removes a node with an key, returns false if the key is not in the set
     */
    bool remove(T key, const int tid=0) {
        return crwwpstm::updateTx<bool>([&] () -> bool {
                Node* prev = head;
                Node* node = prev->next;
                while (true) {
                    if (node == tail) return false;
                    if (key == node->key) {
                        prev->next = node->next;
                        crwwpstm::tmDelete(node);
                        return true;
                    }
                    if (node->key < key) return false;
                    prev = node;
                    node = node->next;
                }
            });
    }


    /*
     * Progress Condition: blocking
     * Returns true if it finds a node with a matching key
     */
    bool contains(T key, const int tid=0) {
        return crwwpstm::readTx<bool>([&] () -> bool {
                Node* node = head->next;
                while (true) {
                    if (node == tail) return false;
                    if (key == node->key) return true;
                    if (node->key < key) return false;
                    node = node->next;
                }
            });
    }


    bool addAll(T** keys, int size, const int tid) {
        for (int i = 0; i < size; i++) add(*keys[i], tid);
    }
};

#endif /* _C_RW_WP_STM_LINKED_LIST_SET_H_ */
