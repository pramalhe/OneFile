/* 
 * File:   rq_debugging.h
 * Author: trbot
 *
 * Created on May 15, 2017, 5:23 PM
 */

#ifndef RQ_DEBUGGING_H
#define RQ_DEBUGGING_H

#ifndef RQ_DEBUGGING_MAX_KEYS_PER_NODE
    #define RQ_DEBUGGING_MAX_KEYS_PER_NODE 32
#endif

#if !defined USE_RQ_DEBUGGING

    #define DEBUG_INIT_RQPROVIDER(x)
    #define DEBUG_VALIDATE_RQ(x)
    #define DEBUG_DEINIT_RQPROVIDER(x)
    #define DEBUG_INIT_THREAD(x)
    #define DEBUG_DEINIT_THREAD(x)
    #define DEBUG_RECORD_RQ_VISITED //
    #define DEBUG_RECORD_RQ_SIZE //
    #define DEBUG_RECORD_RQ_CHECKSUM //

#else

    #include <cassert>
    #include <string>
    #include <iostream>
    using namespace std;
    
    #define MAX_NUM_RQ_IN_EXECUTION (1<<20)

    #ifdef RQ_VISITED_IN_BAGS_HISTOGRAM
        #include <sstream>
        string twoDigits(int x) {
            stringstream ss;
            if (x >= 0 && x < 10) {
                ss<<"0";
            }
            ss<<x;
            return ss.str();
        }

        template <typename T>
        void printLogarithmicHistogram(T * valuesOverTime, int numValues) {
            constexpr int numBits = sizeof(T)*8;
            int histogram[numBits+1];
            memset(histogram, 0, sizeof(histogram));
            T sum = 0;
            int cntNonZero = 0;
            for (int i=0;i<numValues;++i) {
                T v = valuesOverTime[i];
                if (v == 0) continue;
                sum += v;
                ++cntNonZero;
                int pow2 = 0;
                while (v > 1) {
                    v >>= 1;
                    ++pow2;
                }
                assert(pow2 <= numBits);
                ++histogram[pow2];
            }
            for (int i=0;i<=numBits;++i) {
                if (histogram[i] > 0) {
                    cout<<"    (2^"<<twoDigits(i)<<", 2^"<<twoDigits(i+1)<<"]: "<<histogram[i]<<endl;
                }
            }
            cout<<"    average = "<<(sum / (double) cntNonZero)<<endl;
        }

        int ** threadNumNodesVisitedInBags; //[MAX_TID_POW2][MAX_NUM_RQ_IN_EXECUTION];
    #endif

    #ifdef RQ_VALIDATION
        #include <cstring>
        #include "errors.h"
        #define NO_RQ_CHECKSUM (0)
        long long ** threadUpdateChecksum; //[MAX_TID_POW2][MAX_NUM_RQ_IN_EXECUTION];
        long long ** threadRQChecksum; //[MAX_TID_POW2][MAX_NUM_RQ_IN_EXECUTION];
    #endif

    #ifdef RQ_HISTOGRAM
        #include <fstream>
        #define CSV_OUTPUT_FILE "data.csv"
        std::ofstream ofs;

        #define MAX_RQ_SIZE (1<<16)
        __thread int numRQs[MAX_RQ_SIZE+1];
        int totalNumRQs[MAX_RQ_SIZE+1];
    #endif

        inline void DEBUG_RECORD_RQ_VISITED(const int tid, const long long ts, const int numVisited) {
    #ifdef RQ_VISITED_IN_BAGS_HISTOGRAM
            if (ts >= MAX_NUM_RQ_IN_EXECUTION) return;
            threadNumNodesVisitedInBags[tid][ts] = numVisited;
    #endif
        }

        inline void DEBUG_RECORD_RQ_SIZE(const int size) {
    #ifdef RQ_HISTOGRAM
            ++numRQs[size];
    #endif
        }

        template <typename K, typename V, typename Node, class DataStructure>
        inline void DEBUG_RECORD_UPDATE_CHECKSUM(const int tid, const long long timestamp, Node * const * const insertedNodes, Node * const * const deletedNodes, DataStructure * const ds) {
    #ifdef RQ_VALIDATION
            if (timestamp >= MAX_NUM_RQ_IN_EXECUTION) {
                return;
    //            cout << "timestamp is: " << timestamp << endl;
    //            error("timestamp > MAX_NUM_RQ_IN_EXECUTION");
            }
            for (int i=0;insertedNodes[i];++i) {
                K outputKeys[RQ_DEBUGGING_MAX_KEYS_PER_NODE];
                V outputValues[RQ_DEBUGGING_MAX_KEYS_PER_NODE];
                int cnt = ds->getKeys(tid, insertedNodes[i], outputKeys, outputValues);
                assert(cnt <= RQ_DEBUGGING_MAX_KEYS_PER_NODE);
                for (int j=0;j<cnt;++j) {
                    threadUpdateChecksum[tid][timestamp] += outputKeys[j];
                }
            }
            for (int i=0;deletedNodes[i];++i) {
                K outputKeys[RQ_DEBUGGING_MAX_KEYS_PER_NODE];
                V outputValues[RQ_DEBUGGING_MAX_KEYS_PER_NODE];
                int cnt = ds->getKeys(tid, deletedNodes[i], outputKeys, outputValues);
                assert(cnt <= RQ_DEBUGGING_MAX_KEYS_PER_NODE);
                for (int j=0;j<cnt;++j) {
                    threadUpdateChecksum[tid][timestamp] -= outputKeys[j];
                }
            }
    #endif
        }

        template <typename K>
        inline void DEBUG_RECORD_RQ_CHECKSUM(const int tid, const long long timestamp, K const * const rqResult, const int len) {
    #ifdef RQ_VALIDATION
            if (timestamp >= MAX_NUM_RQ_IN_EXECUTION) return;
            //if (timestamp >= MAX_NUM_RQ_IN_EXECUTION) error("timestamp > MAX_NUM_RQ_IN_EXECUTION");
            // compute checksum
            long long checksum = 0;
            for (int i=0;i<len;++i) {
                checksum += (long long) rqResult[i];
            }
            threadRQChecksum[tid][timestamp] = checksum;
    #endif
        }

        void DEBUG_INIT_RQPROVIDER(const int numProcesses) {
    #ifdef RQ_HISTOGRAM
            ofs.open(CSV_OUTPUT_FILE, std::ofstream::out);
            for (int size=0;size<=MAX_RQ_SIZE;++size) {
                totalNumRQs[size] = 0;
            }
    #endif
    #ifdef RQ_VALIDATION
            threadUpdateChecksum = new long long * [numProcesses];
            threadRQChecksum = new long long * [numProcesses];
    #ifdef RQ_VISITED_IN_BAGS_HISTOGRAM
            threadNumNodesVisitedInBags = new int * [numProcesses];
    #endif
            for (int tid=0;tid<numProcesses;++tid) {
                threadUpdateChecksum[tid] = new long long[MAX_NUM_RQ_IN_EXECUTION];
                threadRQChecksum[tid] = new long long[MAX_NUM_RQ_IN_EXECUTION];
    #ifdef RQ_VISITED_IN_BAGS_HISTOGRAM
                threadNumNodesVisitedInBags[tid] = new int[MAX_NUM_RQ_IN_EXECUTION];
    #endif
                memset(threadUpdateChecksum[tid], 0, MAX_NUM_RQ_IN_EXECUTION*sizeof(threadUpdateChecksum[tid][0]));
                memset(threadRQChecksum[tid], 0, MAX_NUM_RQ_IN_EXECUTION*sizeof(threadRQChecksum[tid][0]));
    #ifdef RQ_VISITED_IN_BAGS_HISTOGRAM
                memset(threadNumNodesVisitedInBags[tid], 0, MAX_NUM_RQ_IN_EXECUTION*sizeof(threadNumNodesVisitedInBags[tid][0]));
    #endif
            }
    #endif
        }

        void DEBUG_VALIDATE_RQ(const int numProcesses) {
    #ifdef RQ_VALIDATION
            long long * updateChecksum = new long long[MAX_NUM_RQ_IN_EXECUTION];
            long long * rqChecksum = new long long[MAX_NUM_RQ_IN_EXECUTION];
            int * numNodesVisitedInBags = new int[MAX_NUM_RQ_IN_EXECUTION];
            memset(updateChecksum, 0, MAX_NUM_RQ_IN_EXECUTION*sizeof(updateChecksum[0]));
            memset(rqChecksum, 0, MAX_NUM_RQ_IN_EXECUTION*sizeof(rqChecksum[0]));
            memset(numNodesVisitedInBags, 0, MAX_NUM_RQ_IN_EXECUTION*sizeof(numNodesVisitedInBags[0]));

            for (int tid=0;tid<numProcesses;++tid) {
                for (int timestamp=0;timestamp<MAX_NUM_RQ_IN_EXECUTION;++timestamp) {
                    //if (threadUpdateChecksum[tid][timestamp]) cout<<"threadUpdateChecksum[tid="<<tid<<", timestamp="<<timestamp<<"]="<<threadUpdateChecksum[tid][timestamp]<<endl;
                    rqChecksum[timestamp] += threadRQChecksum[tid][timestamp];
                    updateChecksum[timestamp] += threadUpdateChecksum[tid][timestamp];
    #ifdef RQ_VISITED_IN_BAGS_HISTOGRAM
                    numNodesVisitedInBags[timestamp] += threadNumNodesVisitedInBags[tid][timestamp];
    #endif
                }
            } // note: since each rq gets a unique timestamp, rqChecksum only contains one RQ per timestamp, even after summing over all threads

            bool good = true;
            int numberFailed = 0;
            int numberSucc = 0;
    #ifndef RLU_USED
            long long prefixSum = 0; 
            for (int timestamp=0;timestamp<MAX_NUM_RQ_IN_EXECUTION;++timestamp) {
                if (rqChecksum[timestamp] != NO_RQ_CHECKSUM) {
                    if (rqChecksum[timestamp] != prefixSum) {
                        ++numberFailed;
                        if (numberFailed < 100) {
                            cout<<"RQ VALIDATION ERROR: rqChecksum[timestamp="<<timestamp<<"]="<<rqChecksum[timestamp]<<" is not equal to prefixSum=updateChecksum[0, 1, ..., timestamp-1]="<<prefixSum<<endl;
                        } else if (numberFailed == 100) {
                            cout<<"RQ VALIDATION: too many errors to list..."<<endl;
                        }
                        good = false;
                        //exit(-1);
                    } else {
                        ++numberSucc;
                    }
                }
                prefixSum += updateChecksum[timestamp];
            }
    #else       
            for (int tid=0;tid<numProcesses;++tid) {
                long long prefixSum = 0;
                for (int timestamp=0;timestamp<MAX_NUM_RQ_IN_EXECUTION;++timestamp) {
                    if (threadRQChecksum[tid][timestamp] != NO_RQ_CHECKSUM) {
                        if (threadRQChecksum[tid][timestamp] != prefixSum) {
                            ++numberFailed;
                            if (numberFailed < 100) {
                                cout<<"RQ VALIDATION ERROR: threadRQChecksum[tid="<< tid <<"][timestamp="<<timestamp<<"]="<<threadRQChecksum[tid][timestamp]<<" is not equal to prefixSum=updateChecksum[0, 1, ..., timestamp-1]="<<prefixSum<<endl;
                            } else if (numberFailed == 100) {
                                cout<<"RQ VALIDATION: too many errors to list..."<<endl;
                            }
                            good = false;
                            //exit(-1);
                        } else {
                            ++numberSucc;
                        }
                    }
                    prefixSum += updateChecksum[timestamp];
                }
            }
    #endif
            if (numberFailed > 0) {
                cout<<"RQ VALIDATION TOTAL FAILURES: "<<numberFailed<<endl;
                cout<<"    (note: validation only works for RQs over the entire data structure)"<<endl;
            }
            cout<<"RQ VALIDATION TOTAL SUCCESSES: "<<numberSucc<<endl;
            cout<<"    (note: this captures only non-empty RQs, and is at most "<<MAX_NUM_RQ_IN_EXECUTION<<")"<<endl;
            if (good) cout<<"RQ Validation OK"<<endl;
            cout<<endl;

    #ifdef RQ_VISITED_IN_BAGS_HISTOGRAM
            cout<<"histogram: how many RQs visited x nodes in limbo bags?"<<endl;
            printLogarithmicHistogram(numNodesVisitedInBags, MAX_NUM_RQ_IN_EXECUTION);
            cout<<endl;
    #endif

            delete[] updateChecksum;
            delete[] rqChecksum;
            delete[] numNodesVisitedInBags;
    #endif
        }

        void DEBUG_DEINIT_RQPROVIDER(const int numProcesses) {
    #ifdef RQ_HISTOGRAM
            ofs<<"x,y"<<endl;
            for (int size=0;size<=MAX_RQ_SIZE;++size) {
                if (totalNumRQs[size]) {
                    ofs<<size<<","<<totalNumRQs[size]<<endl;
                }
            }
            ofs.close();
            long long __sum = 0;
            for (int size=0;size<=MAX_RQ_SIZE;++size) {
                __sum += totalNumRQs[size];
            }
    #endif
    #ifdef RQ_VALIDATION
            DEBUG_VALIDATE_RQ(numProcesses);
            for (int tid=0;tid<numProcesses;++tid) {
                delete[] threadUpdateChecksum[tid];
                delete[] threadRQChecksum[tid];
    #ifdef RQ_VISITED_IN_BAGS_HISTOGRAM
                delete[] threadNumNodesVisitedInBags[tid];
    #endif
            }
            delete[] threadUpdateChecksum;
            delete[] threadRQChecksum;
    #ifdef RQ_VISITED_IN_BAGS_HISTOGRAM
            delete[] threadNumNodesVisitedInBags;
    #endif
    #endif
        }

        void DEBUG_INIT_THREAD(const int tid) {
    #ifdef RQ_HISTOGRAM
            for (int size=0;size<=MAX_RQ_SIZE;++size) {
                numRQs[size] = 0;
            }
    #endif
        }

        void DEBUG_DEINIT_THREAD(const int tid) {
    #ifdef RQ_HISTOGRAM
            for (int size=0;size<=MAX_RQ_SIZE;++size) {
                __sync_fetch_and_add(&totalNumRQs[size], numRQs[size]);
            }
    #endif
        }

#endif // else case for "if !defined USE_RQ_DEBUGGING"
    
#endif /* RQ_DEBUGGING_H */