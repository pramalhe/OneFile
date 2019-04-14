#include <iostream>
#include <fstream>
#include <cstring>
#include "BenchmarkSets.hpp"
#include "datastructures/linkedlists/MagedHarrisLinkedListSetHP.hpp"
#include "datastructures/linkedlists/MagedHarrisLinkedListSetHE.hpp"
#include "datastructures/linkedlists/OFLFLinkedListSet.hpp"
#include "datastructures/linkedlists/OFWFLinkedListSet.hpp"
// Macros suck, but it's either TL2 or TinySTM or ESTM, we can't have all at the same time
#if defined USE_TL2
#include "datastructures/linkedlists/TL2STMLinkedListSet.hpp"
#define DATA_FILENAME "data/set-ll-10k-tl2.txt"
#elif defined USE_TINY
#include "datastructures/linkedlists/TinySTMLinkedListSet.hpp"
#define DATA_FILENAME "data/set-ll-10k-tiny.txt"
#else
#include "datastructures/linkedlists/ESTMLinkedListSet.hpp"
#define DATA_FILENAME "data/set-ll-10k.txt"
#endif


int main(void) {
    const std::string dataFilename {DATA_FILENAME};
    vector<int> threadList = { 1, 2, 4, 8, 16, 32, 48, 64 };     // For the laptop or AWS c5.2xlarge
    //vector<int> threadList = { 1, 2, 4, 8, 16, 32, 48, 64, 96 }; // For Cervino
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
    std::cout << "This benchmark is going to take at most " << totalHours << " hours to complete\n";

    for (unsigned ir = 0; ir < ratioList.size(); ir++) {
        auto ratio = ratioList[ir];
        for (unsigned it = 0; it < threadList.size(); it++) {
            auto nThreads = threadList[it];
            int ic = 0;
            BenchmarkSets bench(nThreads);
            std::cout << "\n----- Sets (Linked-Lists)   numElements=" << numElements << "   ratio=" << ratio/10. << "%   threads=" << nThreads << "   runs=" << numRuns << "   length=" << testLength.count() << "s -----\n";
#if defined USE_TL2
            results[ic][it][ir] = bench.benchmark<TL2STMLinkedListSet<UserData>,UserData>            (cNames[ic], ratio, testLength, numRuns, numElements, false);
            ic++;
#elif defined USE_TINY
            results[ic][it][ir] = bench.benchmark<TinySTMLinkedListSet<UserData>,UserData>           (cNames[ic], ratio, testLength, numRuns, numElements, false);
            ic++;
#else
            results[ic][it][ir] = bench.benchmark<OFLFLinkedListSet<UserData>,UserData>              (cNames[ic], ratio, testLength, numRuns, numElements, false);
            ic++;
            results[ic][it][ir] = bench.benchmark<OFWFLinkedListSet<UserData>,UserData>              (cNames[ic], ratio, testLength, numRuns, numElements, false);
            ic++;
            results[ic][it][ir] = bench.benchmark<ESTMLinkedListSet<UserData>,UserData>              (cNames[ic], ratio, testLength, numRuns, numElements, false);
            ic++;
            results[ic][it][ir] = bench.benchmark<MagedHarrisLinkedListSetHP<UserData>,UserData>     (cNames[ic], ratio, testLength, numRuns, numElements, false);
            ic++;
            results[ic][it][ir] = bench.benchmark<MagedHarrisLinkedListSetHE<UserData>,UserData>     (cNames[ic], ratio, testLength, numRuns, numElements, false);
            ic++;
            //results[ic][it][ir] = bench.benchmark<UCSet<CXMutationWF<LinkedListSet<UserData>>,LinkedListSet<UserData>,UserData>,UserData>          (cNames[ic], ratio, testLength, numRuns, numElements, false);
            //ic++;
            //results[ic][it][ir] = bench.benchmark<UCSet<CXMutationWFTimed<LinkedListSet<UserData>>,LinkedListSet<UserData>,UserData>,UserData>     (cNames[ic], ratio, testLength, numRuns, numElements, false);
            //ic++;
#endif
            maxClass = ic;
        }
    }

    // Export tab-separated values to a file to be imported in gnuplot or excel
    ofstream dataFile;
    dataFile.open(dataFilename);
    dataFile << "Threads\t";
    // Printf class names and ratios for each column
    for (unsigned iratio = 0; iratio < ratioList.size(); iratio++) {
        auto ratio = ratioList[iratio];
        for (int iclass = 0; iclass < maxClass; iclass++) dataFile << cNames[iclass] << "-" << ratio/10. << "%"<< "\t";
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
