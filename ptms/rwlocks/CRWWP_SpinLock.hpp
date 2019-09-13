/******************************************************************************
 * Copyright (c) 2014-2017, Pedro Ramalhete, Andreia Correia
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

#ifndef _CRWWP_Spin_H_
#define _CRWWP_Spin_H_

#include <atomic>
#include <stdexcept>
#include <cstdint>
#include <thread>
#include "../../common/ThreadRegistry.hpp"


// Pause to prevent excess processor bus usage
#if defined( __sparc )
#define Pause() __asm__ __volatile__ ( "rd %ccr,%g0" )
#elif defined( __i386 ) || defined( __x86_64 )
#define Pause() __asm__ __volatile__ ( "pause" : : : )
#else
#define Pause() std::this_thread::yield();
#endif




/**
 * <h1> C-RW-WP </h1>
 *
 * A C-RW-WP reader-writer lock with writer preference and using a
 * spin Lock as Cohort.
 *
 * C-RW-WP paper:         http://dl.acm.org/citation.cfm?id=2442532
 *
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
class CRWWPSpinLock {

private:
    class SpinLock {
        alignas(128) std::atomic<int> writers {0};
    public:
        bool isLocked() { return (writers.load()==1); }
        void lock() {
            while (!tryLock()) Pause();
        }
        bool tryLock() {
            if(writers.load()==1)return false;
            int tmp = 0;
            return writers.compare_exchange_strong(tmp,1);
        }
        void unlock() {
            writers.store(0, std::memory_order_release);
        }
    };

    class RIStaticPerThread {
    private:
        static const uint64_t NOT_READING = 0;
        static const uint64_t READING = 1;
        static const int CLPAD = 128/sizeof(uint64_t);
        static const int MAX_THREADS = 128;
        const int maxThreads;
        alignas(128) std::atomic<uint64_t>* states;

    public:
        RIStaticPerThread(int maxThreads=MAX_THREADS) : maxThreads{maxThreads} {
            states = new std::atomic<uint64_t>[maxThreads*CLPAD];
            for (int tid = 0; tid < maxThreads; tid++) {
                states[tid*CLPAD].store(NOT_READING, std::memory_order_relaxed);
            }
        }

        ~RIStaticPerThread() {
            delete[] states;
        }

        inline void arrive(const int tid) noexcept {
            states[tid*CLPAD].store(READING);
        }

        inline void depart(const int tid) noexcept {
            states[tid*CLPAD].store(NOT_READING, std::memory_order_release);
        }

        inline bool isEmpty() noexcept {
            const int maxTid = ThreadRegistry::getMaxThreads();
            for (int tid = 0; tid < maxTid; tid++) {
                if (states[tid*CLPAD].load() != NOT_READING) return false;
            }
            return true;
        }
    };

    static const int MAX_THREADS = 128;
    //static const int LOCKED = 1;
    //static const int UNLOCKED = 0;
    const int maxThreads;
    RIStaticPerThread ri {};
    //alignas(128) std::atomic<int> cohort { UNLOCKED };
    SpinLock splock {};

public:
    CRWWPSpinLock(const int maxThreads=MAX_THREADS) : maxThreads{maxThreads} { }

    std::string className() { return "C-RW-WP-SpinLock"; }

    void exclusiveLock() {
        splock.lock();
        while (!ri.isEmpty()) Pause();
    }

    bool tryExclusiveLock() {
        return splock.tryLock();
    }

    void exclusiveUnlock() {
        splock.unlock();
    }

    void sharedLock(const int tid) {
        while (true) {
            ri.arrive(tid);
            if (!splock.isLocked()) break;
            ri.depart(tid);
            while (splock.isLocked()) Pause();
        }
    }

    void sharedUnlock(const int tid) {
        ri.depart(tid);
    }

    void waitForReaders(){
        while (!ri.isEmpty()) {} // spin waiting for readers
    }
};

#endif /* _CRWWP_H_ */
