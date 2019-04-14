/* 
 * File:   dcss_plus_impl.h
 * Author: Maya Arbel-Raviv
 *
 * Created on May 1, 2017, 10:52 AM
 */

#ifndef DCSS_PLUS_IMPL_H
#define DCSS_PLUS_IMPL_H

#include "dcss_plus.h"
#include <cassert>
#include <stdint.h>
#include <sstream>
using namespace std;

#define BOOL_CAS __sync_bool_compare_and_swap
#define VAL_CAS __sync_val_compare_and_swap

#define DCSSP_TAGBIT 0x1

static bool isDcssp(casword_t val) {
    return (val & DCSSP_TAGBIT);
}

template <typename PAYLOAD_T>
dcsspresult_t dcsspProvider<PAYLOAD_T>::dcsspHelp(const int tid, dcssptagptr_t tagptr, dcsspptr_t snapshot, bool helpingOther) {
    // figure out what the state should be
    casword_t state = DCSSP_STATE_FAILED;

    SOFTWARE_BARRIER;
    casword_t val1 = *(snapshot->addr1);
    SOFTWARE_BARRIER;
    
    //DELAY_UP_TO(1000);
    if (val1 == snapshot->old1) { // linearize here(?)
        state = DCSSP_STATE_SUCCEEDED;
    }
    
    // try to cas the state to the appropriate value
    dcsspptr_t ptr = TAGPTR_UNPACK_PTR(dcsspDescriptors,tagptr);
    casword_t retval;
    bool failedBit;
    MUTABLES_VAL_CAS_FIELD(failedBit, retval, ptr->mutables, snapshot->mutables, DCSSP_STATE_UNDECIDED, state, DCSSP_MUTABLES_MASK_STATE, DCSSP_MUTABLES_OFFSET_STATE); 
    if (failedBit) return {DCSSP_IGNORED_RETVAL,0};                             // failed to access the descriptor: we must be helping another process complete its operation, so we will NOT use this return value!
    
    // TODO: do we do the announcement here? what will be announced exactly? do we let the user provide a pointer/value to announce as an argument to dcssp? do we need to provide an operation to retrieve the current announcement for a given process?
    
    // finish the operation based on the descriptor's state
    if ((retval == DCSSP_STATE_UNDECIDED && state == DCSSP_STATE_SUCCEEDED)     // if we changed the state to succeeded OR
      || retval == DCSSP_STATE_SUCCEEDED) {                                     // if someone else changed the state to succeeded
//        if (state == DCSSP_STATE_FAILED) DELAY_UP_TO(1000);
        assert(helpingOther || ((snapshot->mutables & DCSSP_MUTABLES_MASK_STATE) >> DCSSP_MUTABLES_OFFSET_STATE) == DCSSP_STATE_SUCCEEDED);
        BOOL_CAS(snapshot->addr2, (casword_t) tagptr, snapshot->new2); 
        return {DCSSP_SUCCESS,0};
    } else {                                                                    // either we or someone else changed the state to failed
        assert((retval == DCSSP_STATE_UNDECIDED && state == DCSSP_STATE_FAILED)
                || retval == DCSSP_STATE_FAILED);
        assert(helpingOther || ((snapshot->mutables & DCSSP_MUTABLES_MASK_STATE) >> DCSSP_MUTABLES_OFFSET_STATE) == DCSSP_STATE_FAILED);
        BOOL_CAS(snapshot->addr2, (casword_t) tagptr, snapshot->old2);
//        if (state == DCSSP_STATE_FAILED) DELAY_UP_TO(1000);
        return {DCSSP_FAILED_ADDR1,val1};
    }
}

template <typename PAYLOAD_T>
void dcsspProvider<PAYLOAD_T>::dcsspHelpOther(const int tid, dcssptagptr_t tagptr) {
    const int otherTid = TAGPTR_UNPACK_TID(tagptr);
    assert(otherTid >= 0 && otherTid < NUM_PROCESSES);
    dcsspdesc_t<PAYLOAD_T> newSnapshot;
    const int sz = dcsspdesc_t<PAYLOAD_T>::size;
    assert((((tagptr & MASK_SEQ) >> OFFSET_SEQ) & 1) == 1);
    if (DESC_SNAPSHOT(dcsspdesc_t<PAYLOAD_T>, dcsspDescriptors, &newSnapshot, tagptr, sz)) {
        dcsspHelp(tid, tagptr, &newSnapshot, true);
    } else {
        //TRACE COUTATOMICTID("helpOther unable to get snapshot of "<<tagptrToString(tagptr)<<endl);
    }
}

template <typename PAYLOAD_T>
inline
tagptr_t dcsspProvider<PAYLOAD_T>::getDescriptorTagptr(const int otherTid) {
    dcsspptr_t ptr = &dcsspDescriptors[otherTid];
    tagptr_t tagptr = TAGPTR_NEW(otherTid, ptr->mutables, DCSSP_TAGBIT);
    if ((UNPACK_SEQ(tagptr) & 1) == 0) {
        // descriptor is being initialized! essentially,
        // we can think of there being NO ongoing operation,
        // so we can imagine we return NULL = no descriptor.
        return (tagptr_t) NULL;
    }
    return tagptr;
}

template <typename PAYLOAD_T>
inline
dcsspptr_t dcsspProvider<PAYLOAD_T>::getDescriptorPtr(tagptr_t tagptr) {
    return TAGPTR_UNPACK_PTR(dcsspDescriptors, tagptr);
}

template <typename PAYLOAD_T>
inline
bool dcsspProvider<PAYLOAD_T>::getDescriptorSnapshot(tagptr_t tagptr, dcsspptr_t const dest) {
    if (tagptr == (tagptr_t) NULL) return false;
    return DESC_SNAPSHOT(dcsspdesc_t<PAYLOAD_T>, dcsspDescriptors, dest, tagptr, dcsspdesc_t<PAYLOAD_T>::size);
}

template <typename PAYLOAD_T>
inline
void dcsspProvider<PAYLOAD_T>::helpProcess(const int tid, const int otherTid) {
    tagptr_t tagptr = getDescriptorTagptr(otherTid);
    if (tagptr != (tagptr_t) NULL) dcsspHelpOther(tid, tagptr);
}

template <typename PAYLOAD_T>
void dcsspProvider<PAYLOAD_T>::discardPayloads(const int tid) {
    SOFTWARE_BARRIER;
    dcssptagptr_t tagptr = getDescriptorTagptr(tid);
    dcsspptr_t ptr = getDescriptorPtr(tagptr);
    ptr->payload1[0] = NULL;
    ptr->payload2[0] = NULL;
    SOFTWARE_BARRIER;
}

template <typename PAYLOAD_T>
dcsspresult_t dcsspProvider<PAYLOAD_T>::dcsspVal(const int tid, casword_t * addr1, casword_t old1, casword_t * addr2, casword_t old2, casword_t new2, PAYLOAD_T * const payload1, PAYLOAD_T * const payload2) {
    return dcsspPtr(tid, addr1, old1, addr2, old2 << DCSSP_LEFTSHIFT , new2 << DCSSP_LEFTSHIFT, payload1, payload2);
}

template <typename PAYLOAD_T>
dcsspresult_t dcsspProvider<PAYLOAD_T>::dcsspPtr(const int tid, casword_t * addr1, casword_t old1, casword_t * addr2, casword_t old2, casword_t new2, PAYLOAD_T * const payload1, PAYLOAD_T * const payload2) {
    // create dcssp descriptor
    dcsspptr_t ptr = DESC_NEW(dcsspDescriptors, DCSSP_MUTABLES_NEW, tid);
    assert((((dcsspDescriptors[tid].mutables & MASK_SEQ) >> OFFSET_SEQ) & 1) == 0);
    ptr->addr1 = addr1;
    ptr->old1 = old1;
    ptr->addr2 = addr2;
    ptr->old2 = old2;
    ptr->new2 = new2;
    
    // add payload1 and payload2 to the dcssp descriptor
    int i;
    for (i=0;payload1[i];++i) {
        ptr->payload1[i] = payload1[i];
        assert(i < MAX_PAYLOAD_PTRS);
    }
    ptr->payload1[i] = NULL;
    for (i=0;payload2[i];++i) {
        ptr->payload2[i] = payload2[i];
        assert(i < MAX_PAYLOAD_PTRS);
    }
    ptr->payload2[i] = NULL;
    DESC_INITIALIZED(dcsspDescriptors, tid);
    
    // create tagptr
    assert((((dcsspDescriptors[tid].mutables & MASK_SEQ) >> OFFSET_SEQ) & 1) == 1);
    tagptr_t tagptr = TAGPTR_NEW(tid, ptr->mutables, DCSSP_TAGBIT);
    
    // perform the dcssp operation described by our descriptor
    casword_t r;
    do {
        assert(!isDcssp(ptr->old2));
        assert(isDcssp(tagptr));
        r = VAL_CAS(ptr->addr2, ptr->old2, (casword_t) tagptr);
        if (isDcssp(r)) {
#ifdef USE_DEBUGCOUNTERS
            this->dcsspHelpCounter->inc(tid);
#endif
            dcsspHelpOther(tid, (dcssptagptr_t) r);
        }
    } while (isDcssp(r));
    if (r == ptr->old2){
//        DELAY_UP_TO(1000);
        return dcsspHelp(tid, tagptr, ptr, false); // finish our own operation      
    } 
    return {DCSSP_FAILED_ADDR2,r};//DCSSP_FAILED_ADDR2;
}

template <typename PAYLOAD_T>
casword_t dcsspProvider<PAYLOAD_T>::dcsspRead(const int tid, casword_t volatile * addr) {
    casword_t r;
    while (1) {
        r = *addr;
        if (isDcssp(r)) {
#ifdef USE_DEBUGCOUNTERS
            this->dcsspHelpCounter->inc(tid);
#endif
            dcsspHelpOther(tid, (dcssptagptr_t) r);
        } else {
            return r;
        }
    }
}

template <typename PAYLOAD_T>
dcsspProvider<PAYLOAD_T>::dcsspProvider(const int numProcesses) : NUM_PROCESSES(numProcesses) {
#ifdef USE_DEBUGCOUNTERS
    dcsspHelpCounter = new debugCounter(NUM_PROCESSES);
#endif
    DESC_INIT_ALL(dcsspDescriptors, DCSSP_MUTABLES_NEW, NUM_PROCESSES);
    for (int tid=0;tid<numProcesses;++tid) {
        dcsspDescriptors[tid].addr1 = 0;
        dcsspDescriptors[tid].addr2 = 0;
        dcsspDescriptors[tid].new2 = 0;
        dcsspDescriptors[tid].old1 = 0;
        dcsspDescriptors[tid].old2 = 0;
        dcsspDescriptors[tid].payload1[0] = NULL;
        dcsspDescriptors[tid].payload2[0] = NULL;
    }
}

template <typename PAYLOAD_T>
dcsspProvider<PAYLOAD_T>::~dcsspProvider() {
#ifdef USE_DEBUGCOUNTERS
    delete dcsspHelpCounter;
#endif
}

template <typename PAYLOAD_T>
casword_t dcsspProvider<PAYLOAD_T>::readPtr(const int tid, casword_t volatile * addr) {
    casword_t r;
    r = dcsspRead(tid, addr);
    return r;
}

template <typename PAYLOAD_T>
casword_t dcsspProvider<PAYLOAD_T>::readVal(const int tid, casword_t volatile * addr) {
    return ((casword_t) readPtr(tid, addr))>>DCSSP_LEFTSHIFT;
}

template <typename PAYLOAD_T>
void dcsspProvider<PAYLOAD_T>::writePtr(casword_t volatile * addr, casword_t ptr) {
    //assert((*addr & DCSSP_TAGBIT) == 0);
    assert((ptr & DCSSP_TAGBIT) == 0);
    *addr = ptr;
}

template <typename PAYLOAD_T>
void dcsspProvider<PAYLOAD_T>::writeVal(casword_t volatile * addr, casword_t val) {
    writePtr(addr, val<<DCSSP_LEFTSHIFT);
}

template <typename PAYLOAD_T>
void dcsspProvider<PAYLOAD_T>::initThread(const int tid) {}

template <typename PAYLOAD_T>
void dcsspProvider<PAYLOAD_T>::deinitThread(const int tid) {}

template <typename PAYLOAD_T>
void dcsspProvider<PAYLOAD_T>::debugPrint() {
#ifdef USE_DEBUGCOUNTERS
    cout<<"dcssp helping : "<<this->dcsspHelpCounter->getTotal()<<endl;
#endif
}

#endif /* DCSS_PLUS_IMPL_H */
