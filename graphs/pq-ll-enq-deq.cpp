#include <iostream>
#include <fstream>
#include <cstring>
#include "PBenchmarkQueues.hpp"
#include "pdatastructures/pqueues/POFLFLinkedListQueue.hpp"
#include "pdatastructures/pqueues/POFWFLinkedListQueue.hpp"
#include "pdatastructures/pqueues/RomLogLinkedListQueue.hpp"
#include "pdatastructures/pqueues/RomLRLinkedListQueue.hpp"
#include "pdatastructures/pqueues/PMDKLinkedListQueue.hpp"
#include "pdatastructures/pqueues/MichaelScottQueue.hpp"
#include "pdatastructures/pqueues/PMichaelScottQueue.hpp"
#include "pdatastructures/pqueues/PFriedmanQueue.hpp"


int main(void) {
    const std::string dataFilename {"data/pq-ll-enq-deq.txt"};
    vector<int> threadList = { 1, 2, 4, 8, 16, 32, 48, 64 };     // For the laptop or AWS c5.2xlarge
    const int numPairs = 100*1000*1000;                          // Number of pairs of items to enqueue-dequeue. 100M for the paper
    const int numRuns = 1;                                       // 5 runs for the paper
    const int EMAX_CLASS = 10;
    uint64_t results[EMAX_CLASS][threadList.size()];
    std::string cNames[EMAX_CLASS];
    int maxClass = 0;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*threadList.size());
    std::cout << "If you use PMDK, don't forget to set 'export PMEM_IS_PMEM_FORCE=1'\n";

    for (unsigned it = 0; it < threadList.size(); it++) {
        auto nThreads = threadList[it];
        int ic = 0;
        PBenchmarkQueues bench(nThreads);
        std::cout << "\n----- Persistent Queues (Linked-Lists)   numPairs=" << numPairs << "   threads=" << nThreads << "   runs=" << numRuns << " -----\n";
        results[ic][it] = bench.enqDeq<POFLFLinkedListQueue<uint64_t>,poflf::OneFileLF>       (cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][it] = bench.enqDeq<POFWFLinkedListQueue<uint64_t>,pofwf::OneFileWF>       (cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][it] = bench.enqDeq<RomLogLinkedListQueue<uint64_t>,romuluslog::RomulusLog>(cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][it] = bench.enqDeq<RomLRLinkedListQueue<uint64_t>,romuluslr::RomulusLR>   (cNames[ic], numPairs, numRuns);
        ic++;
        results[ic][it] = bench.enqDeq<PMDKLinkedListQueue<uint64_t>,pmdk::PMDKTM>            (cNames[ic], numPairs, numRuns);
        ic++;
        // We have to use a lot less pairs for the Friedman Queue because it doesn't do memory reclamation and fills up the NVM pool too fast
        results[ic][it] = bench.enqDeqNoTransaction<PFriedmanQueue<uint64_t>>                 (cNames[ic], numPairs, numRuns);
        ic++;
        //results[ic][it] = bench.enqDeqNoTransaction<PMichaelScottQueue<uint64_t>>             (cNames[ic], numPairs, numRuns);
        //ic++;
        //results[ic][it] = bench.enqDeqNoTransaction<MichaelScottQueue<uint64_t>>              (cNames[ic], numPairs, numRuns);
        //ic++;
        // TODO: Add memory reclamation to Michal's queue... use Andreia's technique, or just fill up the pool
        maxClass = ic;
    }

    // Export tab-separated values to a file to be imported in gnuplot or excel
    ofstream dataFile;
    dataFile.open(dataFilename);
    dataFile << "Threads\t";
    // Printf class names
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
