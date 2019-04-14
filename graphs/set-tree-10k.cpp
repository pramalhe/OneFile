#include <iostream>
#include <fstream>
#include <cstring>
#include "BenchmarkSets.hpp"
#include "datastructures/treemaps/NatarajanTreeHE.hpp"
#include "datastructures/treemaps/OFLFRedBlackTree.hpp"
#include "datastructures/treemaps/OFWFRedBlackTree.hpp"
// Macros suck, but it's either TinySTM or ESTM, we can't have both at the same time
#ifdef USE_TINY
#include "datastructures/treemaps/TinySTMRedBlackTree.hpp"
#define DATA_FILENAME "data/set-tree-10k-tiny.txt"
#else
#include "datastructures/treemaps/ESTMRedBlackTree.hpp"
#define DATA_FILENAME "data/set-tree-10k.txt"
#endif



int main(void) {
    const std::string dataFilename {DATA_FILENAME};
    vector<int> threadList = { 1, 2, 4, 8, 16, 32, 48, 64 };     // For the laptop or AWS c5.9xlarge
    vector<int> ratioList = { 1000, 500, 100, 10, 1, 0 };        // Permil ratio: 100%, 50%, 10%, 1%, 0.1%, 0%
    const int numElements = 10000;                               // Number of keys in the set
    const int numRuns = 1;                                       // 5 runs for the paper
    const seconds testLength = 20s;                              // 20s for the paper
    const int EMAX_CLASS = 10;
    uint64_t results[EMAX_CLASS][threadList.size()][ratioList.size()];
    std::string cNames[EMAX_CLASS];
    int maxClass = 0;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*threadList.size()*ratioList.size());

    double totalHours = (double)EMAX_CLASS*ratioList.size()*threadList.size()*testLength.count()*numRuns/(60.*60.);
    std::cout << "This benchmark is going to take about " << totalHours << " hours to complete\n";

    for (unsigned iratio = 0; iratio < ratioList.size(); iratio++) {
        auto ratio = ratioList[iratio];
        for (unsigned it = 0; it < threadList.size(); it++) {
            auto nThreads = threadList[it];
            int ic = 0;
            BenchmarkSets bench(nThreads);
            std::cout << "\n----- Sets (Trees)   numElements=" << numElements << "   ratio=" << ratio/10. << "%   threads=" << nThreads << "   runs=" << numRuns << "   length=" << testLength.count() << "s -----\n";
#ifdef USE_TINY
            results[ic][it][iratio] = bench.benchmark<TinySTMRedBlackTree<uint64_t,uint64_t>,uint64_t>                                     (cNames[ic], ratio, testLength, numRuns, numElements, false);
            ic++;
#else
            results[ic][it][iratio] = bench.benchmark<OFLFRedBlackTree<uint64_t,uint64_t>,uint64_t>                                        (cNames[ic], ratio, testLength, numRuns, numElements, false);
            ic++;
            results[ic][it][iratio] = bench.benchmark<OFWFRedBlackTree<uint64_t,uint64_t>,uint64_t>                                        (cNames[ic], ratio, testLength, numRuns, numElements, false);
            ic++;
            results[ic][it][iratio] = bench.benchmark<ESTMRedBlackTree<uint64_t,uint64_t>,uint64_t>                                        (cNames[ic], ratio, testLength, numRuns, numElements, false);
            ic++;
            results[ic][it][iratio] = bench.benchmark<NatarajanTreeHE<uint64_t,uint64_t>,uint64_t>                                         (cNames[ic], ratio, testLength, numRuns, numElements, false);
            ic++;
#endif
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
