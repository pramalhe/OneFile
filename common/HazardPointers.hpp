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

#ifndef _HAZARD_POINTERS_H_
#define _HAZARD_POINTERS_H_

#include <atomic>
#include <iostream>



/**
 * This is a customized version of Hazard Pointers to be used with CXMutation
 */
// TODO: use std::vector instead of arrays for the retired objects (keep the padding)
template<typename T>
class HazardPointers {

private:
    static const int      MAX_THREADS = 128;
    static const int      MAX_HPS = 5;
    static const int      MAX_RETIRED = MAX_THREADS*MAX_HPS;
    static const int      HP_THRESHOLD_R = 0;  // This is named 'R' in the HP paper
    static const int      CLPAD = 128/sizeof(std::atomic<T*>);
    const int             maxHPs;
    const int             maxThreads;
    alignas(128) std::atomic<T*>*      hp[MAX_THREADS*CLPAD];
    alignas(128) T**                   retiredObjects[MAX_THREADS*CLPAD];  // List of retired nodes that need to be 'deleted' for the current thread
    alignas(128) long                  numRetiredObjects[MAX_THREADS*CLPAD];       // Number of nodes in the retired list
    // Used specifically for CXMutation
    alignas(128) std::atomic<T*>       heads[2*MAX_THREADS*CLPAD];

public:
    HazardPointers(int maxHPs=MAX_HPS, int maxThreads=MAX_THREADS) : maxHPs{maxHPs}, maxThreads{maxThreads} {
        for (int ih = 0; ih < 2*MAX_THREADS; ih++) {
            heads[ih*CLPAD].store(nullptr, std::memory_order_relaxed);
        }
        for (int ithread = 0; ithread < MAX_THREADS; ithread++) {
        	numRetiredObjects[ithread*CLPAD] = 0;
            hp[ithread*CLPAD] = new std::atomic<T*>[MAX_HPS];
            for (int ihp = 0; ihp < MAX_HPS; ihp++) {
                hp[ithread*CLPAD][ihp].store(nullptr, std::memory_order_relaxed);
            }
            retiredObjects[ithread*CLPAD] = new T*[MAX_RETIRED];
            for (int iret = 0; iret < MAX_RETIRED; iret++) {
                retiredObjects[ithread*CLPAD][iret] = nullptr;
            }
        }
    }

    ~HazardPointers() {
        for (int ithread = 0; ithread < MAX_THREADS; ithread++) {
            // Clear the current retired nodes
            for (int iret = 0; iret < numRetiredObjects[ithread*CLPAD]; iret++) {
                delete (T*)retiredObjects[ithread*CLPAD][iret];
            }
            delete[] hp[ithread*CLPAD];
            delete[] retiredObjects[ithread*CLPAD];
        }
    }


    /**
     * Progress Condition: wait-free bounded (by maxHPs)
     *
     * It's ok to use relaxed loads here because:
     * - For progress: we know that the store will eventually become visible,
     *   or another publish() will take its place;
     * - For correctness: it can be re-ordered below, but at most it will protect
     *   an object for longer than required, i.e. until the next publish overwrites it.
     *   Or it gets re-ordered above, but only up to a seq-cst store on the same
     *   variable in publish(), which _must_ be it, even if the store in the publish
     *   is a release store (which is the case for publishRelease()).
     */
    void clear(const int tid) {
        for (int ihp = 0; ihp < maxHPs; ihp++) {
            hp[tid*CLPAD][ihp].store(nullptr, std::memory_order_relaxed);
        }
    }


    /**
     * Progress Condition: wait-free population oblivious
     */
    void clearOne(int ihp, const int tid) {
        hp[tid*CLPAD][ihp].store(nullptr,std::memory_order_relaxed);
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

    inline T* get_protected(int index, const std::atomic<T*>& atom, const int tid) {
        return protect(index, atom, tid);
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
     * This assumes that the ptr lhead is already protected by a "regular" hazard pointers
     */
    void protectHead(int combinedIndex, T* lhead) {
        heads[combinedIndex*CLPAD].store(lhead, std::memory_order_release);
    }

    std::atomic<T*>* getHeads() {
        return heads;
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
        if (numRetiredObjects[tid*CLPAD] >= HP_THRESHOLD_R) scanAndDelete(tid);
        retiredObjects[tid*CLPAD][numRetiredObjects[tid*CLPAD]++] = ptr;
    }


    void copyPtr(int index, int other, const int tid) {
        auto ptr = hp[tid*CLPAD][other].load(std::memory_order_relaxed);
        hp[tid*CLPAD][index].store(ptr, std::memory_order_release);
    }


private:
    void scanAndDelete(const int tid) {
        for (int iret = 0; iret < numRetiredObjects[tid*CLPAD]; ) {
            bool ptrInUse = false;
            auto ptr = (T*)retiredObjects[tid*CLPAD][iret];
            for (int it = 0; it < maxThreads; it++) {
                for (int ihp = maxHPs-1; ihp >= 0; ihp--) {
                    if (ptr == hp[it*CLPAD][ihp].load()) ptrInUse = true;
                }
            }
            if (ptrInUse) { iret++; continue; }
            // Scan the array of heads before deleting the pointer
            for (int icomb = 0; icomb < 2*MAX_THREADS; icomb++) {
                if (ptr == heads[icomb*CLPAD].load()) ptrInUse = true;
            }
            if (ptrInUse) { iret++; continue;  }
            for (int i = iret; i < numRetiredObjects[tid*CLPAD]-1; i++) retiredObjects[tid*CLPAD][i] = retiredObjects[tid*CLPAD][i+1];
            numRetiredObjects[tid*CLPAD]--;
            delete ptr;

        }
    }
};

#endif /* _HAZARD_POINTERS_H_ */
