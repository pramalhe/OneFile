/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef LOCKFREESTACK_H
#define	LOCKFREESTACK_H

#include <atomic>
#include <iostream>
#include "blockbag.h"
using namespace std;

#ifndef VERBOSE
#define VERBOSE if(0)
#endif

// lock free bag that operates on elements of the block<T> type,
// defined in blockbag.h. this class does NOT allocate or deallocate any memory.
// instead, it simply chains blocks together using their next pointers.
// the implementation is a stack, with push and pop at the head.
// the aba problem is avoided using version numbers with a double-wide CAS.
// any contention issues with using a simple stack and overhead issues with
// double-wide CAS are unimportant, because operations on this bag only happen
// once a process has filled up two blocks of objects and needs to hand one
// off. thus, the number of operations on this class is several orders of
// magnitude smaller than the number of operations on the binary search tree.
template <typename T>
class lockfreeblockbag {
private:
    struct tagged_ptr {
        block<T> *ptr;
        long tag;
    };
    std::atomic<tagged_ptr> head;
public:
    lockfreeblockbag() {
        VERBOSE DEBUG std::cout<<"constructor lockfreeblockbag lockfree="<<head.is_lock_free()<<std::endl;
        assert(head.is_lock_free());
        head.store(tagged_ptr({NULL,0}));
    }
    ~lockfreeblockbag() {
        VERBOSE DEBUG std::cout<<"destructor lockfreeblockbag; ";
        block<T> *curr = head.load(memory_order_relaxed).ptr;
        int debugFreed = 0;
        while (curr) {
            block<T> * const temp = curr;
            curr = curr->next;
            //DEBUG ++debugFreed;
            delete temp;
        }
        VERBOSE DEBUG std::cout<<"freed "<<debugFreed<<std::endl;
    }
    block<T>* getBlock() {
        while (true) {
            tagged_ptr expHead = head.load(memory_order_relaxed);
            if (expHead.ptr != NULL) {
                if (head.compare_exchange_weak(
                        expHead,
                        tagged_ptr({expHead.ptr->next, expHead.tag+1}))) {
                    block<T> *result = expHead.ptr;
                    result->next = NULL;
                    return result;
                }
            } else {
                return NULL;
            }
        }
    }
    void addBlock(block<T> *b) {
        while (true) {
            tagged_ptr expHead = head.load(memory_order_relaxed);
            b->next = expHead.ptr;
            if (head.compare_exchange_weak(
                    expHead,
                    tagged_ptr({b, expHead.tag+1}))) {
                return;
            }
        }
    }
    // NOT thread safe
    int sizeInBlocks() {
        int result = 0;
        block<T> *curr = head.load(memory_order_relaxed).ptr;
        while (curr) {
            ++result;
            curr = curr->next;
        }
        return result;
    }
    // thread safe, but concurrent operations are very likely to starve it
    long long size() {
        while (1) {
            long long result = 0;
            block<T> *originalHead = head.load(memory_order_relaxed).ptr;
            block<T> *curr = originalHead;
            while (curr) {
                result += curr->computeSize();
                curr = curr->next;
            }
            if (head.load(memory_order_relaxed).ptr == originalHead) {
                return result;
            }
        }
    }
};

#endif	/* LOCKFREESTACK_H */

