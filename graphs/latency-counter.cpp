#include <iostream>
#include <fstream>
#include <cstring>
#include "BenchmarkLatencyCounter.hpp"
#include "stms/OneFileLF.hpp"
#include "stms/OneFileWF.hpp"
#if defined USE_TINY
#include "stms/TinySTM.hpp"
#define DATA_FILENAME "data/latency-counter-tiny.txt"
#else
#include "stms/ESTM.hpp"
#define DATA_FILENAME "data/latency-counter.txt"
#endif



int main(void) {
    const std::string dataFilename {DATA_FILENAME};
    vector<int> threadList = { 1, 2, 4, 8 };             // For the laptop
    //vector<int> threadList = { 1, 2, 4, 8, 16, 32, 62 };             // For the cervino
    const int EMAX_CLASS = 10;
    BenchmarkLatencyCounter::Result results[EMAX_CLASS][threadList.size()];
    std::string cNames[EMAX_CLASS];
    int maxClass = 0;
    // Reset results
    std::memset(results, 0, sizeof(uint64_t)*EMAX_CLASS*threadList.size());

    for (unsigned it = 0; it < threadList.size(); it++) {
        auto nThreads = threadList[it];
        int ic = 0;
        BenchmarkLatencyCounter bench(nThreads);
        std::cout << "\n----- Latency Counter    nThreads=" << nThreads << " -----\n";
#if defined USE_TINY
        results[ic][it] = bench.latencyBenchmark<tinystm::TinySTM,tinystm::tmtype>(cNames[ic]);
        ic++;
#else
        results[ic][it] = bench.latencyBenchmark<oflf::OneFileLF,oflf::tmtype>(cNames[ic]);
        ic++;
        results[ic][it] = bench.latencyBenchmark<ofwf::OneFileWF,ofwf::tmtype>(cNames[ic]);
        ic++;
        results[ic][it] = bench.latencyBenchmark<estm::ESTM,estm::tmtype>(cNames[ic]);
        ic++;
#endif
        maxClass = ic;
     }

    // Export tab-separated values to a file to be imported in gnuplot or excel
    ofstream dataFile;
    dataFile.open(dataFilename);
    dataFile << "Threads\t";
    // Printf class names and percentiles for each column
    for (int ic = 0; ic < maxClass; ic++) {
        dataFile << cNames[ic] << "-50%"<< "\t";
        dataFile << cNames[ic] << "-90%"<< "\t";
        dataFile << cNames[ic] << "-99%"<< "\t";
        dataFile << cNames[ic] << "-99.9%"<< "\t";
        dataFile << cNames[ic] << "-99.99%"<< "\t";
        dataFile << cNames[ic] << "-99.999%"<< "\t";
    }
    dataFile << "\n";
    for (int it = 0; it < threadList.size(); it++) {
        dataFile << threadList[it] << "\t";
        for (int ic = 0; ic < maxClass; ic++) {
            dataFile << results[ic][it].delay50000 << "\t";
            dataFile << results[ic][it].delay90000 << "\t";
            dataFile << results[ic][it].delay99000 << "\t";
            dataFile << results[ic][it].delay99900 << "\t";
            dataFile << results[ic][it].delay99990 << "\t";
            dataFile << results[ic][it].delay99999 << "\t";
        }
        dataFile << "\n";
    }
    dataFile.close();
    std::cout << "\nSuccessfuly saved results in " << dataFilename << "\n";

    return 0;
}
