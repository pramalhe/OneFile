/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _BENCHMARK_MAPS_H_
#define _BENCHMARK_MAPS_H_

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

using namespace std;
using namespace chrono;

// Regular UserData
struct UserData  {
    long long seq;
    int tid;
    UserData(long long lseq, int ltid=0) {
        this->seq = lseq;
        this->tid = ltid;
    }
    UserData() {
        this->seq = -2;
        this->tid = -2;
    }
    UserData(const UserData &other) : seq(other.seq), tid(other.tid) { }

    bool operator < (const UserData& other) const {
        return seq < other.seq;
    }
    bool operator == (const UserData& other) const {
        return seq == other.seq && tid == other.tid;
    }
    bool operator != (const UserData& other) const {
        return seq != other.seq || tid != other.tid;
    }
};


namespace std {
    template <>
    struct hash<UserData> {
        std::size_t operator()(const UserData& k) const {
            using std::size_t;
            using std::hash;
            return (hash<long long>()(k.seq));  // This hash has no collisions, which is irealistic
        }
    };
}


/**
 * This is a micro-benchmark of sets, used in the CX paper
 */
class BenchmarkMaps {

private:
    struct Result {
        nanoseconds nsEnq = 0ns;
        nanoseconds nsDeq = 0ns;
        long long numEnq = 0;
        long long numDeq = 0;
        long long totOpsSec = 0;

        Result() { }

        Result(const Result &other) {
            nsEnq = other.nsEnq;
            nsDeq = other.nsDeq;
            numEnq = other.numEnq;
            numDeq = other.numDeq;
            totOpsSec = other.totOpsSec;
        }

        bool operator < (const Result& other) const {
            return totOpsSec < other.totOpsSec;
        }
    };

    static const long long NSEC_IN_SEC = 1000000000LL;

    int numThreads;

public:
    BenchmarkMaps(int numThreads) {
        this->numThreads = numThreads;
    }


    /**
     * When doing "updates" we execute a random removal and if the removal is successful we do an add() of the
     * same item immediately after. This keeps the size of the data structure equal to the original size (minus
     * MAX_THREADS items at most) which gives more deterministic results.
     */
    template<template<typename,typename> class S, typename K, typename V>
    long long benchmark(const int updateRatio, const seconds testLengthSeconds, const int numRuns, const int numElements, const bool dedicated=false) {
        long long ops[numThreads][numRuns];
        long long lengthSec[numRuns];
        atomic<bool> quit = { false };
        atomic<bool> startFlag = { false };
        S<K,V>* set = nullptr;
#ifdef TINY_STM
        stm_init_thread();
        //const int tid = 0;
        //WRITE_TX_BEGIN
        //set = TM_ALLOC<S>();
        //WRITE_TX_END
#endif

        // Create all the keys and values in the concurrent set
        K** keyarray = new K*[numElements];
        for (int i = 0; i < numElements; i++) keyarray[i] = new K(i);
        V** valarray = new V*[numElements];
        for (int i = 0; i < numElements; i++) valarray[i] = new V(i);

        // Can either be a Reader or a Writer
        auto rw_lambda = [&](const int updateRatio, long long *ops, const int tid) {
        	uint64_t accum = 0;
            long long numOps = 0;
#ifdef TINY_STM
            stm_init_thread();
#endif
            while (!startFlag.load()) ; // spin
            uint64_t seed = tid+1234567890123456781ULL;
            while (!quit.load()) {
                seed = randomLong(seed);
                int update = seed%1000;
                seed = randomLong(seed);
                auto ix = (unsigned int)(seed%numElements);
                if (update < updateRatio) {
                    // I'm a Writer
                    if (set->remove(*keyarray[ix])) {
                    	numOps++;
                    	set->put(*keyarray[ix], *valarray[ix]);
                    }
                    numOps++;
                } else {
                	// I'm a Reader
                    set->get(*keyarray[ix]);
                    seed = randomLong(seed);
                    ix = (unsigned int)(seed%numElements);
                    set->get(*keyarray[ix]);
                    numOps+=2;
                }

            }
            *ops = numOps;
#ifdef TINY_STM
            stm_exit_thread();
#endif
        };

        for (int irun = 0; irun < numRuns; irun++) {
            set = new S<K,V>();
            // Add all the items to the list
            set->addAll(keyarray, valarray, numElements);
            if (irun == 0) std::cout << "##### " << set->className() << " #####  \n";
            thread rwThreads[numThreads];
            if (dedicated) {
                rwThreads[0] = thread(rw_lambda, 1000, &ops[0][irun], 0);
                rwThreads[1] = thread(rw_lambda, 1000, &ops[1][irun], 1);
                for (int tid = 2; tid < numThreads; tid++) rwThreads[tid] = thread(rw_lambda, updateRatio, &ops[tid][irun], tid);
            } else {
                for (int tid = 0; tid < numThreads; tid++) rwThreads[tid] = thread(rw_lambda, updateRatio, &ops[tid][irun], tid);
            }
            this_thread::sleep_for(100ms);
            auto startBeats = steady_clock::now();
            startFlag.store(true);
            // Sleep for testLengthSeconds seconds
            this_thread::sleep_for(testLengthSeconds);
            quit.store(true);
            auto stopBeats = steady_clock::now();
            for (int tid = 0; tid < numThreads; tid++) rwThreads[tid].join();
            lengthSec[irun] = (stopBeats-startBeats).count();
            if (dedicated) {
                // We don't account for the write-only operations but we aggregate the values from the two threads and display them
                std::cout << "Mutative transactions per second = " << (ops[0][irun] + ops[1][irun])*1000000000LL/lengthSec[irun] << "\n";
                ops[0][irun] = 0;
                ops[1][irun] = 0;
            }
            quit.store(false);
            startFlag.store(false);
            // Measure the time the destructor takes to complete and if it's more than 1 second, print it out
            auto startDel = steady_clock::now();
#ifdef TINY_STM
            WRITE_TX_BEGIN
            TM_FREE<S>(set);
            WRITE_TX_END
#endif
            delete set;

            auto stopDel = steady_clock::now();
            if ((startDel-stopDel).count() > NSEC_IN_SEC) {
                std::cout << "Destructor took " << (startDel-stopDel).count()/NSEC_IN_SEC << " seconds\n";
            }
            // Compute ops at the end of each run
            long long agg = 0;
            for (int tid = 0; tid < numThreads; tid++) {
                agg += ops[tid][irun]*1000000000LL/lengthSec[irun];
            }
        }

        for (int i = 0; i < numElements; i++) delete keyarray[i];
        delete[] keyarray;

        // Accounting
        vector<long long> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun] += ops[tid][irun]*1000000000LL/lengthSec[irun];
            }
        }

        // Compute the median. numRuns must be an odd number
        sort(agg.begin(),agg.end());
        auto maxops = agg[numRuns-1];
        auto minops = agg[0];
        auto medianops = agg[numRuns/2];
        auto delta = (long)(100.*(maxops-minops) / ((double)medianops));
        // Printed value is the median of the number of ops per second that all threads were able to accomplish (on average)
        std::cout << "Ops/sec = " << medianops << "      delta = " << delta << "%   min = " << minops << "   max = " << maxops << "\n";
#ifdef TINY_STM
        stm_exit_thread();
#endif
        return medianops;
    }


    /**
     * An imprecise but fast random number generator
     */
    uint64_t randomLong(uint64_t x) {
        x ^= x >> 12; // a
        x ^= x << 25; // b
        x ^= x >> 27; // c
        return x * 2685821657736338717LL;
    }

};

#endif
