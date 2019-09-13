#include <iostream>
#include <fstream>
#include <cstring>

#include "PBenchmarkSets.hpp"
#include "pdatastructures/TMRedBlackTree.hpp"
#include "pdatastructures/TMRedBlackTreeByRef.hpp"
#include "ptms/romuluslog/RomulusLog.hpp"
#include "ptms/romuluslr/RomulusLR.hpp"
#include "ptms/PMDKTM.hpp"
#include "ptms/OneFilePTMLF.hpp"
#include "ptms/OneFilePTMWF.hpp"


int main(void) {
    const std::string dataFilename {"data/pset-tree-1k.txt"};
    vector<int> threadList = { 1, 2, 4, 8, 16, 32, 48, 64 };     // For the laptop or AWS c5.9xlarge
    vector<int> ratioList = { 1000, 500, 100, 10, 1, 0 };        // Permil ratio: 100%, 50%, 10%, 1%, 0.1%, 0%
    const int numElements = 1000;                                // Number of keys in the set
    const int numRuns = 1;                                       // 5 runs for the paper
    const seconds testLength = 20s;                              // 20s for the paper
    const int EMAX_CLASS = 10;
    uint64_t results[EMAX_CLASS][threadList.size()][ratioList.size()];
    std::string cNames[EMAX_CLASS];
    int maxClass = 0;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*threadList.size()*ratioList.size());

    double totalHours = (double)EMAX_CLASS*ratioList.size()*threadList.size()*testLength.count()*numRuns/(60.*60.);
    std::cout << "This benchmark is going to take at most " << totalHours << " hours to complete\n";
    std::cout << "If you use PMDK, don't forget to set 'export PMEM_IS_PMEM_FORCE=1'\n";

    PBenchmarkSets<uint64_t> bench;
    for (unsigned ir = 0; ir < ratioList.size(); ir++) {
        auto ratio = ratioList[ir];
        for (unsigned it = 0; it < threadList.size(); it++) {
            auto nThreads = threadList[it];
            int ic = 0;
            std::cout << "\n----- Persistent Sets (Red-Black Tree)   numElements=" << numElements << "   ratio=" << ratio/10. << "%   threads=" << nThreads << "   runs=" << numRuns << "   length=" << testLength.count() << "s -----\n";
            results[ic][it][ir] = bench.benchmark<TMRedBlackTree<uint64_t,uint64_t,poflf::OneFileLF,poflf::tmtype>,                    poflf::OneFileLF>       (cNames[ic], nThreads, ratio, testLength, numRuns, numElements, false);
            ic++;
            results[ic][it][ir] = bench.benchmark<TMRedBlackTree<uint64_t,uint64_t,pofwf::OneFileWF,pofwf::tmtype>,                    pofwf::OneFileWF>       (cNames[ic], nThreads, ratio, testLength, numRuns, numElements, false);
            ic++;
            results[ic][it][ir] = bench.benchmark<TMRedBlackTreeByRef<uint64_t,uint64_t,romuluslog::RomulusLog,romuluslog::persist>,   romuluslog::RomulusLog> (cNames[ic], nThreads, ratio, testLength, numRuns, numElements, false);
            ic++;
            results[ic][it][ir] = bench.benchmark<TMRedBlackTreeByRef<uint64_t,uint64_t,romuluslr::RomulusLR,romuluslr::persist>,      romuluslr::RomulusLR>   (cNames[ic], nThreads, ratio, testLength, numRuns, numElements, false);
            ic++;
            results[ic][it][ir] = bench.benchmark<TMRedBlackTreeByRef<uint64_t,uint64_t,pmdk::PMDKTM,pmdk::persist>,                   pmdk::PMDKTM>           (cNames[ic], nThreads, ratio, testLength, numRuns, numElements, false);
            ic++;
            maxClass = ic;
        }
    }

    // Export tab-separated values to a file to be imported in gnuplot or excel
    ofstream dataFile;
    dataFile.open(dataFilename);
    dataFile << "Threads\t";
    // Printf class names and ratios for each column
    for (unsigned ir = 0; ir < ratioList.size(); ir++) {
        auto ratio = ratioList[ir];
        for (int ic = 0; ic < maxClass; ic++) dataFile << cNames[ic] << "-" << ratio/10. << "%"<< "\t";
    }
    dataFile << "\n";
    for (int it = 0; it < threadList.size(); it++) {
        dataFile << threadList[it] << "\t";
        for (unsigned ir = 0; ir < ratioList.size(); ir++) {
            for (int ic = 0; ic < maxClass; ic++) dataFile << results[ic][it][ir] << "\t";
        }
        dataFile << "\n";
    }
    dataFile.close();
    std::cout << "\nSuccessfuly saved results in " << dataFilename << "\n";

    return 0;
}
