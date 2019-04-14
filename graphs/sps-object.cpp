/*
 * Executes SPS (with objects) for the following STMs:
 * - OneFileLF (lock-free)
 * - OneFileWF (bounded wait-free)
 * - Elastic STM (blocking)
 * - TinySTM (blocking)
 * - TL2 (blocking)
 */
#include <iostream>
#include <fstream>
#include <cstring>
#include "BenchmarkSPS.hpp"
//#include "stms/CRWWPSTM.hpp"
#include "stms/OneFileLF.hpp"
#include "stms/OneFileWF.hpp"
// Macros suck, but it's either TL2 or TinySTM or ESTM, we can't have all at the same time
#if defined USE_TL2
#include "stms/TL2STM.hpp"
#define DATA_FILENAME "data/sps-object-tl2.txt"
#elif defined USE_TINY
#include "stms/TinySTM.hpp"
#define DATA_FILENAME "data/sps-object-tiny.txt"
#else
#include "stms/ESTM.hpp"
#define DATA_FILENAME "data/sps-object.txt"
#endif


int main(void) {
    const std::string dataFilename {DATA_FILENAME};
    vector<int> threadList = { 1, 2, 4, 8, 16, 32, 48, 64 };         // For the laptop or AWS c5.9xlarge
    vector<long> swapsPerTxList = { 1, 4, 8, 16, 32, 64, 128, 256 }; // Number of replaced objects per transaction
    const int numRuns = 1;                                           // 5 runs for the paper
    const seconds testLength = 10s;                                  // 20s for the paper
    const int EMAX_CLASS = 10;
    uint64_t results[EMAX_CLASS][threadList.size()][swapsPerTxList.size()];
    std::string cNames[EMAX_CLASS];
    int maxClass = 0;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*threadList.size()*swapsPerTxList.size());

    // SPS Objects Benchmarks multi-threaded
    std::cout << "This benchmark takes about " << (threadList.size()*swapsPerTxList.size()*numRuns*testLength.count()*3./(60*60)) << " hours to complete\n";
    std::cout << "\n----- SPS Benchmark (multi-threaded object array replace) -----\n";
    for (int ithread = 0; ithread < threadList.size(); ithread++) {
        int nThreads = threadList[ithread];
        for (int iswaps = 0; iswaps < swapsPerTxList.size(); iswaps++) {
            int nWords = swapsPerTxList[iswaps];
            int ic = 0;
            BenchmarkSPS bench(nThreads);
            std::cout << "\n----- threads=" << nThreads << "   runs=" << numRuns << "   length=" << testLength.count() << "s   arraySize=" << arraySize << "   swaps/tx=" << nWords << " -----\n";
#if defined USE_TL2
            results[ic][ithread][iswaps] = bench.benchmarkSPSObject<tl2stm::TL2STM,tl2stm::tmtype,tl2stm::tmbase>            (cNames[ic], testLength, nWords, numRuns);
            ic++;
#elif defined USE_TINY
            // TinySTM starves out and blocks forever when there is too much contention
            if ((nThreads >= 16 && nWords >= 128) || (nThreads >= 32 && nWords >= 32) || nThreads >= 64) {
                cNames[ic] = tinystm::TinySTM::className();
                results[ic][ithread][iswaps] = 0;
            } else {
                results[ic][ithread][iswaps] = bench.benchmarkSPSObject<tinystm::TinySTM,tinystm::tmtype,tinystm::tmbase>        (cNames[ic], testLength, nWords, numRuns);
            }
            ic++;
#else
            results[ic][ithread][iswaps] = bench.benchmarkSPSObject<oflf::OneFileLF,oflf::tmtype,oflf::tmbase>(cNames[ic], testLength, nWords, numRuns);
            ic++;
            results[ic][ithread][iswaps] = bench.benchmarkSPSObject<ofwf::OneFileWF,ofwf::tmtype,ofwf::tmbase>(cNames[ic], testLength, nWords, numRuns);
            ic++;
            // ESTM starves out and blocks forever when there is too much contention
            if ((nThreads >= 16 && nWords >= 128) || (nThreads >= 32 && nWords >= 32) || nThreads >= 64) {
                cNames[ic] = estm::ESTM::className();
                results[ic][ithread][iswaps] = 0;
            } else {
                results[ic][ithread][iswaps] = bench.benchmarkSPSObject<estm::ESTM,estm::tmtype,estm::tmbase>                (cNames[ic], testLength, nWords, numRuns);
            }
            ic++;
            //results[iclass][ithread][iswaps] = bench.benchmarkSPSObject<crwwpstm::CRWWPSTM,crwwpstm::tmtype,crwwpstm::tmtbase>    (cNames[iclass], testLength, nWords, numRuns);
            //iclass++;
#endif
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
