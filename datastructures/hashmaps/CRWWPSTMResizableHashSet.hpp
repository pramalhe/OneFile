/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _CRWWP_STM_RESIZABLE_HASH_MAP_H_
#define _CRWWP_STM_RESIZABLE_HASH_MAP_H_

#include <string>

#include "stms/CRWWPSTM.hpp"

/**
 * <h1> A Resizable Hash Map for usage with STMs </h1>
 * TODO
 *
 */
template<typename K>
class CRWWPSTMResizableHashSet {

private:
    struct Node : public crwwpstm::tmbase {
        crwwpstm::tmtype<K>     key;
        crwwpstm::tmtype<Node*> next {nullptr};
        Node(const K& k) : key{k} { } // Copy constructor for k
    };

    crwwpstm::tmtype<long>                         capacity;
    crwwpstm::tmtype<long>                         sizeHM = 0;
    static constexpr double                        loadFactor = 0.75;
    crwwpstm::tmtype<crwwpstm::tmtype<Node*>*>     buckets;      // An array of pointers to Nodes


public:
    CRWWPSTMResizableHashSet(int maxThreads=0, int capacity=4) : capacity{capacity} {
        crwwpstm::updateTx([&] () {
            buckets = (crwwpstm::tmtype<Node*>*)crwwpstm::tmMalloc(capacity*sizeof(crwwpstm::tmtype<Node*>));
            for (int i = 0; i < capacity; i++) buckets[i] = nullptr;
        });
    }


    ~CRWWPSTMResizableHashSet() {
        crwwpstm::updateTx([&] () {
            for(int i = 0; i < capacity; i++){
                Node* node = buckets[i];
                while (node != nullptr) {
                    Node* next = node->next;
                    crwwpstm::tmDelete(node);
                    node = next;
                }
            }
            crwwpstm::tmFree(buckets.load());
        });
    }


    static std::string className() { return crwwpstm::CRWWPSTM::className() + "-HashMap"; }


    void rebuild() {
        int newcapacity = 2*capacity;
        crwwpstm::tmtype<Node*>* newbuckets = (crwwpstm::tmtype<Node*>*)crwwpstm::tmMalloc(newcapacity*sizeof(crwwpstm::tmtype<Node*>));
        for (int i = 0; i < newcapacity; i++) newbuckets[i] = nullptr;
        for (int i = 0; i < capacity; i++) {
            Node* node = buckets[i];
            while(node!=nullptr){
                Node* next = node->next;
                auto h = std::hash<K>{}(node->key) % newcapacity;
                node->next = newbuckets[h];
                newbuckets[h] = node;
                node = next;
            }
        }
        crwwpstm::tmFree(buckets.load());
        buckets = newbuckets;
        capacity = newcapacity;
    }


    /*
     * Adds a node with a key if the key is not present, otherwise replaces the value.
     * If saveOldValue is set, it will set 'oldValue' to the previous value, iff there was already a mapping.
     *
     * Returns true if there was no mapping for the key, false if there was already a value and it was replaced.
     */
    bool innerPut(const K& key) {
        if (sizeHM > capacity*loadFactor) rebuild();
        auto h = std::hash<K>{}(key) % capacity;
        Node* node = buckets[h];
        Node* prev = node;
        while (true) {
            if (node == nullptr) {
                Node* newnode = crwwpstm::tmNew<Node>(key);
                if (node == prev) {
                    buckets[h] = newnode;
                } else {
                    prev->next = newnode;
                }
                sizeHM++;
                return true;  // New insertion
            }
            if (key == node->key) return false;
            prev = node;
            node = node->next;
        }
    }


    /*
     * Removes a key and its mapping.
     * Saves the value in 'oldvalue' if 'saveOldValue' is set.
     *
     * Returns returns true if a matching key was found
     */
    bool innerRemove(const K& key) {
        auto h = std::hash<K>{}(key) % capacity;
        Node* node = buckets[h];
        Node* prev = node;
        while (true) {
            if (node == nullptr) return false;
            if (key == node->key) {
                if (node == prev) {
                    buckets[h] = node->next;
                } else {
                    prev->next = node->next;
                }
                sizeHM--;
                crwwpstm::tmDelete(node);
                return true;
            }
            prev = node;
            node = node->next;
        }
    }


    /*
     * Returns true if key is present. Saves a copy of 'value' in 'oldValue' if 'saveOldValue' is set.
     */
    bool innerGet(const K& key) {
        auto h = std::hash<K>{}(key) % capacity;
        Node* node = buckets[h];
        while (true) {
            if (node == nullptr) return false;
            if (key == node->key) return true;
            node = node->next;
        }
    }


    //
    // Set methods for running the usual tests and benchmarks
    //

    // Inserts a key only if it's not already present
    bool add(K key, const int tid=0) {
        return crwwpstm::updateTx<bool>([&] () {
            return innerPut(key);
        });
    }

    // Returns true only if the key was present
    bool remove(K key, const int tid=0) {
        return crwwpstm::updateTx<bool>([&] () {
            return innerRemove(key);
        });
    }

    bool contains(K key, const int tid=0) {
        return crwwpstm::readTx<bool>([&] () {
            return innerGet(key);
        });
    }

    // Used only for benchmarks
    void addAll(K** keys, const int size, const int tid=0) {
        for (int i = 0; i < size; i++) add(*keys[i]);
    }
};

#endif /* _CRWWP_STM_RESIZABLE_HASH_MAP_H_ */
