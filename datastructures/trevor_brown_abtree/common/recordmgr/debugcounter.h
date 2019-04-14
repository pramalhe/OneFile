/* 
 * File:   debugcounter.h
 * Author: trbot
 *
 * Created on September 27, 2015, 4:43 PM
 */

#ifndef DEBUGCOUNTER_H
#define	DEBUGCOUNTER_H

#include <string>
#include <sstream>
#include "plaf.h"
using namespace std;

class debugCounter {
private:
    const int NUM_PROCESSES;
    volatile long long * data; // data[tid*PREFETCH_SIZE_WORDS] = count for thread tid (padded to avoid false sharing)
public:
    void add(const int tid, const long long val) {
        data[tid*PREFETCH_SIZE_WORDS] += val;
    }
    void inc(const int tid) {
        add(tid, 1);
    }
    long long get(const int tid) {
        return data[tid*PREFETCH_SIZE_WORDS];
    }
    long long getTotal() {
        long long result = 0;
        for (int tid=0;tid<NUM_PROCESSES;++tid) {
            result += get(tid);
        }
        return result;
    }
    void clear() {
        for (int tid=0;tid<NUM_PROCESSES;++tid) {
            data[tid*PREFETCH_SIZE_WORDS] = 0;
        }
    }
    debugCounter(const int numProcesses) : NUM_PROCESSES(numProcesses) {
        data = new long long[numProcesses*PREFETCH_SIZE_WORDS];
        clear();
    }
    ~debugCounter() {
        delete[] data;
    }
};

#endif	/* DEBUGCOUNTER_H */

