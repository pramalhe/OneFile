/* 
 * File:   dcss_plus.h
 * Author: Maya Arbel-Raviv
 *
 * Created on May 1, 2017, 10:42 AM
 */

#ifndef DCSS_PLUS_H
#define DCSS_PLUS_H

#include <cstdarg>
#include <csignal>
#include <string.h>
#include "descriptors.h"

#define dcssptagptr_t uintptr_t
#define dcsspptr_t dcsspdesc_t<PAYLOAD_T> *
#define casword_t intptr_t

#define DCSSP_STATE_UNDECIDED 0
#define DCSSP_STATE_SUCCEEDED 4
#define DCSSP_STATE_FAILED 8

#define DCSSP_LEFTSHIFT 1

#define DCSSP_IGNORED_RETVAL -1
#define DCSSP_SUCCESS 0
#define DCSSP_FAILED_ADDR1 1 
#define DCSSP_FAILED_ADDR2 2

#define MAX_PAYLOAD_PTRS 6

struct dcsspresult_t {
    int status;
    casword_t failed_val;
};

template <typename PAYLOAD_T>
class dcsspdesc_t {
public:
    volatile mutables_t mutables;
    casword_t volatile * volatile addr1;
    casword_t volatile old1;
    casword_t volatile * volatile addr2;
    casword_t volatile old2;
    casword_t volatile new2;
    PAYLOAD_T volatile payload1[MAX_PAYLOAD_PTRS+1];
    PAYLOAD_T volatile payload2[MAX_PAYLOAD_PTRS+1];
    const static int size = sizeof(mutables)+sizeof(addr1)+sizeof(old1)+sizeof(addr2)+sizeof(old2)+sizeof(new2)+sizeof(PAYLOAD_T)*(MAX_PAYLOAD_PTRS+1)+sizeof(PAYLOAD_T)*(MAX_PAYLOAD_PTRS+1);
    char padding[PREFETCH_SIZE_BYTES+(((64<<10)-size%64)%64)]; // add padding to prevent false sharing
} __attribute__ ((aligned(64)));

template <typename PAYLOAD_T>
class dcsspProvider {
    /**
     * Data definitions
     */
private:
    // descriptor reduction algorithm
    #define DCSSP_MUTABLES_OFFSET_STATE 0
    #define DCSSP_MUTABLES_MASK_STATE 0xf
    #define DCSSP_MUTABLES_NEW(mutables) \
        ((((mutables)&MASK_SEQ)+(1<<OFFSET_SEQ)) \
        | (DCSSP_STATE_UNDECIDED<<DCSSP_MUTABLES_OFFSET_STATE))
    #include "descriptors_impl2.h"
    char __padding_desc[PREFETCH_SIZE_BYTES];
    dcsspdesc_t<PAYLOAD_T> dcsspDescriptors[LAST_TID+1] __attribute__ ((aligned(64)));
    char __padding_desc3[PREFETCH_SIZE_BYTES];

public:
#ifdef USE_DEBUGCOUNTERS
    debugCounter * dcsspHelpCounter;
#endif
    const int NUM_PROCESSES;
    
    /**
     * Function declarations
     */
    dcsspProvider(const int numProcesses);
    ~dcsspProvider();
    void initThread(const int tid);
    void deinitThread(const int tid);
    void writePtr(casword_t volatile * addr, casword_t val);        // use for addresses that might have been modified by DCSSP (ONLY GOOD FOR INITIALIZING, CANNOT DEAL WITH CONCURRENT DCSSP OPERATIONS.)
    void writeVal(casword_t volatile * addr, casword_t val);        // use for addresses that might have been modified by DCSSP (ONLY GOOD FOR INITIALIZING, CANNOT DEAL WITH CONCURRENT DCSSP OPERATIONS.)
    casword_t readPtr(const int tid, casword_t volatile * addr);    // use for addresses that might have been modified by DCSSP
    casword_t readVal(const int tid, casword_t volatile * addr);    // use for addresses that might have been modified by DCSSP
    inline dcsspresult_t dcsspPtr(const int tid, casword_t * addr1, casword_t old1, casword_t * addr2, casword_t old2, casword_t new2, PAYLOAD_T * const payload1, PAYLOAD_T * const payload2); // use when addr2 is a pointer, or another type that does not use its least significant bit
    inline dcsspresult_t dcsspVal(const int tid, casword_t * addr1, casword_t old1, casword_t * addr2, casword_t old2, casword_t new2, PAYLOAD_T * const payload1, PAYLOAD_T * const payload2); // use when addr2 uses its least significant bit, but does not use its most significant but
    void discardPayloads(const int tid);
    void debugPrint();
    
    tagptr_t getDescriptorTagptr(const int otherTid);
    dcsspptr_t getDescriptorPtr(tagptr_t tagptr);
    bool getDescriptorSnapshot(tagptr_t tagptr, dcsspptr_t const dest);
    void helpProcess(const int tid, const int otherTid);
private:
    casword_t dcsspRead(const int tid, casword_t volatile * addr);
    inline dcsspresult_t dcsspHelp(const int tid, dcssptagptr_t tagptr, dcsspptr_t snapshot, bool helpingOther);
    void dcsspHelpOther(const int tid, dcssptagptr_t tagptr);
};

#endif /* DCSS_PLUS_H */

