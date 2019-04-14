/*
 * Executes the following non-blocking (array based) queues in a single-enqueue-single-dequeue benchmark:
 * - LCRQ (lock-free)
 * - FAAArrayQueue (lock-free)
 * - MWC-LF (lock-free)
 * - MWC-WF (wait-free bounded)
 */
#include <iostream>
#include <fstream>
#include <cstring>
#include "BenchmarkQueues.hpp"
#include "datastructures/queues/LCRQueue.hpp"
#include "datastructures/queues/FAAArrayQueue.hpp"
#include "datastructures/queues/OFLFArrayLinkedListQueue.hpp"
#include "datastructures/queues/OFWFArrayLinkedListQueue.hpp"
#include "datastructures/queues/OFLFArrayQueue.hpp"
// Macros suck, but it's either TL2 or TinySTM or ESTM, we can't have all at the same time
#if defined USE_TL2
#include "datastructures/queues/TL2STMArrayLinkedListQueue.hpp"
#include "datastructures/queues/TL2STMArrayQueue.hpp"
#define DATA_FILENAME "data/q-array-enq-deq-tl2.txt"
#elif defined USE_TINY
#include "datastructures/queues/TinySTMArrayLinkedListQueue.hpp"
#define DATA_FILENAME "data/q-array-enq-deq-tiny.txt"
#else
#include "datastructures/queues/ESTMArrayLinkedListQueue.hpp"
#define DATA_FILENAME "data/q-array-enq-deq.txt"
#endif

#define MILLION  1000000LL

int main(void) {
    const std::string dataFilename {DATA_FILENAME};
    vector<int> threadList = { 1, 2, 4, 8, 16, 32, 48, 64 };     // For the laptop or AWS c5.9xlarge
    const int numRuns = 1;                                   // Number of runs
    const long numPairs = 100*MILLION;                       // 10M is fast enough on the laptop, but on AWS we can use 100M
    const int EMAX_CLASS = 10;
    uint64_t results[EMAX_CLASS][threadList.size()];
    std::string cNames[EMAX_CLASS];
    int maxClass = 0;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*threadList.size());

    // Enq-Deq Throughput benchmarks
    for (int ithread = 0; ithread < threadList.size(); ithread++) {
        int nThreads = threadList[ithread];
        int ic = 0;
        BenchmarkQueues bench(nThreads);
        std::cout << "\n----- q-array-enq-deq   threads=" << nThreads << "   pairs=" << numPairs/MILLION << "M   runs=" << numRuns << "-----\n";
#ifdef USE_TL2
        results[ic][ithread] = bench.enqDeq<TL2STMArrayLinkedListQueue<UserData>> (cNames[ic], numPairs, numRuns);
        //results[iclass][ithread] = bench.enqDeq<TL2STMArrayQueue<UserData>> (cNames[ic], numPairs, numRuns);
        ic++;
#elif defined USE_TINY
        results[ic][ithread] = bench.enqDeq<TinySTMArrayLinkedListQueue<UserData>> (cNames[ic], numPairs, numRuns);
        ic++;
#else
        results[ic][ithread] = bench.enqDeq<OFLFArrayLinkedListQueue<UserData>>   (cNames[ic], numPairs, numRuns);
        //results[iclass][ithread] = bench.enqDeq<OFLFArrayQueue<UserData>>   (cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][ithread] = bench.enqDeq<OFWFArrayLinkedListQueue<UserData>>   (cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][ithread] = bench.enqDeq<ESTMArrayLinkedListQueue<UserData>>   (cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][ithread] = bench.enqDeq<FAAArrayQueue<UserData>>              (cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][ithread] = bench.enqDeq<LCRQueue<UserData>>                   (cNames[ic], numPairs, numRuns);
        ic++;
#endif
        maxClass = ic;
    }

    // Export tab-separated values to a file to be imported in gnuplot or excel
    ofstream dataFile;
    dataFile.open(dataFilename);
    dataFile << "Threads\t";
    // Printf class names for each column
    for (int ic = 0; ic < maxClass; ic++) dataFile << cNames[ic] << "\t";
    dataFile << "\n";
    for (int it = 0; it < threadList.size(); it++) {
        dataFile << threadList[it] << "\t";
        for (int ic = 0; ic < maxClass; ic++) dataFile << results[ic][it] << "\t";
        dataFile << "\n";
    }
    dataFile.close();
    std::cout << "\nSuccessfuly saved results in " << dataFilename << "\n";

    return 0;
}
