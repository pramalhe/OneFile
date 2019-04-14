/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _BENCHMARK_SPS_H_
#define _BENCHMARK_SPS_H_

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <typeinfo>

static const long arraySize = 1000; // 1k or 1M entries in the SPS array


using namespace std;
using namespace chrono;


/**
 * This is a micro-benchmark
 */
class BenchmarkSPS {

private:
    int numThreads;

public:
    struct UserData  {
        long long seq;
        int tid;
        UserData(long long lseq, int ltid) {
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
    };

    BenchmarkSPS(int numThreads) {
        this->numThreads = numThreads;
    }


    /*
     * An array of integers that gets randomly permutated.
     */
    template<typename TM, template<typename> class TMTYPE>
    uint64_t benchmarkSPSInteger(std::string& className, const seconds testLengthSeconds, const long numSwapsPerTx, const int numRuns) {
        long long ops[numThreads][numRuns];
        long long lengthSec[numRuns];
        atomic<bool> startFlag = { false };
        atomic<bool> quit = { false };

        className = TM::className();
        cout << "##### " << TM::className() << " #####  \n";

        // Create the array of integers and initialize it
        TMTYPE<uint64_t>* parray;
        // It's ok to capture by reference, we're running single-threaded now
        TM::updateTx([&] () {
            parray = new TMTYPE<uint64_t>[arraySize];
            //parray = (TMTYPE<uint64_t>*)TM::tmMalloc(sizeof(TMTYPE<uint64_t>)*arraySize);
        } );
        // Break up the initialization into transactions of 1k stores, so it fits in the log
        for (long j = 0; j < arraySize; j+=1000) {
            TM::updateTx([&] () {
                for (int i = 0; i < 1000 && i+j < arraySize; i++) parray[i+j] = i+j;
            } );
        }

        auto func = [this,&startFlag,&quit,&numSwapsPerTx,&parray](long long *ops, const int tid) {
            uint64_t seed = tid+1234567890123456781ULL;
            // Spin until the startFlag is set
            while (!startFlag.load()) {}
            // Do transactions until the quit flag is set
            long long tcount = 0;
            while (!quit.load()) {
                TM::updateTx([&] () {
                    for (int i = 0; i < numSwapsPerTx; i++) {
                        seed = randomLong(seed);
                        auto ia = seed%arraySize;
                        uint64_t tmp = parray[ia];
                        seed = randomLong(seed);
                        auto ib = seed%arraySize;
                        parray[ia] = parray[ib];
                        parray[ib] = tmp;
                    }
                } );
                ++tcount;
                /*
                                PE::read_transaction([this,&seed,&parray,&numWordsPerTransaction] () {
                                    PersistentArrayInt<persist>* read_array = PE::template get_object<PersistentArrayInt<persist>>(PIDX_INT_ARRAY);
                                    // Check that the array is consistent
                                    int sum = 0;
                                    for (int i = 0; i < arraySize; i++) {
                                        sum += read_array->counters[i];
                                    }
                                    assert(sum == 0);
                                } );
                */
            }
            *ops = tcount;
        };
        for (int irun = 0; irun < numRuns; irun++) {
            if (irun == 0) className = TM::className();
            thread enqdeqThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) enqdeqThreads[tid] = thread(func, &ops[tid][irun], tid);
            auto startBeats = steady_clock::now();
            startFlag.store(true);
            // Sleep for 20 seconds
            this_thread::sleep_for(testLengthSeconds);
            quit.store(true);
            auto stopBeats = steady_clock::now();
            for (int tid = 0; tid < numThreads; tid++) enqdeqThreads[tid].join();
            lengthSec[irun] = (stopBeats-startBeats).count();
            startFlag.store(false);
            quit.store(false);
        }

        // It's ok to capture by reference, we're running single-threaded now
        TM::updateTx([&] () {
            delete[] parray;
            //TM::tmFree(parray);
        });

        // Accounting
        vector<long long> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
        	for(int i=0;i<numThreads;i++){
        		agg[irun] += ops[i][irun]*1000000000LL/lengthSec[irun];
        	}
        }
        // Compute the median. numRuns should be an odd number
        sort(agg.begin(),agg.end());
        auto maxops = agg[numRuns-1];
        auto minops = agg[0];
        auto medianops = agg[numRuns/2];
        auto delta = (long)(100.*(maxops-minops) / ((double)medianops));
        // Printed value is the median of the number of ops per second that all threads were able to accomplish (on average)
        std::cout << "Swaps/sec = " << medianops*numSwapsPerTx << "     delta = " << delta*numSwapsPerTx << "%   min = " << minops*numSwapsPerTx << "   max = " << maxops*numSwapsPerTx << "\n";
        return medianops*numSwapsPerTx;
    }



    /*
     * An array of objects that gets randomly permutated.
     */
    template<typename TM, template<typename> class TMTYPE, typename TMBASE>
    uint64_t benchmarkSPSObject(std::string& className, const seconds testLengthSeconds, const long numSwapsPerTx, const int numRuns) {
        long long ops[numThreads][numRuns];
        long long lengthSec[numRuns];
        atomic<bool> startFlag = { false };
        atomic<bool> quit = { false };

        struct MyObject : public TMBASE {
            uint64_t a {0};  // For the OneFile STMs these don't need to be tmtypes because they're immutable after visible in this benchmark
            uint64_t b {0};
            MyObject(uint64_t a0, uint64_t b0) {
                a = a0;
                b = b0;
            }
            MyObject(const MyObject &other) {
                a = other.a;
                b = other.b;
            }
        };

        // Create the array of integers and initialize it
        TMTYPE<MyObject*>* parray;
        parray = new TMTYPE<MyObject*>[arraySize];
        // Break up the initialization into transactions of 1k stores, so it fits in the log
        for (long j = 0; j < arraySize; j+=1000) {
            TM::updateTx([&] () {
                for (int i = 0; i < 1000 && i+j < arraySize; i++) parray[i+j] = TM::template tmNew<MyObject>((uint64_t)i+j,(uint64_t)i);
            } );
        }
        /*
         TM::updateTx([&] () {
            for (int i = 0; i < arraySize; i++) parray[i] = TM::template tmNew<MyObject>((uint64_t)i,(uint64_t)i);
        } );
        */


        auto func = [this,&startFlag,&quit,&numSwapsPerTx,&parray](long long *ops, const int tid) {
            uint64_t seed = tid+1234567890123456781ULL;
            // Spin until the startFlag is set
            while (!startFlag.load()) {}
            // Do transactions until the quit flag is set
            long long tcount = 0;
            while (!quit.load()) {
                TM::updateTx([&] () {
                    for (int i = 0; i < numSwapsPerTx; i++) {
                        seed = randomLong(seed);
                        auto ia = seed%arraySize;
                        // Create a new object with the same contents to replace the old object, at a random location
                        MyObject* tmp = TM::template tmNew<MyObject>(*parray[ia]);
                        TM::template tmDelete<MyObject>(parray[ia]);
                        parray[ia] = tmp;
                    }
                } );
                ++tcount;
            }
            *ops = tcount;
        };
        for (int irun = 0; irun < numRuns; irun++) {
            if (irun == 0) {
                className = TM::className();
                cout << "##### " << TM::className() << " #####  \n";
            }
            thread enqdeqThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) enqdeqThreads[tid] = thread(func, &ops[tid][irun], tid);
            auto startBeats = steady_clock::now();
            startFlag.store(true);
            // Sleep for 20 seconds
            this_thread::sleep_for(testLengthSeconds);
            quit.store(true);
            auto stopBeats = steady_clock::now();
            for (int tid = 0; tid < numThreads; tid++) enqdeqThreads[tid].join();
            lengthSec[irun] = (stopBeats-startBeats).count();
            startFlag.store(false);
            quit.store(false);
        }
        TM::updateTx([&] () {
            for (int i = 0; i < arraySize; i++) TM::template tmDelete<MyObject>(parray[i]);
        });
        delete[] parray;

        // Accounting
        vector<long long> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            for(int i=0;i<numThreads;i++){
                agg[irun] += ops[i][irun]*1000000000LL/lengthSec[irun];
            }
        }
        // Compute the median. numRuns should be an odd number
        sort(agg.begin(),agg.end());
        auto maxops = agg[numRuns-1];
        auto minops = agg[0];
        auto medianops = agg[numRuns/2];
        auto delta = (long)(100.*(maxops-minops) / ((double)medianops));
        // Printed value is the median of the number of ops per second that all threads were able to accomplish (on average)
        std::cout << "Swaps/sec = " << medianops*numSwapsPerTx << "     delta = " << delta*numSwapsPerTx << "%   min = " << minops*numSwapsPerTx << "   max = " << maxops*numSwapsPerTx << "\n";
        return medianops*numSwapsPerTx;
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
