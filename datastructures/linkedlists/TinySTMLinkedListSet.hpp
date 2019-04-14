/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _TINY_STM_LINKED_LIST_SET_H_
#define _TINY_STM_LINKED_LIST_SET_H_

#include "stms/TinySTM.hpp"


/**
 * <h1> A Linked List Set for usage with TinySTM </h1>
 */
template<typename T>
class TinySTMLinkedListSet : public tinystm::tmbase {

private:
    struct Node : public tinystm::tmbase {
        T key;
        tinystm::tmtype<Node*> next{nullptr};
        Node() {}
        Node(T key) : key{key} { }
    };

    alignas(128) tinystm::tmtype<Node*>  head {nullptr};
    alignas(128) tinystm::tmtype<Node*>  tail {nullptr};


public:
    TinySTMLinkedListSet(unsigned int maxThreads=0) {
        tinystm::updateTx<bool>([this] () {
            Node* lhead = tinystm::tmNew<Node>();
            Node* ltail = tinystm::tmNew<Node>();
            head = lhead;
            head->next = ltail;
            tail = ltail;
            return true;
        });
    }


    ~TinySTMLinkedListSet() {
        tinystm::updateTx<bool>([this] () {
            // Delete all the nodes in the list
            Node* prev = head;
            Node* node = prev->next;
            while (node != tail) {
                tinystm::tmDelete(prev);
                prev = node;
                node = node->next;
            }
            tinystm::tmDelete(prev);
            tinystm::tmDelete(tail.load());
            return true;
        });
    }


    static std::string className() { return "TinySTM-LinkedListSet"; }


    /*
     * Progress Condition: blocking
     * Adds a node with a key, returns false if the key is already in the set
     */
    bool add(T key, const int tid=0) {
        return tinystm::updateTx<bool>([this,key] () {
                Node* newNode = tinystm::tmNew<Node>(key);
                Node* prev = head;
                Node* node = prev->next;
                Node* ltail = tail;
                while (true) {
                    if (node == ltail) break;
                    T nkey = node->key;
                    if (key == nkey) {
                        tinystm::tmDelete(newNode); // If the key was already in the set, free the node that was never used
                        return false;
                    }
                    if (nkey < key) break;
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
        return tinystm::updateTx<bool>([this,key] () {
                Node* prev = head;
                Node* node = prev->next;
                Node* ltail = tail;
                while (true) {
                    if (node == ltail) return false;
                    T nkey = node->key;
                    if (key == nkey) {
                        prev->next = node->next;
                        tinystm::tmDelete(node);
                        return true;
                    }
                    if (nkey < key) return false;
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
        return tinystm::readTx<bool>([this,key] () {
                Node* node = head->next;
                Node* ltail = tail;
                while (true) {
                    if (node == ltail) return false;
                    T nkey = node->key;
                    if (key == nkey) return true;
                    if (nkey < key) return false;
                    node = node->next;
                }
            });
    }


    bool addAll(T** keys, int size, const int tid) {
        for (int i = 0; i < size; i++) add(*keys[i], tid);
    }
};

#endif /* _TINY_STM_LINKED_LIST_SET_H_ */
