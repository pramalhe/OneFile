 /*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _ONEFILE_WF_LINKED_LIST_SET_H_
#define _ONEFILE_WF_LINKED_LIST_SET_H_

#include <atomic>
#include <stdexcept>
#include "stms/OneFileWF.hpp"


/**
 * <h1> A Linked List Set for One-File STM (wait-Free) </h1>
 */
template<typename T>
class OFWFLinkedListSet : public ofwf::tmbase {

private:
    struct Node : public ofwf::tmbase {
        T key {};
        ofwf::tmtype<Node*> next {nullptr};
        Node(T key) : key{key} { }
        Node() {}
    };

    alignas(128) ofwf::tmtype<Node*>  head {nullptr};
    alignas(128) ofwf::tmtype<Node*>  tail {nullptr};


public:
    OFWFLinkedListSet(unsigned int maxThreads=0) {
        ofwf::updateTx([this] () {
            Node* lhead = ofwf::tmNew<Node>();
            Node* ltail = ofwf::tmNew<Node>();
            head = lhead;
            head->next = ltail;
            tail = ltail;
        });
    }


    ~OFWFLinkedListSet() {
        ofwf::updateTx([this] () {
            // Delete all the nodes in the list
            Node* prev = head;
            Node* node = prev->next;
            while (node != tail) {
                ofwf::tmDelete(prev);
                prev = node;
                node = node->next;
            }
            ofwf::tmDelete(prev);
            ofwf::tmDelete(tail.pload());
        });
    }


    static std::string className() { return ofwf::OneFileWF::className() + "-LinkedListSet"; }


    /*
     * Progress Condition: wait-free
     * Adds a node with a key, returns false if the key is already in the set
     */
    bool add(T key, const int tid=0) {
        return ofwf::updateTx<bool>([this,key] () {
            Node* newNode = ofwf::tmNew<Node>(key);
            Node* prev = head;
            Node* node = prev->next;
            Node* ltail = tail;
            while (true) {
                if (node == ltail) break;
                T nkey = node->key;
                if (key == nkey) {
                    ofwf::tmDelete(newNode); // If the key was already in the set, free the node that was never used
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
     * Progress Condition: wait-free
     * Removes a node with an key, returns false if the key is not in the set
     */
    bool remove(T key, const int tid=0) {
        return ofwf::updateTx<bool>([this,key] () {
            Node* prev = head;
            Node* node = prev->next;
            Node* ltail = tail;
            while (true) {
                if (node == ltail) return false;
                T nkey = node->key;
                if (key == nkey) {
                    prev->next = node->next;
                    ofwf::tmDelete(node);
                    return true;
                }
                if (nkey < key) return false;
                prev = node;
                node = node->next;
            }
        });
    }


    /*
     * Progress Condition: wait-free
     * Returns true if it finds a node with a matching key
     */
    bool contains(T key, const int tid=0) {
        return ofwf::readTx<bool>([this,key] () {
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

#endif /* _ONE_FILE_WF_LINKED_LIST_SET_H_ */
