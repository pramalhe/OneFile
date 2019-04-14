/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _OF_LF_RESIZABLE_HASH_MAP_H_
#define _OF_LF_RESIZABLE_HASH_MAP_H_

#include <string>

#include "stms/OneFileLF.hpp"

/**
 * <h1> A Resizable Hash Map for usage with STMs </h1>
 * TODO
 *
 */
template<typename K>
class OFLFResizableHashSet {

private:
    struct Node : public oflf::tmbase {
        oflf::tmtype<K>     key;
        oflf::tmtype<Node*> next {nullptr};
        Node(const K& k) : key{k} { } // Copy constructor for k
    };

    oflf::tmtype<uint64_t>                     capacity;
    oflf::tmtype<uint64_t>                     sizeHM = 0;
    static constexpr double                         loadFactor = 0.75;
    oflf::tmtype<oflf::tmtype<Node*>*>    buckets;      // An array of pointers to Nodes


public:
    OFLFResizableHashSet(int maxThreads=0, uint64_t capacity=4) : capacity{capacity} {
        oflf::updateTx([&] () {
            buckets = (oflf::tmtype<Node*>*)oflf::tmMalloc(capacity*sizeof(oflf::tmtype<Node*>));
            for (int i = 0; i < capacity; i++) buckets[i] = nullptr;
        });
    }


    ~OFLFResizableHashSet() {
        oflf::updateTx([&] () {
            for (int i = 0; i < capacity; i++){
                Node* node = buckets[i];
                while (node != nullptr) {
                    Node* next = node->next;
                    oflf::tmDelete(node);
                    node = next;
                }
            }
            oflf::tmFree(buckets.pload());
        });
    }


    static std::string className() { return oflf::OneFileLF::className() + "-HashMap"; }


    void rebuild() {
        uint64_t newcapacity = 2*capacity;
        oflf::tmtype<Node*>* newbuckets = (oflf::tmtype<Node*>*)oflf::tmMalloc(newcapacity*sizeof(oflf::tmtype<Node*>));
        for (int i = 0; i < newcapacity; i++) newbuckets[i] = nullptr;
        for (int i = 0; i < capacity; i++) {
            Node* node = buckets[i];
            while (node!=nullptr) {
                Node* next = node->next;
                auto h = std::hash<K>{}(node->key) % newcapacity;
                node->next = newbuckets[h];
                newbuckets[h] = node;
                node = next;
            }
        }
        oflf::tmFree(buckets.pload());
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
        if (sizeHM.pload() > capacity.pload()*loadFactor) rebuild();
        auto h = std::hash<K>{}(key) % capacity;
        Node* node = buckets[h];
        Node* prev = node;
        while (true) {
            if (node == nullptr) {
                Node* newnode = oflf::tmNew<Node>(key);
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
                oflf::tmDelete(node);
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
        return oflf::updateTx<bool>([&] () {
            return innerPut(key);
        });
    }

    // Returns true only if the key was present
    bool remove(K key, const int tid=0) {
        return oflf::updateTx<bool>([&] () {
            return innerRemove(key);
        });
    }

    bool contains(K key, const int tid=0) {
        return oflf::readTx<bool>([&] () {
            return innerGet(key);
        });
    }

    // Used only for benchmarks
    void addAll(K** keys, const int size, const int tid=0) {
        for (int i = 0; i < size; i++) add(*keys[i]);
    }
};

#endif /* _OF_LF_RESIZABLE_HASH_MAP_H_ */
