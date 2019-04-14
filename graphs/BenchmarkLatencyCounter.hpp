/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _BENCHMARK_LATENCY_COUNTER_H_
#define _BENCHMARK_LATENCY_COUNTER_H_

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>


using namespace std;
using namespace chrono;


/**
 * This is a micro-benchmark for measuring on an array of counters
 */
class BenchmarkLatencyCounter {

private:
    // Latency constants
    static const long long kLatencyMeasures =   1000000LL;   // We measure 100M iterations
    static const long long kLatencyWarmups =     100000LL;   // Plus these many warmup

    static const long long NSEC_IN_SEC = 1000000000LL;
    static const uint64_t NUM_COUNTERS = 64;

    int numThreads;

public:
    struct Result {
        uint64_t delay50000;
        uint64_t delay90000;
        uint64_t delay99000;
        uint64_t delay99900;
        uint64_t delay99990;
        uint64_t delay99999;
    };

    BenchmarkLatencyCounter(int numThreads) {
        this->numThreads = numThreads;
    }

    /*
     * Execute latency benchmarks
     * We only do one run for this benchmark
     */
    template<typename TM, template<typename> class TMTYPE>
    Result latencyBenchmark(std::string& className) {
        atomic<bool> start = { false };
        TMTYPE<uint64_t> *counters;
        TM::template updateTx([&] () { // It's ok to pass by reference because we're single-threaded
            counters = (TMTYPE<uint64_t>*)TM::tmMalloc(sizeof(TMTYPE<uint64_t>)*NUM_COUNTERS);
            for (int i = 0; i < NUM_COUNTERS; i++) counters[i] = 0;
        });

        auto latency_lambda = [this,&start,&counters](nanoseconds* delays, const int tid) {
            long long delayIndex = 0;
            while (!start.load()) this_thread::yield();
            // Warmup + Measurements
            for (int iter=0; iter < (kLatencyWarmups+kLatencyMeasures)/numThreads; iter++) {
                // Alternate transactions between left-right and right-left
                auto startBeats = steady_clock::now();
                TM::updateTx([=] () {
                    for (int i = 0; i < NUM_COUNTERS; i++) counters[i] = counters[i]+1;
                });
                auto stopBeats = steady_clock::now();
                if (iter >= kLatencyWarmups/numThreads) delays[delayIndex++] = (stopBeats-startBeats);
                TM::updateTx([=] () {
                    for (int i = NUM_COUNTERS-1; i > 0; i--) counters[i] = counters[i]+1;
                });
            }
        };

        nanoseconds* delays[numThreads];
        for (int it = 0; it < numThreads; it++) {
            delays[it] = new nanoseconds[kLatencyMeasures/numThreads];
            for (int imeas=0; imeas < kLatencyMeasures/numThreads; imeas++) delays[it][imeas] = 0ns;
        }

        cout << "##### " << TM::className() << " #####  \n";
        className = TM::className();
        thread latencyThreads[numThreads];
        for (int tid = 0; tid < numThreads; tid++) latencyThreads[tid] = thread(latency_lambda, delays[tid], tid);
        start.store(true);
        this_thread::sleep_for(50ms);
        for (int tid = 0; tid < numThreads; tid++) latencyThreads[tid].join();

        // Aggregate all the delays for enqueues and dequeues and compute the maxs
        cout << "Aggregating delays for " << kLatencyMeasures/1000000 << " million measurements...\n";
        vector<nanoseconds> aggDelay(kLatencyMeasures);
        long long idx = 0;
        for (int it = 0; it < numThreads; it++) {
            for (int i = 0; i < kLatencyMeasures/numThreads; i++) {
                aggDelay[idx] = delays[it][i];
                idx++;
            }
        }

        // Sort the aggregated delays
        cout << "Sorting delays...\n";
        sort(aggDelay.begin(), aggDelay.end());

        // Show the 50% (median), 90%, 99%, 99.9%, 99.99%, 99.999% and maximum in microsecond/nanoseconds units
        long per50000 = (long)(kLatencyMeasures*50000LL/100000LL);
        long per70000 = (long)(kLatencyMeasures*70000LL/100000LL);
        long per80000 = (long)(kLatencyMeasures*80000LL/100000LL);
        long per90000 = (long)(kLatencyMeasures*90000LL/100000LL);
        long per99000 = (long)(kLatencyMeasures*99000LL/100000LL);
        long per99900 = (long)(kLatencyMeasures*99900LL/100000LL);
        long per99990 = (long)(kLatencyMeasures*99990LL/100000LL);
        long per99999 = (long)(kLatencyMeasures*99999LL/100000LL);
        long imax = kLatencyMeasures-1;

        cout << "Enqueue delay (us): 50%=" << aggDelay[per50000].count()/1000 << "  70%=" << aggDelay[per70000].count()/1000 << "  80%=" << aggDelay[per80000].count()/1000
             << "  90%=" << aggDelay[per90000].count()/1000 << "  99%=" << aggDelay[per99000].count()/1000
             << "  99.9%=" << aggDelay[per99900].count()/1000 << "  99.99%=" << aggDelay[per99990].count()/1000
             << "  99.999%=" << aggDelay[per99999].count()/1000 << "  max=" << aggDelay[imax].count()/1000 << "\n";

        Result res = {
            (uint64_t)aggDelay[per50000].count()/1000, (uint64_t)aggDelay[per90000].count()/1000,
            (uint64_t)aggDelay[per99000].count()/1000, (uint64_t)aggDelay[per99900].count()/1000,
            (uint64_t)aggDelay[per99990].count()/1000, (uint64_t)aggDelay[per99999].count()/1000
        };
/*
        // Show in csv format
        cout << "delay (us):\n";
        cout << "50, " << aggDelay[per50000].count()/1000 << "\n";
        cout << "90, " << aggDelay[per90000].count()/1000 << "\n";
        cout << "99, " << aggDelay[per99000].count()/1000 << "\n";
        cout << "99.9, " << aggDelay[per99900].count()/1000 << "\n";
        cout << "99.99, " << aggDelay[per99990].count()/1000 << "\n";
        cout << "99.999, " << aggDelay[per99999].count()/1000 << "\n";
*/
        TM::template updateTx([&] () { // It's ok to pass by reference because we're single-threaded
            TM::tmFree(counters);
        });

        // Cleanup
        for (int it = 0; it < numThreads; it++) delete[] delays[it];
        return res;
    }


#ifdef NEVER
public:

    static void allLatencyTests() {
        // Burst Latency benchmarks
        //vector<int> threadList = { 30, 30, 30, 30, 30, 30, 30 }; // For the latency table in the paper
        //vector<int> threadList = { 4 };
        vector<int> threadList = { 1, 2, 4, 8, 12, 16, 20, 24, 28, 30, 32 };

        for (int nThreads : threadList) {
            BenchmarkLatencyQ bench(nThreads, 0, 0s); // Only the numThreads is used in this test
            std::cout << "\n----- Burst Latency   numThreads=" << bench.numThreads << "   kLatencyMeasures=" << kLatencyMeasures/1000000LL << "M -----\n";
            bench.latencyBurstBenchmark<MichaelScottQueue<UserData>>();
        }
        for (int nThreads : threadList) {
            BenchmarkLatencyQ bench(nThreads, 0, 0s); // Only the numThreads is used in this test
            std::cout << "\n----- Burst Latency   numThreads=" << bench.numThreads << "   kLatencyMeasures=" << kLatencyMeasures/1000000LL << "M -----\n";
            bench.latencyBurstBenchmark<BitNextQueue<UserData>>();
        }
        for (int nThreads : threadList) {
            BenchmarkLatencyQ bench(nThreads, 0, 0s); // Only the numThreads is used in this test
            std::cout << "\n----- Burst Latency   numThreads=" << bench.numThreads << "   kLatencyMeasures=" << kLatencyMeasures/1000000LL << "M -----\n";
            bench.latencyBurstBenchmark<BitNextLazyHeadQueue<UserData>>();
        }
        /*
        for (int nThreads : threadList) {
            BenchmarkLatencyQ bench(nThreads, 0, 0s); // Only the numThreads is used in this test
            std::cout << "\n----- Burst Latency   numThreads=" << bench.numThreads << "   kLatencyMeasures=" << kLatencyMeasures/1000000LL << "M -----\n";
            bench.latencyBurstBenchmark<KoganPetrankQueueCHP<UserData>>();
        }
        for (int nThreads : threadList) {
            BenchmarkLatencyQ bench(nThreads, 0, 0s); // Only the numThreads is used in this test
            std::cout << "\n----- Burst Latency   numThreads=" << bench.numThreads << "   kLatencyMeasures=" << kLatencyMeasures/1000000LL << "M -----\n";
            bench.latencyBurstBenchmark<CRTurnQueue<UserData>>();
        }
        */
    }
#endif
};

#endif
