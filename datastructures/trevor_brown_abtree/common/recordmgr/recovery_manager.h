/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef RECOVERY_MANAGER_H
#define	RECOVERY_MANAGER_H

#ifdef __CYGWIN__
    struct sigaction { void * sa_sigaction; int sa_flags; int sa_mask; };
    struct siginfo_t {};
    struct sigjmp_buf {};
    #define sigsetjmp(buf, flags) 0
    #define siglongjmp(buf, flags) 0
    #define sigemptyset(x) 0
    #define sigfillset(x) 0
    #define sigaddset(x, sig) 0
    #define pthread_sigmask(sig, set, nul) 0
    #define SIG_UNBLOCK 0
    #define SA_RESTART 0
    #define SA_SIGINFO 0
#else
    #include <setjmp.h>
#endif

#ifndef VERBOSE
    #define VERBOSE if(0)
#endif
    
#include <cassert>
#include <csignal>
#include "globals.h"
#ifdef USE_DEBUGCOUNTERS
    #include "debugcounter.h"
#endif

// for crash recovery
static pthread_key_t pthreadkey;
static struct sigaction ___act;
static void *___singleton = NULL;
extern pthread_key_t pthreadkey;
extern struct sigaction ___act;
extern void *___singleton;

static pthread_t registeredThreads[MAX_TID_POW2];
static void *errnoThreads[MAX_TID_POW2];
static sigjmp_buf *setjmpbuffers;
extern pthread_t registeredThreads[MAX_TID_POW2];
extern void *errnoThreads[MAX_TID_POW2];
extern sigjmp_buf *setjmpbuffers;

#ifdef USE_DEBUGCOUNTERS
static debugCounter countInterrupted(MAX_TID_POW2);
static debugCounter countLongjmp(MAX_TID_POW2);
extern debugCounter countInterrupted;
extern debugCounter countLongjmp;
#endif
#define MAX_THREAD_ADDR 10000

#ifdef CRASH_RECOVERY_USING_SETJMP
#define CHECKPOINT_AND_RUN_UPDATE(tid, finishedbool) \
    if (MasterRecordMgr::supportsCrashRecovery() && sigsetjmp(setjmpbuffers[(tid)], 0)) { \
        recordmgr->enterQuiescentState((tid)); \
        (finishedbool) = recoverAnyAttemptedSCX((tid), -1); \
        recordmgr->recoveryMgr->unblockCrashRecoverySignal(); \
    } else
#define CHECKPOINT_AND_RUN_QUERY(tid) \
    if (MasterRecordMgr::supportsCrashRecovery() && sigsetjmp(setjmpbuffers[(tid)], 0)) { \
        recordmgr->enterQuiescentState((tid)); \
        recordmgr->recoveryMgr->unblockCrashRecoverySignal(); \
    } else
#endif

// warning: this crash recovery code will only work if you've created a SINGLE instance of bst during an execution.
// there are ways to make it work for multiple instances; i just haven't done that.
template <class MasterRecordMgr>
void crashhandler(int signum, siginfo_t *info, void *uctx) {
    MasterRecordMgr * const recordmgr = (MasterRecordMgr * const) ___singleton;
#ifdef SIGHANDLER_IDENTIFY_USING_PTHREAD_GETSPECIFIC
    int tid = (int) ((long) pthread_getspecific(pthreadkey));
#endif
    TRACE COUTATOMICTID("received signal "<<signum<<std::endl);

    // if i'm active (not in a quiescent state), i must throw an exception
    // and clean up after myself, instead of continuing my operation.
#ifdef USE_DEBUGCOUNTERS
    DEBUG countInterrupted.inc(tid);
#endif
    __sync_synchronize();
    if (!recordmgr->isQuiescent(tid)) {
#ifdef PERFORM_RESTART_IN_SIGHANDLER
        recordmgr->enterQuiescentState(tid);
    #ifdef USE_DEBUGCOUNTERS
        DEBUG countLongjmp.inc(tid);
    #endif
        __sync_synchronize();
    #ifdef CRASH_RECOVERY_USING_SETJMP
        siglongjmp(setjmpbuffers[tid], 1);
    #endif
#endif
    }
    // otherwise, i simply continue my operation as if nothing happened.
    // this lets me behave nicely when it would be dangerous for me to be
    // restarted (being in a Q state is analogous to having interrupts 
    // disabled in an operating system kernel; however, whereas disabling
    // interrupts blocks other processes' progress, being in a Q state
    // implies that you cannot block the progress of any other thread.)
}

template <class MasterRecordMgr>
class RecoveryMgr {
public:
    const int NUM_PROCESSES;
    const int neutralizeSignal;
    
    inline int getTidInefficient(const pthread_t me) {
        int tid = -1;
        for (int i=0;i<NUM_PROCESSES;++i) {
            if (pthread_equal(registeredThreads[i], me)) {
                tid = i;
            }
        }
        // fail to find my tid -- should be impossible
        if (tid == -1) {
            COUTATOMIC("THIS SHOULD NEVER HAPPEN"<<std::endl);
            assert(false);
            exit(-1);
        }
        return tid;
    }
    inline int getTidInefficientErrno() {
        int tid = -1;
        for (int i=0;i<NUM_PROCESSES;++i) {
            // here, we use the fact that errno is defined to be a thread local variable
            if (&errno == errnoThreads[i]) {
                tid = i;
            }
        }
        // fail to find my tid -- should be impossible
        if (tid == -1) {
            COUTATOMIC("THIS SHOULD NEVER HAPPEN"<<std::endl);
            assert(false);
            exit(-1);
        }
        return tid;
    }
    inline int getTid_pthread_getspecific() {
        void * result = pthread_getspecific(pthreadkey);
        if (!result) {
            assert(false);
            COUTATOMIC("ERROR: failed to get thread id using pthread_getspecific"<<std::endl);
            exit(-1);
        }
        return (int) ((long) result);
    }
    inline pthread_t getPthread(const int tid) {
        return registeredThreads[tid];
    }
    
    void initThread(const int tid) {
        // create mapping between tid and pthread_self for the signal handler
        // and for any thread that neutralizes another
        registeredThreads[tid] = pthread_self();

        // here, we use the fact that errno is defined to be a thread local variable
        errnoThreads[tid] = &errno;
        if (pthread_setspecific(pthreadkey, (void*) (long) tid)) {
            COUTATOMIC("ERROR: failure of pthread_setspecific for tid="<<tid<<std::endl);
        }
        const long __readtid = (long) ((int *) pthread_getspecific(pthreadkey));
        VERBOSE DEBUG COUTATOMICTID("did pthread_setspecific, pthread_getspecific of "<<__readtid<<std::endl);
        assert(__readtid == tid);
    }
    
    void unblockCrashRecoverySignal() {
        __sync_synchronize();
        sigset_t oldset;
        sigemptyset(&oldset);
        sigaddset(&oldset, neutralizeSignal);
        if (pthread_sigmask(SIG_UNBLOCK, &oldset, NULL)) {
            VERBOSE COUTATOMIC("ERROR UNBLOCKING SIGNAL"<<std::endl);
            exit(-1);
        }
    }
    
    RecoveryMgr(const int numProcesses, const int _neutralizeSignal, MasterRecordMgr * const masterRecordMgr)
            : NUM_PROCESSES(numProcesses) , neutralizeSignal(_neutralizeSignal){
        setjmpbuffers = new sigjmp_buf[numProcesses];
        pthread_key_create(&pthreadkey, NULL);
        
#ifndef __CYGWIN__
        if (MasterRecordMgr::supportsCrashRecovery()) {
            // set up crash recovery signal handling for this process
            memset(&___act, 0, sizeof(___act));
            ___act.sa_sigaction = crashhandler<MasterRecordMgr>; // specify signal handler
            ___act.sa_flags = SA_RESTART | SA_SIGINFO; // restart any interrupted sys calls instead of silently failing
            sigfillset(&___act.sa_mask);               // block signals during handler
            if (sigaction(_neutralizeSignal, &___act, NULL)) {
                COUTATOMIC("ERROR: could not register signal handler for signal "<<_neutralizeSignal<<std::endl);
                assert(false);
                exit(-1);
            } else {
                VERBOSE COUTATOMIC("registered signal "<<_neutralizeSignal<<" for crash recovery"<<std::endl);
            }
        }
#endif
        // set up shared pointer to this class instance for the signal handler
        ___singleton = (void *) masterRecordMgr;
    }
    ~RecoveryMgr() {
        delete[] setjmpbuffers;
    }
};

#endif	/* RECOVERY_MANAGER_H */

