/******************************************************************************
 * Copyright (c) 2014-2016, Pedro Ramalhete, Andreia Correia
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

#ifndef _HAZARD_POINTERS_SIM_QUEUE_H_
#define _HAZARD_POINTERS_SIM_QUEUE_H_

#include <atomic>
#include <iostream>
#include <functional>
#include <vector>


/*
 * The main difference from this implementation to regular Hazard Pointers is
 * that the constructor takes a function pointer to function 'find' which
 * acts as a callback, returning true if the pointer is still stored somewhere
 * in the data structure. This is used by SimQueue to indicate if there is a
 * pointer to the object we're trying to de-allocate in the array of enqReused.
 */
template<typename T>
class HazardPointersSimQueue {

private:
    static const int      HP_MAX_THREADS = 128;
    static const int      HP_MAX_HPS = 11;     // This is named 'K' in the HP paper
    static const int      CLPAD = 128/sizeof(std::atomic<T*>);
    static const int      HP_THRESHOLD_R = 0; // This is named 'R' in the HP paper
    static const int      MAX_RETIRED = HP_MAX_THREADS*HP_MAX_HPS; // Maximum number of retired objects per thread

    const int             maxHPs;
    const int             maxThreads;

    std::atomic<T*>       hp[HP_MAX_THREADS*CLPAD][HP_MAX_HPS];
    // It's not nice that we have a lot of empty vectors, but we need padding to avoid false sharing
    std::vector<T*>       retiredList[HP_MAX_THREADS*CLPAD];

    std::function<bool(T*)> findPtr;

public:
    HazardPointersSimQueue(std::function<bool(T*)>& find, int maxHPs=HP_MAX_HPS, int maxThreads=HP_MAX_THREADS) : maxHPs{maxHPs}, maxThreads{maxThreads} {
        findPtr = find;
        for (int ithread = 0; ithread < HP_MAX_THREADS; ithread++) {
            for (int ihp = 0; ihp < HP_MAX_HPS; ihp++) {
                hp[ithread*CLPAD][ihp].store(nullptr, std::memory_order_relaxed);
            }
        }
    }

    ~HazardPointersSimQueue() {
        for (int ithread = 0; ithread < HP_MAX_THREADS; ithread++) {
            // Clear the current retired nodes
            for (unsigned iret = 0; iret < retiredList[ithread*CLPAD].size(); iret++) {
                delete retiredList[ithread*CLPAD][iret];
            }
        }
    }


    /**
     * Progress Condition: wait-free bounded (by maxHPs)
     */
    void clear(const int tid) {
        for (int ihp = 0; ihp < maxHPs; ihp++) {
            hp[tid*CLPAD][ihp].store(nullptr, std::memory_order_release);
        }
    }


    /**
     * Progress Condition: wait-free population oblivious
     */
    void clearOne(int ihp, const int tid) {
        hp[tid*CLPAD][ihp].store(nullptr, std::memory_order_release);
    }


    /**
     * Progress Condition: lock-free
     */
    T* protect(int index, const std::atomic<T*>& atom, const int tid) {
        T* n = nullptr;
        T* ret;
		while ((ret = atom.load()) != n) {
			hp[tid*CLPAD][index].store(ret);
			n = ret;
		}
		return ret;
    }

    /**
     * This returns the same value that is passed as ptr, which is sometimes useful
     * Progress Condition: wait-free population oblivious
     */
    T* protectPtr(int index, T* ptr, const int tid) {
        hp[tid*CLPAD][index].store(ptr);
        return ptr;
    }


    /**
     * This returns the same value that is passed as ptr, which is sometimes useful
     * Progress Condition: wait-free population oblivious
     */
    T* protectRelease(int index, T* ptr, const int tid) {
        hp[tid*CLPAD][index].store(ptr, std::memory_order_release);
        return ptr;
    }


    /**
     * This returns the same value that is passed as ptr, which is sometimes useful
     * Progress Condition: wait-free bounded (by the number of threads squared)
     */
    void retire(T* ptr, const int tid) {
        retiredList[tid*CLPAD].push_back(ptr);
        for (unsigned iret = 0; iret < retiredList[tid*CLPAD].size();) {
            auto obj = retiredList[tid*CLPAD][iret];
            if (findPtr(obj)) {
                iret++;
                continue;
            }
            bool canDelete = true;
            for (int tid = 0; tid < maxThreads && canDelete; tid++) {
                for (int ihp = maxHPs-1; ihp >= 0; ihp--) {
                    if (hp[tid*CLPAD][ihp].load() == obj) {
                        canDelete = false;
                        break;
                    }
                }
            }
            if (canDelete) {
                retiredList[tid*CLPAD].erase(retiredList[tid*CLPAD].begin() + iret);
                delete obj;
                continue;
            }
            iret++;
        }
    }
};

#endif /* _HAZARD_POINTERS_H_ */
