/*
 * Copyright 2017-2019
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _ONE_FILE_WAIT_FREE_TRANSACTIONAL_MEMORY_WITH_HAZARD_ERAS_H_
#define _ONE_FILE_WAIT_FREE_TRANSACTIONAL_MEMORY_WITH_HAZARD_ERAS_H_

#include <atomic>
#include <cassert>
#include <iostream>
#include <vector>
#include <functional>
#include <cstring>

// Please keep this file in sync (as much as possible) with ptms/POneFileWF.hpp

namespace ofwf {

//
// User configurable variables.
// Feel free to change these if you need larger transactions, more allocations per transacation, or more threads.
//

// Maximum number of registered threads that can execute transactions
static const int REGISTRY_MAX_THREADS = 128;
// Maximum number of stores in the WriteSet per transaction
static const uint64_t TX_MAX_STORES = 40*1024;
// Number of buckets in the hashmap of the WriteSet.
static const uint64_t HASH_BUCKETS = 1024;
// Maximum number of allocations in one transaction
static const uint64_t TX_MAX_ALLOCS = 10*1024;
// Maximum number of deallocations in one transaction
static const uint64_t TX_MAX_RETIRES = 10*1024;



// DCAS / CAS2 macro
#define DCAS(ptr, o1, o2, n1, n2)                               \
({                                                              \
    char __ret;                                                 \
    __typeof__(o2) __junk;                                      \
    __typeof__(*(ptr)) __old1 = (o1);                           \
    __typeof__(o2) __old2 = (o2);                               \
    __typeof__(*(ptr)) __new1 = (n1);                           \
    __typeof__(o2) __new2 = (n2);                               \
    asm volatile("lock cmpxchg16b %2;setz %1"                   \
                   : "=d"(__junk), "=a"(__ret), "+m" (*ptr)     \
                   : "b"(__new1), "c"(__new2),                  \
                     "a"(__old1), "d"(__old2));                 \
    __ret; })


// Functions to convert between a transaction identifier (uint64_t) and a pair of {sequence,index}
static inline uint64_t seqidx2trans(uint64_t seq, uint64_t idx) {
    return (seq << 10) | idx;
}
static inline uint64_t trans2seq(uint64_t trans) {
    return trans >> 10;
}
static inline uint64_t trans2idx(uint64_t trans) {
    return trans & 0x3FF; // 10 bits
}


//
// Thread Registry stuff
//
extern void thread_registry_deregister_thread(const int tid);

// An helper class to do the checkin and checkout of the thread registry
struct ThreadCheckInCheckOut {
    static const int NOT_ASSIGNED = -1;
    int tid { NOT_ASSIGNED };
    ~ThreadCheckInCheckOut() {
        if (tid == NOT_ASSIGNED) return;
        thread_registry_deregister_thread(tid);
    }
};

extern thread_local ThreadCheckInCheckOut tl_tcico;

// Forward declaration of global/singleton instance
class ThreadRegistry;
extern ThreadRegistry gThreadRegistry;

/*
 * <h1> Registry for threads </h1>
 *
 * This is singleton type class that allows assignement of a unique id to each thread.
 * The first time a thread calls ThreadRegistry::getTID() it will allocate a free slot in 'usedTID[]'.
 * This tid wil be saved in a thread-local variable of the type ThreadCheckInCheckOut which
 * upon destruction of the thread will call the destructor of ThreadCheckInCheckOut and free the
 * corresponding slot to be used by a later thread.
 */
class ThreadRegistry {
private:
    alignas(128) std::atomic<bool>      usedTID[REGISTRY_MAX_THREADS];   // Which TIDs are in use by threads
    alignas(128) std::atomic<int>       maxTid {-1};                     // Highest TID (+1) in use by threads

public:
    ThreadRegistry() {
        for (int it = 0; it < REGISTRY_MAX_THREADS; it++) {
            usedTID[it].store(false, std::memory_order_relaxed);
        }
    }

    // Progress condition: wait-free bounded (by the number of threads)
    int register_thread_new(void) {
        for (int tid = 0; tid < REGISTRY_MAX_THREADS; tid++) {
            if (usedTID[tid].load(std::memory_order_acquire)) continue;
            bool unused = false;
            if (!usedTID[tid].compare_exchange_strong(unused, true)) continue;
            // Increase the current maximum to cover our thread id
            int curMax = maxTid.load();
            while (curMax <= tid) {
                maxTid.compare_exchange_strong(curMax, tid+1);
                curMax = maxTid.load();
            }
            tl_tcico.tid = tid;
            return tid;
        }
        std::cout << "ERROR: Too many threads, registry can only hold " << REGISTRY_MAX_THREADS << " threads\n";
        assert(false);
    }

    // Progress condition: wait-free population oblivious
    inline void deregister_thread(const int tid) {
        usedTID[tid].store(false, std::memory_order_release);
    }

    // Progress condition: wait-free population oblivious
    static inline uint64_t getMaxThreads(void) {
        return gThreadRegistry.maxTid.load(std::memory_order_acquire);
    }

    // Progress condition: wait-free bounded (by the number of threads)
    static inline int getTID(void) {
        int tid = tl_tcico.tid;
        if (tid != ThreadCheckInCheckOut::NOT_ASSIGNED) return tid;
        return gThreadRegistry.register_thread_new();
    }
};


// Each object tracked by Hazard Eras needs to have tmbase as one of its base classes.
struct tmbase {
    uint64_t newEra_ {0};        // Filled by tmNew() or tmMalloc()
    uint64_t delEra_ {0};        // Filled by tmDelete() or tmFree()
};


// One entry in the log of allocations (not used for retires like in the WF version).
// In case the transactions aborts, we can rollback our allocations, hiding the type information inside the lambda.
// Sure, we could keep everything in std::function, but this uses less memory.
struct Deletable {
    void* obj {nullptr};         // Pointer to object to be deleted
    void (*reclaim)(void*);      // A wrapper to keep the type of the underlying object
};


// A wrapper to std::function so that we can track it with Hazard Eras
struct TransFunc : public tmbase {
    std::function<uint64_t()> func;
    template<typename F> TransFunc(F&& f) : func{f} { }
};


// This is a specialized implementation of Hazard Eras meant to be used in the OneFile STM.
// Hazard Eras is a lock-free memory reclamation technique described here:
// https://github.com/pramalhe/ConcurrencyFreaks/blob/master/papers/hazarderas-2017.pdf
// https://dl.acm.org/citation.cfm?id=3087588
//
// We're using OF::curTx.seq as the global era.
//
// This implementation is different from the lock-free OneFile STM because we need
// to track the lifetime of the std::function objects where the lambdas are put.
class HazardErasOF {
private:
    static const uint64_t                    NOERA = 0;
    static const int                         CLPAD = 128/sizeof(std::atomic<uint64_t>);
    static const int                         THRESHOLD_R = 0; // This is named 'R' in the HP paper
    const unsigned int                       maxThreads;
    alignas(128) std::atomic<uint64_t>*      he;
    // It's not nice that we have a lot of empty vectors, but we need padding to avoid false sharing
    alignas(128) std::vector<tmbase*>        retiredList[REGISTRY_MAX_THREADS*CLPAD];
    alignas(128) std::vector<TransFunc*>   retiredListTx[REGISTRY_MAX_THREADS*CLPAD];

public:
    HazardErasOF(unsigned int maxThreads=REGISTRY_MAX_THREADS) : maxThreads{maxThreads} {
        he = new std::atomic<uint64_t>[REGISTRY_MAX_THREADS*CLPAD];
        for (unsigned it = 0; it < REGISTRY_MAX_THREADS; it++) {
            he[it*CLPAD].store(NOERA, std::memory_order_relaxed);
            retiredList[it*CLPAD].reserve(REGISTRY_MAX_THREADS);  // We pre-reserve one object per thread, should be enough to start
            retiredListTx[it*CLPAD].reserve(REGISTRY_MAX_THREADS);
        }
    }

    ~HazardErasOF() {
        // Clear the objects in the retired lists
        for (unsigned it = 0; it < maxThreads; it++) {
            for (unsigned iret = 0; iret < retiredList[it*CLPAD].size(); iret++) {
                tmbase* del = retiredList[it*CLPAD][iret];
                std::free(del);
                // No need to call destructor because it was already executed as part of the transaction
            }
            for (unsigned iret = 0; iret < retiredListTx[it*CLPAD].size(); iret++) {
                TransFunc* tx = retiredListTx[it*CLPAD][iret];
                delete tx;
            }
        }
        delete[] he;
    }

    // Progress condition: wait-free population oblivious
    inline void clear(const int tid) {
        he[tid*CLPAD].store(NOERA, std::memory_order_release);
    }

    // Progress condition: wait-free population oblivious
    inline void set(uint64_t trans, const int tid) {
        he[tid*CLPAD].store(trans2seq(trans));
    }

    // Progress condition: wait-free population oblivious
    inline void addToRetiredList(tmbase* newdel, const int tid) {
        retiredList[tid*CLPAD].push_back(newdel);
    }

    // Progress condition: wait-free population oblivious
    inline void addToRetiredListTx(TransFunc* tx, const int tid) {
        retiredListTx[tid*CLPAD].push_back(tx);
    }

    /**
     * Progress condition: bounded wait-free
     *
     * Attemps to delete the no-longer-in-use objects in the retired list.
     * We need to pass the currEra coming from the seq of the currTx so that
     * the objects from the current transaction don't get deleted.
     *
     * TODO: consider using erase() with std::remove_if()
     */
    void clean(uint64_t curEra, const int tid) {
        if (retiredList[tid*CLPAD].size() < THRESHOLD_R) return;
        for (unsigned iret = 0; iret < retiredList[tid*CLPAD].size();) {
            tmbase* del = retiredList[tid*CLPAD][iret];
            if (canDelete(curEra, del)) {
                retiredList[tid*CLPAD].erase(retiredList[tid*CLPAD].begin() + iret);
                std::free(del);
                // No need to call destructor because it was executed as part of the transaction
                continue;
            }
            iret++;
        }
        for (unsigned iret = 0; iret < retiredListTx[tid*CLPAD].size();) {
            TransFunc* tx = retiredListTx[tid*CLPAD][iret];
            if (canDelete(curEra, tx)) {
                retiredListTx[tid*CLPAD].erase(retiredListTx[tid*CLPAD].begin() + iret);
                delete tx;
                continue;
            }
            iret++;
        }
    }

    // Progress condition: wait-free bounded (by the number of threads)
    inline bool canDelete(uint64_t curEra, tmbase* del) {
        // We can't delete objects from the current transaction
        if (del->delEra_ == curEra) return false;
        for (unsigned it = 0; it < ThreadRegistry::getMaxThreads(); it++) {
            const auto era = he[it*CLPAD].load(std::memory_order_acquire);
            if (era == NOERA || era < del->newEra_ || era > del->delEra_) continue;
            return false;
        }
        return true;
    }
};


// T is typically a pointer to a node, but it can be integers or other stuff, as long as it fits in 64 bits
template<typename T> struct tmtype {
    // Stores the actual value as an atomic
    alignas(16) std::atomic<uint64_t>  val;
    // Lets hope this comes immediately after 'val' in memory mapping, otherwise the DCAS() will fail
    alignas(8)  std::atomic<uint64_t>  seq {1};

    tmtype() { }

    tmtype(T initVal) { isolated_store(initVal); }

    // Casting operator
    operator T() { return pload(); }

    // Prefix increment operator: ++x
    void operator++ () { pstore(pload()+1); }
    // Prefix decrement operator: --x
    void operator-- () { pstore(pload()-1); }
    void operator++ (int) { pstore(pload()+1); }
    void operator-- (int) { pstore(pload()-1); }

    // Equals operator: first downcast to T and then compare
    bool operator == (const T& otherval) const { return pload() == otherval; }

    // Difference operator: first downcast to T and then compare
    bool operator != (const T& otherval) const { return pload() != otherval; }

    // Relational operators
    bool operator < (const T& rhs) { return pload() < rhs; }
    bool operator > (const T& rhs) { return pload() > rhs; }
    bool operator <= (const T& rhs) { return pload() <= rhs; }
    bool operator >= (const T& rhs) { return pload() >= rhs; }

    // Operator arrow ->
    T operator->() { return pload(); }

    // Copy constructor
    tmtype<T>(const tmtype<T>& other) { pstore(other.pload()); }

    // Assignment operator from an tmtype
    tmtype<T>& operator=(const tmtype<T>& other) {
        pstore(other.pload());
        return *this;
    }

    // Assignment operator from a value
    tmtype<T>& operator=(T value) {
        pstore(value);
        return *this;
    }

    // Operator &
    T* operator&() {
        return (T*)this;
    }

    // Meant to be called when know we're the only ones touching
    // these contents, for example, in the constructor of an object, before
    // making the object visible to other threads.
    inline void isolated_store(T newVal) {
        val.store((uint64_t)newVal, std::memory_order_relaxed);
    }

    // Used only internally to initialize the operations[] array
    inline void operationsInit() {
        val.store((uint64_t)nullptr, std::memory_order_relaxed);
        seq.store(0, std::memory_order_relaxed);
    }

    // Used only internally to initialize the results[] array
    inline void resultsInit() {
        val.store(0, std::memory_order_relaxed);
        seq.store(1, std::memory_order_relaxed);
    }

    // Used only internally by updateTx() to determine if the request is opened or not
    inline uint64_t getSeq() const {
        return seq.load(std::memory_order_acquire);
    }

    // Used only internally by updateTx()
    inline void rawStore(T& newVal, uint64_t lseq) {
        val.store((uint64_t)newVal, std::memory_order_relaxed);
        seq.store(lseq, std::memory_order_release);
    }

    // Methods that are defined later because they have compilation dependencies on gOFWF
    inline T pload() const;
    inline bool rawLoad(T& keepVal, uint64_t& keepSeq);
    inline void pstore(T newVal);
};


// A single entry in the write-set
struct WriteSetEntry {
    void*          addr {nullptr};  // Address of value+sequence to change
    uint64_t       val;             // Desired value to change to
    WriteSetEntry* next {nullptr};  // Pointer to next node in the (intrusive) hash map
};

extern thread_local bool tl_is_read_only;


// The write-set is a log of the words modified during the transaction.
// This log is an array with an intrusive hashmap of size HASH_BUCKETS.
struct WriteSet {
    static const uint64_t MAX_ARRAY_LOOKUP = 30;  // Beyond this, it seems to be faster to use the hashmap
    WriteSetEntry*        buckets[HASH_BUCKETS];  // Intrusive HashMap for fast lookup in large(r) transactions
    uint64_t              numStores {0};          // Number of stores in the writeSet for the current transaction
    WriteSetEntry         log[TX_MAX_STORES];     // Redo log of stores

    WriteSet() {
        numStores = 0;
        for (unsigned i = 0; i < HASH_BUCKETS; i++) buckets[i] = &log[TX_MAX_STORES-1];
    }

    // Each address on a different bucket
    inline uint64_t hash(const void* addr) const {
        return (((uint64_t)addr) >> 3) % HASH_BUCKETS;
    }

    // Adds a modification to the redo log
    inline void addOrReplace(void* addr, uint64_t val) {
        if (tl_is_read_only) tl_is_read_only = false;
        const uint64_t hashAddr = hash(addr);
        if (numStores < MAX_ARRAY_LOOKUP) {
            // Lookup in array
            for (unsigned int idx = 0; idx < numStores; idx++) {
                if (log[idx].addr == addr) {
                    log[idx].val = val;
                    return;
                }
            }
        } else {
            // Lookup in hashmap
            WriteSetEntry* be = buckets[hashAddr];
            if (be < &log[numStores] && hash(be->addr) == hashAddr) {
                while (be != nullptr) {
                    if (be->addr == addr) {
                        be->val = val;
                        return;
                    }
                    be = be->next;
                }
            }
        }
        // Add to array
        WriteSetEntry* e = &log[numStores++];
        assert(numStores < TX_MAX_STORES);
        e->addr = addr;
        e->val = val;
        // Add to hashmap
        WriteSetEntry* be = buckets[hashAddr];
        // Clear if entry is from previous tx
        e->next = (be < e && hash(be->addr) == hashAddr) ? be : nullptr;
        buckets[hashAddr] = e;
    }

    // Does a lookup on the WriteSet for an addr.
    // If the numStores is lower than MAX_ARRAY_LOOKUP, the lookup is done on the log, otherwise, the lookup is done on the hashmap.
    // If it's not in the write-set, return lval.
    inline uint64_t lookupAddr(const void* addr, uint64_t lval) {
        if (numStores < MAX_ARRAY_LOOKUP) {
            // Lookup in array
            for (unsigned int idx = 0; idx < numStores; idx++) {
                if (log[idx].addr == addr) return log[idx].val;
            }
        } else {
            // Lookup in hashmap
            const uint64_t hashAddr = hash(addr);
            WriteSetEntry* be = buckets[hashAddr];
            if (be < &log[numStores] && hash(be->addr) == hashAddr) {
                while (be != nullptr) {
                    if (be->addr == addr) return be->val;
                    be = be->next;
                }
            }
        }
        return lval;
    }

    // Assignment operator, used when making a copy of a WriteSet to help another thread
    WriteSet& operator = (const WriteSet &other) {
        numStores = other.numStores;
        for (uint64_t i = 0; i < numStores; i++) log[i] = other.log[i];
        return *this;
    }

    // Applies all entries in the log as DCASes.
    // Seq must match for DCAS to succeed. This method is on the "hot-path".
    inline void apply(uint64_t seq, const int tid) {
        for (uint64_t i = 0; i < numStores; i++) {
            // Use an heuristic to give each thread 8 consecutive DCAS to apply
            WriteSetEntry& e = log[(tid*8 + i) % numStores];
            tmtype<uint64_t>* tmte = (tmtype<uint64_t>*)e.addr;
            uint64_t lval = tmte->val.load(std::memory_order_acquire);
            uint64_t lseq = tmte->seq.load(std::memory_order_acquire);
            if (lseq < seq) DCAS((uint64_t*)e.addr, lval, lseq, e.val, seq);
        }
    }
};


// Forward declaration
struct OpData;
// This is used by addOrReplace() to know which OpData instance to use for the current transaction
extern thread_local OpData* tl_opdata;


// ts purpose is to hold thread-local data
struct OpData {
    uint64_t              curTx {0};                   // Used during a transaction to keep the value of currTx read in beginTx() (owner thread only)
    std::atomic<uint64_t> request {0};                 // Can be moved to CLOSED by other threads, using a CAS
    uint64_t              nestedTrans {0};             // Thread-local: Number of nested transactions
    uint64_t              numRetires {0};              // Number of calls to retire() in this transaction (owner thread only)
    tmbase*               rlog[TX_MAX_RETIRES];        // List of retired objects during the transaction (owner thread only)
    uint64_t              numAllocs {0};               // Number of calls to tmNew() in this transaction (owner thread only)
    Deletable             alog[TX_MAX_ALLOCS];         // List of newly allocated objects during the transaction (owner thread only)
};


// Used to identify aborted transactions
struct AbortedTx {};
static constexpr AbortedTx AbortedTxException {};

class OneFileWF;
extern OneFileWF gOFWF;


/**
 * <h1> OneFile STM (Wait-Free) </h1>
 *
 * OneFile is a Software Transacional Memory with wait-free progress, meant to
 * implement wait-free data structures. It has integrated wait-free memory
 * reclamation using Hazard Eras: https://dl.acm.org/citation.cfm?id=3087588
 *
 * OneFile is a word-based STM and it uses double-compare-and-swap (DCAS).
 *
 * Right now it has several limitations, some will be fixed in the future, some may be hard limitations of this approach:
 * - We can't have stack allocated tmtype<> variables. For example, we can't created inside a transaction "tmtpye<uint64_t> tmp = a;",
 *   it will give weird errors because of stack allocation.
 * - We need DCAS but it can be emulated with LL/SC or even with single-word CAS
 *   if we do redirection to a (lock-free) pool with SeqPtrs;
 */
class OneFileWF {
private:
    static const bool                    debug = false;
    HazardErasOF                         he {REGISTRY_MAX_THREADS};
    OpData                              *opData;
    // Maximum number of times a reader will fail a transaction before turning into an updateTx()
    static const int                     MAX_READ_TRIES = 4;
    // Member variables for wait-free consensus
    tmtype<TransFunc*>*                  operations;  // We've tried adding padding here but it didn't make a difference
    tmtype<uint64_t>*                    results;
public:
    std::atomic<uint64_t>                pad0[16];  // two cache lines of padding, before and after curTx
    std::atomic<uint64_t>                curTx {seqidx2trans(1,0)};
    std::atomic<uint64_t>                pad1[15];
    WriteSet                            *writeSets;                    // Two write-sets for each thread

    OneFileWF() {
        opData = new OpData[REGISTRY_MAX_THREADS];
        writeSets = new WriteSet[REGISTRY_MAX_THREADS];
        operations = new tmtype<TransFunc*>[REGISTRY_MAX_THREADS];
        for (unsigned i = 0; i < REGISTRY_MAX_THREADS; i++) operations[i].operationsInit();
        results = new tmtype<uint64_t>[REGISTRY_MAX_THREADS];
        for (unsigned i = 0; i < REGISTRY_MAX_THREADS; i++) results[i].resultsInit();
    }

    ~OneFileWF() {
        delete[] opData;
        delete[] writeSets;
        delete[] operations;
        delete[] results;
    }

    static std::string className() { return "OneFileSTM-WF"; }

    // Progress condition: wait-free population-oblivious
    // Attempts to publish our write-set (commit the transaction) and then applies the write-set.
    // Returns true if my transaction was committed.
    inline bool commitTx(OpData& myopd, const int tid) {
        // If it's a read-only transaction, then commit immediately
        if (writeSets[tid].numStores == 0 && myopd.numRetires == 0) return true;
        // Give up if the curTx has changed sinced our transaction started
        if (myopd.curTx != curTx.load(std::memory_order_acquire)) return false;
        // Move our request to OPEN, using the sequence of the previous transaction +1
        uint64_t seq = trans2seq(myopd.curTx);
        uint64_t newTx = seqidx2trans(seq+1,tid);
        myopd.request.store(newTx, std::memory_order_release);
        // Attempt to CAS curTx to our OpData instance (tid) incrementing the seq in it
        uint64_t lcurTx = myopd.curTx;
        if (debug) printf("tid=%i  attempting CAS on curTx from (%ld,%ld) to (%ld,%ld)\n", tid, trans2seq(lcurTx), trans2idx(lcurTx), seq+1, (uint64_t)tid);
        if (!curTx.compare_exchange_strong(lcurTx, newTx)) return false;
        // Execute each store in the write-set using DCAS() and close the request
        helpApply(newTx, tid);
        retireRetiresFromLog(myopd, tid);
        myopd.numAllocs = 0;
        if (debug) printf("Committed transaction (%ld,%ld) with %ld stores\n", seq+1, (uint64_t)tid, writeSets[tid].numStores);
        return true;
    }

    // Progress condition: wait-free (bounded by the number of threads)
    // Applies a mutative transaction or gets another thread with an ongoing
    // transaction to apply it.
    // If three 'seq' have passed since the transaction when we published our
    // function, then the worst-case scenario is: the first transaction does not
    // see our function; the second transaction transforms our function
    // but doesn't apply the corresponding write-set; the third transaction
    // guarantees that the log of the second transaction is applied.
    inline void innerUpdateTx(OpData& myopd, TransFunc* funcptr, const int tid) {
        ++myopd.nestedTrans;
        if (debug) printf("updateTx(tid=%d)\n", tid);
        // We need an era from before the 'funcptr' is announced, so as to protect it
        uint64_t firstEra = trans2seq(curTx.load(std::memory_order_acquire));
        operations[tid].rawStore(funcptr, results[tid].getSeq());
        tl_opdata = &myopd;
        // Check 3x for the completion of our operation because we don't have a fence
        // on operations[tid].rawStore(), otherwise it would be just 2x.
        for (int iter = 0; iter < 4; iter++) {
            // An update transaction is read-only until it does the first store()
            tl_is_read_only = true;
            // Clear the logs of the previous transaction
            deleteAllocsFromLog(myopd);
            writeSets[tid].numStores = 0;
            myopd.numRetires = 0;
            myopd.curTx = curTx.load(std::memory_order_acquire);
            // Optimization: if my request is answered, then my tx is committed
            if (results[tid].getSeq() > operations[tid].getSeq()) break;
            helpApply(myopd.curTx, tid);
            // Reset the write-set after (possibly) helping another transaction complete
            writeSets[tid].numStores = 0;
            // Use HE to protect the objects we're going to access during the transform phase
            he.set(myopd.curTx, tid);
            if (myopd.curTx != curTx.load()) continue;
            try {
                if (!transformAll(myopd.curTx, tid)) continue;
            } catch (AbortedTx&) {
                continue;
            }
            if (commitTx(myopd, tid)) break;
        }
        deleteAllocsFromLog(myopd);
        tl_opdata = nullptr;
        --myopd.nestedTrans;
        he.clear(tid);
        retireMyFunc(tid, funcptr, firstEra);
    }

    // Update transaction with non-void return value
    template<typename R, class F> static R updateTx(F&& func) {
        const int tid = ThreadRegistry::getTID();
        OpData& myopd = gOFWF.opData[tid];
        if (myopd.nestedTrans > 0) return func();
        // Copy the lambda to a std::function<> and announce a request with the pointer to it
        gOFWF.innerUpdateTx(myopd, new TransFunc([func] () { return (uint64_t)func(); }), tid);
        return (R)gOFWF.results[tid].pload();
    }

    // Update transaction with void return value
    template<class F> static void updateTx(F&& func) {
        const int tid = ThreadRegistry::getTID();
        OpData& myopd = gOFWF.opData[tid];
        if (myopd.nestedTrans > 0) {
            func();
            return;
        }
        // Copy the lambda to a std::function<> and announce a request with the pointer to it
        gOFWF.innerUpdateTx(myopd, new TransFunc([func] () { func(); return 0; }), tid);
    }

    // Progress condition: wait-free (bounded by the number of threads + MAX_READ_TRIES)
    template<typename R, class F> R readTransaction(F&& func) {
        const int tid = ThreadRegistry::getTID();
        OpData& myopd = opData[tid];
        if (myopd.nestedTrans > 0) return func();
        ++myopd.nestedTrans;
        tl_opdata = &myopd;
        tl_is_read_only = true;
        if (debug) printf("readTx(tid=%d)\n", tid);
        R retval {};
        writeSets[tid].numStores = 0;
        myopd.numAllocs = 0;
        myopd.numRetires = 0;
        for (int iter = 0; iter < MAX_READ_TRIES; iter++) {
            myopd.curTx = curTx.load(std::memory_order_acquire);
            helpApply(myopd.curTx, tid);
            // Use HE to protect the objects we're going to access during the simulation
            he.set(myopd.curTx, tid);
            // Reset the write-set after (possibly) helping another transaction complete
            writeSets[tid].numStores = 0;
            if (myopd.curTx != curTx.load()) continue;
            try {
                retval = func();
            } catch (AbortedTx&) {
                continue;
            }
            --myopd.nestedTrans;
            tl_opdata = nullptr;
            he.clear(tid);
            return retval;
        }
        if (debug) printf("readTx() executed MAX_READ_TRIES, posing as updateTx()\n");
        --myopd.nestedTrans;
        // Tried too many times unsucessfully, pose as an updateTx()
        return updateTx<R>(func);
    }

    template<typename R, typename F> static R readTx(F&& func) { return gOFWF.readTransaction<R>(func); }
    //template<typename F> static void readTx(F&& func) { gOFWF.readTransaction(func); }

    // When inside a transaction, the user can't call "new" directly because if
    // the transaction fails, it would leak the memory of these allocations.
    // Instead, we provide an allocator that keeps pointers to these objects
    // in a log, and in the event of a failed commit of the transaction, it will
    // delete the objects so that there are no leaks.
    // TODO: Add static_assert to check if T is of tmbase
    template <typename T, typename... Args> static T* tmNew(Args&&... args) {
        T* ptr = (T*)std::malloc(sizeof(T));
        new (ptr) T(std::forward<Args>(args)...);  // new placement
        ptr->newEra_ = trans2seq(gOFWF.curTx.load(std::memory_order_acquire));
        OpData* myopd = tl_opdata;
        if (myopd != nullptr) {
            assert(myopd->numAllocs != TX_MAX_ALLOCS);
            Deletable& del = myopd->alog[myopd->numAllocs++];
            del.obj = ptr;
            // This func ptr to a lambda gives us a way to call the destructor
            // when a transaction aborts.
            del.reclaim = [](void* obj) { static_cast<T*>(obj)->~T(); std::free(obj); };
        }
        return ptr;
    }

    // The user can not directly delete objects in the transaction because the
    // transaction may fail and needs to be retried and other threads may be
    // using those objects.
    // Instead, it has to call retire() for the objects it intends to delete.
    // The retire() puts the objects in the rlog, and only when the transaction
    // commits, the objects are put in the Hazard Eras retired list.
    // The del.delEra is filled in retireRetiresFromLog().
    // TODO: Add static_assert to check if T is of tmbase
    template<typename T> static void tmDelete(T* obj) {
        if (obj == nullptr) return;
        obj->~T(); // Execute destructor as part of the current transaction
        OpData* myopd = tl_opdata;
        if (myopd == nullptr) {
            std::free(obj);  // Outside a transaction, just delete the object
            return;
        }
        assert(myopd->numRetires != TX_MAX_RETIRES);
        myopd->rlog[myopd->numRetires++] = obj;
    }

    // We snap a tmbase at the beginning of the allocation
    static void* tmMalloc(size_t size) {
        uint8_t* ptr = (uint8_t*)std::malloc(size+sizeof(tmbase));
        // We must reset the contents to zero to guarantee that if any tmtypes are allocated inside, their 'seq' will be zero
        std::memset(ptr+sizeof(tmbase), 0, size);
        ((tmbase*)ptr)->newEra_ = trans2seq(gOFWF.curTx.load(std::memory_order_acquire));
        OpData* myopd = tl_opdata;
        if (myopd != nullptr) {
            assert(myopd->numAllocs != TX_MAX_ALLOCS);
            Deletable& del = myopd->alog[myopd->numAllocs++];
            del.obj = ptr;
            del.reclaim = [](void* obj) { std::free(obj); };
        }
        return ptr + sizeof(tmbase);
    }

    // We assume there is a tmbase allocated in the beginning of the allocation
    static void tmFree(void* obj) {
        if (obj == nullptr) return;
        OpData* myopd = tl_opdata;
        uint8_t* ptr = (uint8_t*)obj - sizeof(tmbase);
        if (myopd == nullptr) {
            std::free(ptr);  // Outside a transaction, just free the object
            return;
        }
        assert(myopd->numRetires != TX_MAX_RETIRES);
        myopd->rlog[myopd->numRetires++] = (tmbase*)ptr;
    }

private:
    // Progress condition: wait-free population oblivious
    inline void helpApply(uint64_t lcurTx, const uint64_t tid) {
        const uint64_t idx = trans2idx(lcurTx);
        const uint64_t seq = trans2seq(lcurTx);
        OpData& opd = opData[idx];
        // Nothing to apply unless the request matches the curTx
        if (lcurTx != opd.request.load(std::memory_order_acquire)) return;
        if (idx != tid) {
            // Make a copy of the write-set and check if it is consistent
            writeSets[tid] = writeSets[idx];
            // Use HE to protect the objects the transaction touches
            he.set(lcurTx, tid);
            if (lcurTx != curTx.load()) return;
            // The published era is now protecting all objects alive in the transaction lcurTx
            if (lcurTx != opd.request.load(std::memory_order_acquire)) return;
        }
        if (debug) printf("Applying %ld stores in write-set\n", writeSets[tid].numStores);
        writeSets[tid].apply(seq, tid);
        const uint64_t newReq = seqidx2trans(seq+1,idx);
        if (idx == tid) {
            opd.request.store(newReq, std::memory_order_release);
        } else {
            if (opd.request.load(std::memory_order_acquire) == lcurTx) {
                opd.request.compare_exchange_strong(lcurTx, newReq);
            }
        }
    }

    // This is called when the transaction fails, to undo all the allocations done during the transaction
     void deleteAllocsFromLog(OpData& myopd) {
        for (unsigned i = 0; i < myopd.numAllocs; i++) {
            myopd.alog[i].reclaim(myopd.alog[i].obj);
        }
        myopd.numAllocs = 0;
    }

    // My transaction was successful, it's my duty to cleanup any retired objects.
    // This is called by the owner thread when the transaction succeeds, to pass
    // the retired objects to Hazard Eras. We can't delete the objects
    // immediately because there might be other threads trying to apply our log
    // which may (or may not) contain addresses inside the objects in this list.
    void retireRetiresFromLog(OpData& myopd, const int tid) {
        uint64_t lseq = trans2seq(curTx.load(std::memory_order_acquire));
        // First, add all the objects to the list of retired/zombies
        for (unsigned i = 0; i < myopd.numRetires; i++) {
            myopd.rlog[i]->delEra_ = lseq;
            he.addToRetiredList(myopd.rlog[i], tid);
        }
        // Second, start a cleaning phase, scanning to see which objects can be removed
        he.clean(lseq, tid);
        myopd.numRetires = 0;
    }


    inline void retireMyFunc(const int tid, TransFunc* myfunc, uint64_t firstEra) {
        myfunc->newEra_ = firstEra;
        myfunc->delEra_ = trans2seq(curTx.load(std::memory_order_acquire))+1; // Do we really need the +1 ?
        he.addToRetiredListTx(myfunc, tid);
    }

    // Aggregate all the functions of the different thread's writeTransaction()
    // and transform them into to a single log (the current thread's log).
    // Returns true if all active TransFunc were transformed
    inline bool transformAll(const uint64_t lcurrTx, const int tid) {
        for (unsigned i = 0; i < ThreadRegistry::getMaxThreads(); i++) {
            // Check if the operation of thread i has been applied (has a matching result)
            TransFunc* txfunc;
            uint64_t res, operationsSeq, resultSeq;
            if (!operations[i].rawLoad(txfunc, operationsSeq)) continue;
            if (!results[i].rawLoad(res, resultSeq)) continue;
            if (resultSeq > operationsSeq) continue;
            // Operation has not yet been applied, check that transaction identifier has not changed
            if (lcurrTx != curTx.load(std::memory_order_acquire)) return false;
            // Apply the operation of thread i and save result in results[i],
            // with this store being part of the transaction itself.
            results[i] = txfunc->func();
        }
        return true;
    }
};


//
// Wrapper methods to the global TM instance. The user should use these:
//
template<typename R, typename F> static R updateTx(F&& func) { return gOFWF.updateTx<R>(func); }
template<typename R, typename F> static R readTx(F&& func) { return gOFWF.readTx<R>(func); }
template<typename F> static void updateTx(F&& func) { gOFWF.updateTx(func); }
template<typename F> static void readTx(F&& func) { gOFWF.readTx(func); }
template<typename T, typename... Args> T* tmNew(Args&&... args) { return OneFileWF::tmNew<T>(args...); }
template<typename T> void tmDelete(T* obj) { OneFileWF::tmDelete<T>(obj); }
inline void* tmMalloc(size_t size) { return OneFileWF::tmMalloc(size); }
inline void tmFree(void* obj) { OneFileWF::tmFree(obj); }


// We have to check if there is a new ongoing transaction and if so, abort
// this execution immediately for two reasons:
// 1. Memory Reclamation: the val we're returning may be a pointer to an
// object that has since been retired and deleted, therefore we can't allow
// user code to de-reference it;
// 2. Invariant Conservation: The val we're reading may be from a newer
// transaction, which implies that it may break an invariant in the user code.
// See examples of invariant breaking in this post:
// http://concurrencyfreaks.com/2013/11/stampedlocktryoptimisticread-and.html
template<typename T> inline T tmtype<T>::pload() const {
    T lval = (T)val.load(std::memory_order_acquire);
    OpData* const myopd = tl_opdata;
    if (myopd == nullptr) return lval;
    uint64_t lseq = seq.load(std::memory_order_acquire);
    if (lseq > trans2seq(myopd->curTx)) throw AbortedTxException;
    if (tl_is_read_only) return lval;
    return (T)gOFWF.writeSets[tl_tcico.tid].lookupAddr(this, (uint64_t)lval);
}

// This method is meant to be used by the internal consensus mechanism, not by the user.
// Returns true if the 'val' and 'seq' placed in 'keepVal' and 'keepSeq'
// are consistent, i.e. linearizabile. We need to use acquire-loads to keep
// order and re-check the 'seq' to make sure it corresponds to the 'val' we're returning.
template<typename T> inline bool tmtype<T>::rawLoad(T& keepVal, uint64_t& keepSeq) {
    keepSeq = seq.load(std::memory_order_acquire);
    keepVal = (T)val.load(std::memory_order_acquire);
    return (keepSeq == seq.load(std::memory_order_acquire));
}

// We don't need to check currTx here because we're not de-referencing
// the val. It's only after a load() that the val may be de-referenced
// (in user code), therefore we do the check on load() only.
template<typename T> inline void tmtype<T>::pstore(T newVal) {
    if (tl_opdata == nullptr) { // Looks like we're outside a transaction
        val.store((uint64_t)newVal, std::memory_order_relaxed);
    } else {
        gOFWF.writeSets[tl_tcico.tid].addOrReplace(this, (uint64_t)newVal);
    }
}


//
// Place these in a .cpp if you include this header from multiple files (compilation units)
//
OneFileWF gOFWF {};
thread_local OpData* tl_opdata {nullptr};
// Global/singleton to hold all the thread registry functionality
ThreadRegistry gThreadRegistry {};
// During a transaction, this is true up until the first store()
thread_local bool tl_is_read_only {false};
// This is where every thread stores the tid it has been assigned when it calls getTID() for the first time.
// When the thread dies, the destructor of ThreadCheckInCheckOut will be called and de-register the thread.
thread_local ThreadCheckInCheckOut tl_tcico {};
// Helper function for thread de-registration
void thread_registry_deregister_thread(const int tid) {
    gThreadRegistry.deregister_thread(tid);
}

} // wait-free transactions with wait-free memory reclamation, and it's all less than 1000 lines of code  :)
#endif /* _ONE_FILE_WAIT_FREE_TRANSACTIONAL_MEMORY_WITH_HAZARD_ERAS_H_ */
