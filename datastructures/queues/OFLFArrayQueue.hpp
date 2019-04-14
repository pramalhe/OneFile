/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _OFLF_STM_ARRAY_QUEUE_H_
#define _OFLF_STM_ARRAY_QUEUE_H_

#include <atomic>
#include <stdexcept>

#include "stms/OneFileLF.hpp"

/**
 * <h1> An Array Queue </h1>
 *
 */
template<typename T>
class OFLFArrayQueue : public oflf::tmbase {

private:
    static const int MAX_ITEMS = 2048;
    oflf::tmtype<uint64_t> headidx {0};
    oflf::tmtype<T*>       items[MAX_ITEMS];
    oflf::tmtype<uint64_t> tailidx {0};


public:
    OFLFArrayQueue(unsigned int maxThreads=0) {
        oflf::updateTx<bool>([this] () {
            for (int i = 0; i < MAX_ITEMS; i++) items[i] = nullptr;
            return true;
        });
    }


    ~OFLFArrayQueue() { }


    static std::string className() { return "OF-LF-ArrayQueue"; }


    /*
     * Progress Condition: blocking
     * Always returns true
     */
    bool enqueue(T* item, const int tid=0) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        return oflf::updateTx<bool>([this,item] () -> bool {
            if (tailidx >= headidx+MAX_ITEMS) return false; // queue is full
            items[tailidx % MAX_ITEMS] = item;
            ++tailidx;
            return true;
        });
    }


    /*
     * Progress Condition: blocking
     */
    T* dequeue(const int tid=0) {
        return oflf::updateTx<T*>([this] () -> T* {
            if (tailidx == headidx) return nullptr; // queue is empty
            T* item = items[headidx % MAX_ITEMS];
            ++headidx;
            return item;
        });
    }
};

#endif /* _OF_LF_STM_ARRAY_QUEUE_H_ */
