/*
 * This benchmark executes SPS for the following PTMs:
 * - RomulusLog
 * - RomulusLR
 * - PMDK
 * - OneFilePTM-LF (lock-free)
 * - OneFilePTM-WF (wait-free bounded)
 */
#include <iostream>
#include <fstream>
#include <cstring>

#include "PBenchmarkSPS.hpp"
#include "ptms/romuluslog/RomulusLog.hpp"
#include "ptms/romuluslr/RomulusLR.hpp"
#include "ptms/PMDKTM.hpp"
#include "ptms/OneFilePTMLF.hpp"
#include "ptms/OneFilePTMWF.hpp"


int main(void) {
    const std::string dataFilename {"data/psps-integer.txt"};
    vector<int> threadList = { 1, 2, 4, 8, 16, 32, 48, 64 }; // For the laptop or AWS c5.9xlarge
    vector<long> swapsPerTxList = { 1, 4, 8, 16, 32, 64, 128, 256 };
    const int numRuns = 1;                                   // 5 runs for the paper
    const seconds testLength = 20s;                          // 20s for the paper
    const int EMAX_CLASS = 10;
    int maxClass = 0;
    uint64_t results[EMAX_CLASS][threadList.size()][swapsPerTxList.size()];
    std::string cNames[EMAX_CLASS];
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*threadList.size()*swapsPerTxList.size());

    // SPS Benchmarks multi-threaded
    std::cout << "If you use PMDK, don't forget to set 'export PMEM_IS_PMEM_FORCE=1'\n";
    std::cout << "\n----- Persistent SPS Benchmark (multi-threaded integer array swap) -----\n";
    for (int it = 0; it < threadList.size(); it++) {
        int nThreads = threadList[it];
        for (int is = 0; is < swapsPerTxList.size(); is++) {
            int nWords = swapsPerTxList[is];
            int ic = 0;
            PBenchmarkSPS bench(nThreads);
            std::cout << "\n----- threads=" << nThreads << "   runs=" << numRuns << "   length=" << testLength.count() << "s   arraySize=" << arraySize << "   swaps/tx=" << nWords << " -----\n";
            results[ic][it][is] = bench.benchmarkSPSInteger<poflf::OneFileLF,        poflf::tmtype>        (cNames[ic], testLength, nWords, numRuns);
            ic++;
            results[ic][it][is] = bench.benchmarkSPSInteger<pofwf::OneFileWF,        pofwf::tmtype>        (cNames[ic], testLength, nWords, numRuns);
            ic++;
            results[ic][it][is] = bench.benchmarkSPSInteger<romuluslog::RomulusLog,  romuluslog::persist>  (cNames[ic], testLength, nWords, numRuns);
            ic++;
            results[ic][it][is] = bench.benchmarkSPSInteger<romuluslr::RomulusLR,    romuluslr::persist>   (cNames[ic], testLength, nWords, numRuns);
            ic++;
            results[ic][it][is] = bench.benchmarkSPSInteger<pmdk::PMDKTM,            pmdk::persist>        (cNames[ic], testLength, nWords, numRuns);
            ic++;
            maxClass = ic;
        }
        std::cout << "\n";
    }

    // Export tab-separated values to a file to be imported in gnuplot or excel
    ofstream dataFile;
    dataFile.open(dataFilename);
    dataFile << "Swaps\t";
    // Printf class names for each column plus the corresponding thread
    for (int ic = 0; ic < maxClass; ic++) {
        for (int it = 0; it < threadList.size(); it++) {
            int nThreads = threadList[it];
            dataFile << cNames[ic] << "-" << nThreads <<"T\t";
        }
    }
    dataFile << "\n";
    for (int is = 0; is < swapsPerTxList.size(); is++) {
        dataFile << swapsPerTxList[is] << "\t";
        for (int ic = 0; ic < maxClass; ic++) {
            for (int it = 0; it < threadList.size(); it++) {
                dataFile << results[ic][it][is] << "\t";
            }
        }
        dataFile << "\n";
    }
    dataFile.close();
    std::cout << "\nSuccessfuly saved results in " << dataFilename << "\n";



    return 0;
}
