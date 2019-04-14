#include <cstdlib>
#include <cmath>
#include <iostream>
#include "dcss_plus_impl.h"

using namespace std;

#define NUM_OPS 10000000
#define INCREMENT 1

#define FALSE_SHARING_ULL_FACTOR 24
#define FALSE_SHARING_PAD_BYTES 192

#define COUNTER(tid) (counters[(tid)*FALSE_SHARING_ULL_FACTOR])

int numProcesses = 0;
volatile unsigned long long counters[MAX_TID_POW2*FALSE_SHARING_ULL_FACTOR];
volatile char padding[FALSE_SHARING_PAD_BYTES];
volatile unsigned long long faa;

volatile bool start;
volatile int running; // number of threads that are running
dcsspProvider * prov;

#ifndef KERNEL
#define KERNEL test_kernel1
#endif

#ifndef VALIDATE
#define VALIDATE validate1
#endif

//#define GET_FAA_FOR_TID(tid) ((faa >> ((tid)*(62/numProcesses))) & (numProcesses == 1 ? 0xffffffffffffffffULL : ((1ULL<<(62/numProcesses))-1)))

void * test_kernel1(void * arg) {
    const int tid = *((int *) arg);
    //const unsigned long long numOps = min(1ULL<<20, 1ULL<<(62/numProcesses)-1);
    //const unsigned long long increment = 1ULL<<(tid*(62/numProcesses));
    prov->initThread(tid);
    __sync_fetch_and_add(&running, 1);
    while (!start) { __sync_synchronize(); }
    
    //COUTATOMICTID("performing "<<numOps<<" operations"<<endl);
    int numSucc = 0;
    while (numSucc < NUM_OPS) {
#if 1
        void * deletedNodes[] = {NULL};
        casword_t oldval = (casword_t) prov->readVal(tid,(casword_t*)&(COUNTER(tid)));
        casword_t newval = (casword_t) oldval+1;
        if (DCSSP_SUCCESS == prov->dcsspVal(tid, (casword_t *) &faa, (casword_t) faa, (casword_t *) &COUNTER(tid), oldval, newval, deletedNodes)) {
            ++numSucc;
            __sync_fetch_and_add(&faa, INCREMENT);
        }
#else
        ++numSucc;
        ++COUNTER(tid);
        __sync_fetch_and_add(&faa, INCREMENT);
#endif
    }
    
    prov->deinitThread(tid);
}

bool validate1() {
    // compute checksum
    bool good = true;
    for (int i=0;i<numProcesses;++i) {
        unsigned long long c = prov->readVal(i,(casword_t*)&(COUNTER(i)));
        if (c != NUM_OPS) {
            cout<<"ERROR: counters["<<i<<"]="<<c<<" does not match NUM_OPS="<<NUM_OPS<<endl;
            good = false;
        } else {
            cout<<"thread "<<i<<": counter="<<c<<" NUM_OPS="<<NUM_OPS<<endl;
        }

//        if (c != GET_FAA_FOR_TID(i)) {
//            cout<<"ERROR: counters["<<i<<"]="<<c<<" does not match FAA subword="<<(GET_FAA_FOR_TID(i))<<endl;
//            good = false;
//        }
//        cout<<"thread "<<i<<": counter="<<c<<" faa="<<(GET_FAA_FOR_TID(i))<<endl;
    }
    
    const unsigned long long f = faa;
    if (f != NUM_OPS * numProcesses) {
        cout<<"ERROR: faa="<<f<<" does not match NUM_OPS*numProcesses="<<(NUM_OPS*numProcesses)<<endl;
        good = false;
    } else {
        cout<<"faa="<<f<<" and NUM_OPS*numProcesses="<<(NUM_OPS*numProcesses)<<endl;
    }
    return good;
}

void * test_kernel2(void * arg) {
    const int tid = *((int *) arg);
    prov->initThread(tid);
    __sync_fetch_and_add(&running, 1);
    while (!start) { __sync_synchronize(); }
    
    //COUTATOMICTID("performing "<<numOps<<" operations"<<endl);
    int numSucc = 0;
    while (numSucc < NUM_OPS) {
        void * deletedNodes[] = {NULL};
        casword_t old1 = (casword_t) COUNTER((tid+1)%numProcesses);
        casword_t old2 = (casword_t) prov->readVal(tid, (casword_t *) &faa);
        casword_t new2 = (casword_t) old2+1;
        if (DCSSP_SUCCESS == prov->dcsspVal(tid, (casword_t *) &COUNTER((tid+1)%numProcesses), old1, (casword_t *) &faa, old2, new2, deletedNodes)) {
            ++numSucc;
            ++COUNTER(tid);
        }
    }
    prov->deinitThread(tid);
}

bool validate2() {
    // compute checksum
    bool good = true;
    for (int i=0;i<numProcesses;++i) {
        unsigned long long c = COUNTER(i);
        if (c != NUM_OPS) {
            cout<<"ERROR: counters["<<i<<"]="<<c<<" does not match NUM_OPS="<<NUM_OPS<<endl;
            good = false;
        } else {
            cout<<"thread "<<i<<": counter="<<c<<" NUM_OPS="<<NUM_OPS<<endl;
        }
    }
    
    const int tid = 0;
    const unsigned long long f = prov->readVal(tid, (casword_t *) &faa);
    if (f != NUM_OPS * numProcesses) {
        cout<<"ERROR: faa="<<f<<" does not match NUM_OPS*numProcesses="<<(NUM_OPS*numProcesses)<<endl;
        good = false;
    } else {
        cout<<"faa="<<f<<" and NUM_OPS*numProcesses="<<(NUM_OPS*numProcesses)<<endl;
    }
    return good;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        cout<<"Usage: "<<argv[0]<<" NUM_THREADS"<<endl;
        exit(-1);
    }
    numProcesses = atoi(argv[1]);
    
    // create threads
    const int tid = 0; // dummy tid for main thread
    pthread_t *threads[numProcesses];
    int ids[numProcesses];
    for (int i=0;i<numProcesses;++i) {
        threads[i] = new pthread_t;
        ids[i] = i;
        COUNTER(i) = 0;
    }
    
    // init data structure
    faa = 0;
    for (int i=0;i<numProcesses;++i) {
        COUNTER(i) = 0;
    }
    prov = new dcsspProvider(numProcesses);

    // start all threads
    running = 0;
    start = false;
    __sync_synchronize();
    for (int i=0;i<numProcesses;++i) {
        if (pthread_create(threads[i], NULL, KERNEL, &ids[i])) {
            cerr<<"ERROR: could not create thread"<<endl;
            exit(-1);
        }
    }
    while (running < numProcesses) {
        TRACE COUTATOMIC("main thread: waiting for threads to START running="<<running<<endl);
        __sync_synchronize();
    } // wait for all threads to be ready
    COUTATOMIC("main thread: starting trial..."<<endl);
    __sync_synchronize();
    start = true;
    __sync_synchronize();
    
    // join all threads
    for (int i=0;i<numProcesses;++i) {
//        COUTATOMIC("joining thread "<<i<<endl);
        if (pthread_join(*(threads[i]), NULL)) {
            cerr<<"ERROR: could not join thread"<<endl;
            exit(-1);
        }
    }
    
    if (VALIDATE()) {
        COUTATOMIC("main thread: "<<"All tests passed."<<endl);
    } else {
        COUTATOMIC("main thread: "<<"ERROR occurred."<<endl);
    }
    
    delete prov;
    
    return 0;
}

