/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _BENCHMARK_Q_H_
#define _BENCHMARK_Q_H_

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <cassert>


using namespace std;
using namespace chrono;

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


/**
 * This is a micro-benchmark to run the tests shown in CRTurnQueue paper
 *
 * <h2> Performance Benchmarks </h2>
 * TODO
 *
 *
 * <h2> Latency Distribution </h2>
 *
 * - We fire up 28 threads of type LatencyThread;
 * - Each thread does alternatively 1000 enqueue() and 1000 dequeue(). All dequeues are non-null;
 * - After start, each thread does 1M iterations as warmup.
 * - Measurements are done for 4M iterations, that are saved in a local array, 2M enqueue and 2M dequeue;
 * -
 *
 */
class BenchmarkQueues {

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

    // Performance benchmark constants
    static const long long kNumPairsWarmup =     1000000LL;     // Each threads does 1M iterations as warmup

    // Contants for Ping-Pong performance benchmark
    static const int kPingPongBatch = 1000;            // Each thread starts by injecting 1k items in the queue


    static const long long NSEC_IN_SEC = 1000000000LL;

    int numThreads;

public:

    BenchmarkQueues(int numThreads) {
        this->numThreads = numThreads;
    }


    /**
     * enqueue-dequeue pairs: in each iteration a thread executes an enqueue followed by a dequeue;
     * the benchmark executes 10^8 pairs partitioned evenly among all threads;
     */
    template<typename Q>
    uint64_t enqDeq(std::string& className, const long numPairs, const int numRuns) {
        nanoseconds deltas[numThreads][numRuns];
        atomic<bool> startFlag = { false };
        Q* queue = nullptr;
        className = Q::className();
        cout << "##### " << className << " #####  \n";

        auto enqdeq_lambda = [this,&startFlag,&numPairs,&queue](nanoseconds *delta, const int tid) {
            UserData ud(0,0);
            while (!startFlag.load()) {} // Spin until the startFlag is set
            // Warmup phase
            for (long long iter = 0; iter < kNumPairsWarmup/numThreads; iter++) {
                queue->enqueue(&ud, tid);
                if (queue->dequeue(tid) == nullptr) cout << "Error at warmup dequeueing iter=" << iter << "\n";
            }
            // Measurement phase
            auto startBeats = steady_clock::now();
            for (long long iter = 0; iter < numPairs/numThreads; iter++) {
                queue->enqueue(&ud, tid);
                if (queue->dequeue(tid) == nullptr) cout << "Error at measurement dequeueing iter=" << iter << "\n";
            }
            auto stopBeats = steady_clock::now();
            *delta = stopBeats - startBeats;
        };

        for (int irun = 0; irun < numRuns; irun++) {
            queue = new Q(numThreads);
            thread enqdeqThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) enqdeqThreads[tid] = thread(enqdeq_lambda, &deltas[tid][irun], tid);
            startFlag.store(true);
            // Sleep for 2 seconds just to let the threads see the startFlag
            this_thread::sleep_for(2s);
            for (int tid = 0; tid < numThreads; tid++) enqdeqThreads[tid].join();
            startFlag.store(false);
            delete (Q*)queue;
        }

        // Sum up all the time deltas of all threads so we can find the median run
        vector<nanoseconds> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            agg[irun] = 0ns;
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun] += deltas[tid][irun];
            }
        }

        // Compute the median. numRuns should be an odd number
        sort(agg.begin(),agg.end());
        auto median = agg[numRuns/2].count()/numThreads; // Normalize back to per-thread time (mean of time for this run)

        cout << "Total Ops/sec = " << numPairs*2*NSEC_IN_SEC/median << "\n";
        return (numPairs*2*NSEC_IN_SEC/median);
    }


    /**
     * Start with only enqueues 100K/numThreads, wait for them to finish, then do only dequeues but only 100K/numThreads
     */
    template<typename Q>
    void burst(std::string& className, uint64_t& resultsEnq, uint64_t& resultsDeq,
               const long long burstSize, const int numIters, const int numRuns, const bool isSC=false) {
        Result results[numThreads][numRuns];
        atomic<bool> startEnq = { false };
        atomic<bool> startDeq = { false };
        atomic<long> barrier = { 0 };
        Q* queue = nullptr;

        auto burst_lambda = [this,&startEnq,&startDeq,&burstSize,&barrier,&numIters,&isSC,&queue](Result *res, const int tid) {
            UserData ud(0,0);
            // Warmup only if it is not Single-Consumer
            if (!isSC) {
                const long long warmupIters = 100000LL;  // Do 100K for each thread as a warmup
                for (long long iter = 0; iter < warmupIters; iter++) queue->enqueue(&ud, tid);
                for (long long iter = 0; iter < warmupIters; iter++) {
                    if (queue->dequeue(tid) == nullptr) cout << "ERROR: warmup dequeued nullptr in iter=" << iter << "\n";
                }
            }
            // Measurements
            for (int iter = 0; iter < numIters; iter++) {
                // Start with enqueues
                while (!startEnq.load()) {} // spin is better than yield here
                auto startBeats = steady_clock::now();
                for (long long i = 0; i < burstSize/numThreads; i++) {
                    queue->enqueue(&ud, tid);
                }
                auto stopBeats = steady_clock::now();
                res->nsEnq += (stopBeats-startBeats);
                res->numEnq += burstSize/numThreads;
                if (barrier.fetch_add(1) == numThreads) cout << "ERROR: in barrier\n";
                // dequeues
                while (!startDeq.load()) { } // spin is better than yield here
                if (isSC) { // Handle the single-consumer case
                    if (tid == 0) {
                        startBeats = steady_clock::now();
                        // We need to deal with rounding errors in the single-consumer case
                        for (long long i = 0; i < ((long long)(burstSize/numThreads))*numThreads; i++) {
                            if (queue->dequeue(tid) == nullptr) {
                                cout << "ERROR: dequeued nullptr in iter=" << i << "\n";
                                assert(false);
                            }
                        }
                        stopBeats = steady_clock::now();
                        if (queue->dequeue(tid) != nullptr) cout << "ERROR: dequeued non-null, there must be duplicate items!\n";
                        res->nsDeq += (stopBeats-startBeats);
                        res->numDeq += burstSize/numThreads;
                    }
                } else {
                    startBeats = steady_clock::now();
                    for (long long i = 0; i < burstSize/numThreads; i++) {
                        if (queue->dequeue(tid) == nullptr) {
                            cout << "ERROR: dequeued nullptr in iter=" << i << "\n";
                            assert(false);
                        }
                    }
                    stopBeats = steady_clock::now();
                    res->nsDeq += (stopBeats-startBeats);
                    res->numDeq += burstSize/numThreads;
                }
                if (barrier.fetch_add(1) == numThreads) cout << "ERROR: in barrier\n";
            }
        };

        for (int irun = 0; irun < numRuns; irun++) {
            queue = new Q(numThreads);
            if (irun == 0) {
                className = queue->className();
                cout << "##### " << queue->className() << " #####  \n";
            }
            thread burstThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) burstThreads[tid] = thread(burst_lambda, &results[tid][irun], tid);
            this_thread::sleep_for(100ms);
            for (int iter=0; iter < numIters; iter++) {
                // enqueue round
                startEnq.store(true);
                while (barrier.load() != numThreads) this_thread::yield();
                startEnq.store(false);
                long tmp =  numThreads;
                if (!barrier.compare_exchange_strong(tmp, 0)) cout << "ERROR: CAS\n";
                // dequeue round
                startDeq.store(true);
                while (barrier.load() != numThreads) this_thread::yield();
                startDeq.store(false);
                tmp = numThreads;
                if (!barrier.compare_exchange_strong(tmp, 0)) cout << "ERROR: CAS\n";
            }
            for (int tid = 0; tid < numThreads; tid++) burstThreads[tid].join();
            delete queue;
        }

        // Accounting
        vector<Result> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            nanoseconds maxNsEnq = 0ns;
            nanoseconds maxNsDeq = 0ns;
            for (int tid = 0; tid < numThreads; tid++) {
                if (results[tid][irun].nsEnq > maxNsEnq) maxNsEnq = results[tid][irun].nsEnq;
                if (results[tid][irun].nsDeq > maxNsDeq) maxNsDeq = results[tid][irun].nsDeq;
                agg[irun].numEnq += results[tid][irun].numEnq;
                agg[irun].numDeq += results[tid][irun].numDeq;
            }
            agg[irun].nsEnq = maxNsEnq;
            agg[irun].nsDeq = maxNsDeq;
            agg[irun].totOpsSec = agg[irun].nsEnq.count()+agg[irun].nsDeq.count();
        }

        // Compute the median. numRuns should be an odd number
        sort(agg.begin(),agg.end());
        Result median = agg[numRuns/2];
        const long long allThreadsEnqPerSec = median.numEnq*NSEC_IN_SEC/median.nsEnq.count();
        const long long allThreadsDeqPerSec = median.numDeq*NSEC_IN_SEC/median.nsDeq.count();

        // Printed value is the median of the number of ops per second that all threads were able to accomplish (on average)
        cout << "Enq/sec = " << allThreadsEnqPerSec << "   Deq/sec = " << allThreadsDeqPerSec << "\n";
        resultsEnq = allThreadsEnqPerSec;
        resultsDeq = allThreadsDeqPerSec;
    }
};

#endif
