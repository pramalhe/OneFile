/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef HASHTABLE_H
#define	HASHTABLE_H

#include <cassert>
#include <cstdlib>
#include <iostream>
#include "plaf.h"
using namespace std;

namespace hashset_namespace {
// note: TABLE_SIZE must be a power of two for bitwise operations below to work
#define TABLE_SIZE 32
#define FIRST_INDEX(key) (hash((key)) & (TABLE_SIZE-1))
#define NEXT_INDEX(ix) ((ix)+1 % TABLE_SIZE)
#define EMPTY_CELL 0
    template<typename K>
    class hashset {
        private:
            bool cleared;
            K* keys[TABLE_SIZE];
            inline int hash(K * const key) {
                // MurmurHash3's integer finalizer
                long long k = (long long) key;
                k ^= k >> 33;
                k *= 0xff51afd7ed558ccd;
                k ^= k >> 33;
                k *= 0xc4ceb9fe1a85ec53;
                k ^= k >> 33;
                return k;
            }
            int getIndex(K * const key) {
                int ix;
                for (ix=FIRST_INDEX(key)
                        ; keys[ix] != EMPTY_CELL && keys[ix] != key
                        ; ix=NEXT_INDEX(ix)) {
                    assert(ix >= 0);
                    assert(ix < TABLE_SIZE);
                }
                assert(ix >= 0);
                assert(ix < TABLE_SIZE);
                return ix;
            }
        public:
            hashset() {
                VERBOSE DEBUG std::cout<<"constructor hashset"<<std::endl;
                cleared = false;
                clear();
            }
            ~hashset() {
                VERBOSE DEBUG std::cout<<"destructor hashset"<<std::endl;
            }
            void clear() {
                if (!cleared) {
                    memset(keys, EMPTY_CELL, TABLE_SIZE*sizeof(K*));
                    cleared = true;
                }
            }
            bool contains(K * const key) {
                return get(key) != EMPTY_CELL;
            }
            K* get(K * const key) {
                return keys[getIndex(key)];
            }
            void insert(K * const key) {
                int ix = getIndex(key);
                keys[ix] = key;
            }
            void erase(K * const key) {
                int ix = getIndex(key);
                // no need for an if statement, because keys[ix] is either key or null.
                keys[ix] = EMPTY_CELL;
            }
    } __attribute__ ((aligned(PREFETCH_SIZE_BYTES)));
    
//    // hash set that allows multiple readers and ONE updater.
//    // i am pretty certain this is NOT linearizable.
//    // note: to use this with reclaim_hazardptr_hash, this would need to
//    //       be a MULTISET!!!! this is because protectObject is called multiple times,
//    //       and a single unprotectObject must not unprotect the object!!!
//    template<typename K>
//    class AtomicHashSet {
//        private:
//            int size;       // NOT ATOMICALLY ACCESSIBLE BY OTHER THREADS THAN OWNER
//            bool cleared;   // NOT ATOMICALLY ACCESSIBLE BY OTHER THREADS THAN OWNER
//            atomic_uintptr_t keys[TABLE_SIZE];
//            inline int hash(K * const key) {
//                // MurmurHash3's integer finalizer
//                long long k = (long long) key;
//                k ^= k >> 33;
//                k *= 0xff51afd7ed558ccd;
//                k ^= k >> 33;
//                k *= 0xc4ceb9fe1a85ec53;
//                k ^= k >> 33;
//                return k;
//            }
//            int getIndex(K * const key) {
//                int ix;
//                for (ix=FIRST_INDEX(key)
//                        ; keys[ix] != EMPTY_CELL && keys[ix] != key
//                        ; ix=NEXT_INDEX(ix)) {
//                    assert(ix >= 0);
//                    assert(ix < TABLE_SIZE);
//                }
//                assert(ix >= 0);
//                assert(ix < TABLE_SIZE);
//                return ix;
//            }
//        public:
//            AtomicHashSet() {
//                VERBOSE DEBUG std::cout<<"constructor AtomicHashSet"<<std::endl;
//                cleared = false;
//                clear();
//            }
//            ~AtomicHashSet() {
//                VERBOSE DEBUG std::cout<<"destructor AtomicHashSet"<<std::endl;
//            }
//            void clear() {
//                if (!cleared) {
//                    memset(keys, EMPTY_CELL, TABLE_SIZE*sizeof(K*));
//                    cleared = true;
//                }
//            }
//            bool contains(K * const key) {
//                return get(key) != EMPTY_CELL;
//            }
//            K* get(K * const key) {
//                return (K*) keys[getIndex(key)].load();
//            }
//            void insert(K * const key) {
//                int ix = getIndex(key);
//                keys[ix].store(key);
//                ++size;
//            }
//            void erase(K * const key) {
//                int ix = getIndex(key);
//                // no need for an if statement, because keys[ix] is either key or null.
//                if (keys[ix] == EMPTY_CELL) {
//                    keys[ix].store(EMPTY_CELL);
//                    --size;
//                }
//            }
//    } __attribute__ ((aligned(BYTES_IN_CACHE_LINE)));

    template<typename K>
    class hashset_new {
    private:
        int tableSize;
        K** keys;
        int __size;
        inline long hash(K * const key) {
            // MurmurHash3's integer finalizer
            long long k = (long long) key;
            k ^= k >> 33;
            k *= 0xff51afd7ed558ccd;
            k ^= k >> 33;
            k *= 0xc4ceb9fe1a85ec53;
            k ^= k >> 33;
            return k;
        }
        inline int getIndex(K * const key) {
            int ix = firstIndex(key);
            assert(ix >= 0);
            assert(ix < tableSize);
            while (true) {
                if (keys[ix] == EMPTY_CELL || keys[ix] == key) {
                    return ix;
                }
                ix = nextIndex(ix);
                assert(ix >= 0);
                assert(ix < tableSize);
            }
        }
        inline int firstIndex(K * const key) {
            return (hash(key) & (tableSize-1));
        }
        inline int nextIndex(const int ix) {
            return ((ix+1) & (tableSize-1));
        }
    public:
        hashset_new(const int numberOfElements) {
            tableSize = 32;
            while (tableSize < numberOfElements*2) {
                tableSize *= 2;
            }
            VERBOSE DEBUG std::cout<<"constructor hashset_new capacity="<<tableSize<<std::endl;
            keys = new K*[tableSize];
            __size = -1;
            clear();
        }
        ~hashset_new() {
//            VERBOSE DEBUG std::cout<<"destructor hashset_new"<<std::endl;
            delete[] keys;
        }
        void clear() {
            if (__size) {
                memset(keys, EMPTY_CELL, tableSize*sizeof(K*));
                __size = 0;
            }
        }
        bool contains(K * const key) {
            return get(key) != EMPTY_CELL;
        }
        K* get(K * const key) {
            return keys[getIndex(key)];
        }
        void insert(K * const key) {
            int ix = getIndex(key);
            if (keys[ix] == EMPTY_CELL) {
                keys[ix] = key;
                ++__size;
                assert(__size < tableSize);
            }
        }
        int size() {
            return __size;
        }
    };

}

#endif	/* HASHTABLE_H */

