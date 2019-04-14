/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _ONEFILE_LF_LINKED_LIST_SET_H_
#define _ONEFILE_LF_LINKED_LIST_SET_H_

#include <atomic>
#include <stdexcept>
#include "stms/OneFileLF.hpp"


/**
 * <h1> A Linked List Set for One-File STM (Lock-Free) </h1>
 */
template<typename T>
class OFLFLinkedListSet : public oflf::tmbase {

private:
    struct Node : public oflf::tmbase {
        T key {};
        oflf::tmtype<Node*> next {nullptr};
        Node() {}
        Node(T key) : key{key} { }
    };

    alignas(128) oflf::tmtype<Node*>  head {nullptr};
    alignas(128) oflf::tmtype<Node*>  tail {nullptr};


public:
    OFLFLinkedListSet(unsigned int maxThreads=0) {
        oflf::updateTx([this] () {
            Node* lhead = oflf::tmNew<Node>();
            Node* ltail = oflf::tmNew<Node>();
            head = lhead;
            head->next = ltail;
            tail = ltail;
        });
    }


    ~OFLFLinkedListSet() {
        oflf::updateTx([this] () {
            // Delete all the nodes in the list
            Node* prev = head;
            Node* node = prev->next;
            while (node != tail) {
                oflf::tmDelete(prev);
                prev = node;
                node = node->next;
            }
            oflf::tmDelete(prev);
            oflf::tmDelete(tail.pload());
        });
    }


    static std::string className() { return oflf::OneFileLF::className() + "-LinkedListSet"; }


    /*
     * Progress Condition: lock-free
     * Adds a node with a key, returns false if the key is already in the set
     */
    bool add(T key, const int tid=0) {
        return oflf::updateTx<bool>([this,key] () -> bool {
            Node* newNode = oflf::tmNew<Node>(key);
            Node* prev = head;
            Node* node = prev->next;
            Node* ltail = tail;
            while (true) {
                if (node == ltail) break;
                T nkey = node->key;
                if (key == nkey) {
                    oflf::tmDelete(newNode); // If the key was already in the set, free the node that was never used
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
     * Progress Condition: lock-free
     * Removes a node with an key, returns false if the key is not in the set
     */
    bool remove(T key, const int tid=0) {
        return oflf::updateTx<bool>([this,key] () -> bool {
            Node* prev = head;
            Node* node = prev->next;
            Node* ltail = tail;
            while (true) {
                if (node == ltail) return false;
                T nkey = node->key;
                if (key == nkey) {
                    prev->next = node->next;
                    oflf::tmDelete(node);
                    return true;
                }
                if (nkey < key) return false;
                prev = node;
                node = node->next;
            }
        });
    }


    /*
     * Progress Condition: lock-free
     * Returns true if it finds a node with a matching key
     */
    bool contains(T key, const int tid=0) {
        return oflf::readTx<bool>([this,key] () -> bool {
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
        return true;
    }
};

#endif /* _ONE_FILE_LF_LINKED_LIST_SET_H_ */
