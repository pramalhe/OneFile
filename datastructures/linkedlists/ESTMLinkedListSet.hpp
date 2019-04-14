/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _ESTM_LINKED_LIST_SET_H_
#define _ESTM_LINKED_LIST_SET_H_


#include "../../stms/ESTM.hpp"               // This header defines the macros for the STM being compiled


/**
 * <h1> A Linked List Set for Elastic STM </h1>
 * When we make the 'ltail' optimization here, it causes a crash on ESTM, therefore we don't do it.
 */
template<typename T>
class ESTMLinkedListSet : public estm::tmbase {

private:
    struct Node : public estm::tmbase {
        T key {};
        estm::tmtype<Node*> next {nullptr};
        Node() {}
        Node(T key) : key{key} { }
    };

    alignas(128) estm::tmtype<Node*>  head {nullptr};
    alignas(128) estm::tmtype<Node*>  tail {nullptr};


public:
    ESTMLinkedListSet(unsigned int maxThreads=0) {
        estm::updateTx([&] () {
            Node* lhead = estm::tmNew<Node>();
            Node* ltail = estm::tmNew<Node>();
            head = lhead;
            head->next = ltail;
            tail = ltail;
        });
    }


    ~ESTMLinkedListSet() {
        estm::updateTx([&] () {
            // Delete all the nodes in the list
            Node* prev = head;
            Node* node = prev->next;
            while (node != tail) {
                estm::tmDelete(prev);
                prev = node;
                node = node->next;
            }
            estm::tmDelete(prev);
            estm::tmDelete(tail.load());
        });
    }


    static std::string className() { return estm::ESTM::className() + "-LinkedListSet"; }


    /*
     * Progress Condition: blocking
     * Adds a node with a key, returns false if the key is already in the set
     */
    bool add(T key, const int tid=0) {
        return estm::updateTx<bool>([this,key] () {
                Node* newNode = estm::tmNew<Node>(key);
                Node* prev = head;
                Node* node = prev->next;
                while (true) {
                    if (node == tail) break;
                    if (key == node->key) {
                        estm::tmDelete(newNode); // If the key was already in the set, free the node that was never used
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
        return estm::updateTx<bool>([this,key] () {
                Node* prev = head;
                Node* node = prev->next;
                while (true) {
                    if (node == tail) return false;
                    if (key == node->key) {
                        prev->next = node->next;
                        estm::tmDelete(node);
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
        return estm::readTx<bool>([this,key] () {
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
        return true;
    }
};

#endif /* _ESTM_LINKED_LIST_SET_H_ */
