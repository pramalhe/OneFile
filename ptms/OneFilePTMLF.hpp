/*
 * Copyright 2017-2019
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _PERSISTENT_ONE_FILE_LOCK_FREE_TRANSACTIONAL_MEMORY_H_
#define _PERSISTENT_ONE_FILE_LOCK_FREE_TRANSACTIONAL_MEMORY_H_

#include <atomic>
#include <cassert>
#include <iostream>
#include <vector>
#include <functional>
#include <cstring>
#include <sys/mman.h>   // Needed if we use mmap()
#include <sys/types.h>  // Needed by open() and close()
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>     // Needed by close()

// Please keep this file in sync (as much as possible) with stms/OneFileLF.hpp

// Macros needed for persistence
#ifdef PWB_IS_CLFLUSH
  /*
   * More info at http://elixir.free-electrons.com/linux/latest/source/arch/x86/include/asm/special_insns.h#L213
   * Intel programming manual at https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-optimization-manual.pdf
   * Use these for Broadwell CPUs (cervino server)
   */
  #define PWB(addr)              __asm__ volatile("clflush (%0)" :: "r" (addr) : "memory")                  // Broadwell only works with this.
  #define PFENCE()               {}                                                                         // No ordering fences needed for CLFLUSH (section 7.4.6 of Intel manual)
  #define PSYNC()                {}                                                                         // For durability it's not obvious, but CLFLUSH seems to be enough, and PMDK uses the same approach
#elif PWB_IS_CLWB
  /* Use this for CPUs that support clwb, such as the SkyLake SP series (c5 compute intensive instances in AWS are an example of it) */
  #define PWB(addr)              __asm__ volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)(addr)))  // clwb() only for Ice Lake onwards
  #define PFENCE()               __asm__ volatile("sfence" : : : "memory")
  #define PSYNC()                __asm__ volatile("sfence" : : : "memory")
#elif PWB_IS_NOP
  /* pwbs are not needed for shared memory persistency (i.e. persistency across process failure) */
  #define PWB(addr)              {}
  #define PFENCE()               __asm__ volatile("sfence" : : : "memory")
  #define PSYNC()                __asm__ volatile("sfence" : : : "memory")
#elif PWB_IS_CLFLUSHOPT
  /* Use this for CPUs that support clflushopt, which is most recent x86 */
  #define PWB(addr)              __asm__ volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)(addr)))    // clflushopt (Kaby Lake)
  #define PFENCE()               __asm__ volatile("sfence" : : : "memory")
  #define PSYNC()                __asm__ volatile("sfence" : : : "memory")
#else
#error "You must define what PWB is. Choose PWB_IS_CLFLUSHOPT if you don't know what your CPU is capable of"
#endif


/*
 * Differences between POneFileLF and the non-persistent OneFileLF:
 * - A secondary redo log (PWriteSet) is placed in persistent memory before attempting a 'commit'.
 * - The set of the request in helpApply() is always done with a CAS to enforce ordering on the PWBs of the DCAS;
 * - The persistent logs are allocated in PM, same as all user allocations from tmNew(), 'curTx', and 'request'
 */
namespace poflf {

//
// User configurable variables.
// Feel free to change these if you need larger transactions, more allocations per transacation, or more threads.
//

// Maximum number of registered threads that can execute transactions
static const int REGISTRY_MAX_THREADS = 128;
// Maximum number of stores in the WriteSet per transaction
static const uint64_t TX_MAX_STORES = 40*1024;
// Number of buckets in the hashmap of the WriteSet.
static const uint64_t HASH_BUCKETS = 2048;

// Persistent-specific configuration
// Name of persistent file mapping
static const char * PFILE_NAME = "/dev/shm/ponefilelf_shared";
// Start address of mapped persistent memory
static uint8_t* PREGION_ADDR = (uint8_t*)0x7fea00000000;
// Size of persistent memory. Part of it will be used by the redo logs
static const uint64_t PREGION_SIZE = 1024*1024*1024ULL;   // 1 GB by default
// End address of mapped persistent memory
static uint8_t* PREGION_END = (PREGION_ADDR+PREGION_SIZE);
// Maximum number of root pointers available for the user
static const uint64_t MAX_ROOT_POINTERS = 100;


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

// Flush each cache line in a range
static inline void flushFromTo(void* from, void* to) noexcept {
    const uint64_t cache_line_size = 64;
    uint8_t* ptr = (uint8_t*)(((uint64_t)from) & (~(cache_line_size-1)));
    for (; ptr < (uint8_t*)to; ptr += cache_line_size) PWB(ptr);
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


// Forward declaration needed by EsLoco
template<typename T> struct tmtype;

// We need to split the contents from the methods due to compilation dependencies
template<typename T> struct tmtypebase {
    // Stores the actual value as an atomic
    std::atomic<uint64_t>  val;
    // Lets hope this comes immediately after 'val' in memory mapping, otherwise the DCAS() will fail
    std::atomic<uint64_t>  seq;
};


/*
 * EsLoco is an Extremely Simple memory aLOCatOr
 *
 * It is based on intrusive singly-linked lists (a free-list), one for each power of two size.
 * All blocks are powers of two, the smallest size enough to contain the desired user data plus the block header.
 * There is an array named 'freelists' where each entry is a pointer to the head of a stack for that respective block size.
 * Blocks are allocated in powers of 2 of words (64bit words).
 * Each block has an header with two words: the size of the node (in words), the pointer to the next node.
 * The minimum block size is 4 words, with 2 for the header and 2 for the user.
 * When there is no suitable block in the freelist, it will create a new block from the remaining pool.
 *
 * EsLoco was designed for usage in PTMs but it doesn't have to be used only for that.
 * Average number of stores for an allocation is 1.
 * Average number of stores for a de-allocation is 2.
 *
 * Memory layout:
 * ------------------------------------------------------------------------
 * | poolTop | freelists[0] ... freelists[61] | ... allocated objects ... |
 * ------------------------------------------------------------------------
 */
template <template <typename> class P>
class EsLoco {
private:
    struct block {
        P<block*>   next;   // Pointer to next block in free-list (when block is in free-list)
        P<uint64_t> size;   // Exponent of power of two of the size of this block in bytes.
    };

    const bool debugOn = false;

    // Volatile data
    uint8_t* poolAddr {nullptr};
    uint64_t poolSize {0};

    // Pointer to array of persistent heads of free-list
    block* freelists {nullptr};
    // Volatile pointer to persistent pointer to last unused address (the top of the pool)
    P<uint8_t*>* poolTop {nullptr};

    // Number of blocks in the freelists array.
    // Each entry corresponds to an exponent of the block size: 2^4, 2^5, 2^6... 2^40
    static const int kMaxBlockSize = 50; // 1024 TB of memory should be enough

    // For powers of 2, returns the highest bit, otherwise, returns the next highest bit
    uint64_t highestBit(uint64_t val) {
        uint64_t b = 0;
        while ((val >> (b+1)) != 0) b++;
        if (val > (1ULL << b)) return b+1;
        return b;
    }

    uint8_t* aligned(uint8_t* addr) {
        return (uint8_t*)((size_t)addr & (~0x3FULL)) + 128;
    }

public:
    void init(void* addressOfMemoryPool, size_t sizeOfMemoryPool, bool clearPool=true) {
        // Align the base address of the memory pool
        poolAddr = aligned((uint8_t*)addressOfMemoryPool);
        poolSize = sizeOfMemoryPool + (uint8_t*)addressOfMemoryPool - poolAddr;
        // The first thing in the pool is a pointer to the top of the pool
        poolTop = (P<uint8_t*>*)poolAddr;
        // The second thing in the pool is the array of freelists
        freelists = (block*)(poolAddr + sizeof(*poolTop));
        if (clearPool) {
            std::memset(poolAddr, 0, poolSize);
            for (int i = 0; i < kMaxBlockSize; i++) freelists[i].next.pstore(nullptr);
            // The size of the freelists array in bytes is sizeof(block)*kMaxBlockSize
            // Align to cache line boundary (DCAS needs 16 byte alignment)
            poolTop->pstore(aligned(poolAddr + sizeof(*poolTop) + sizeof(block)*kMaxBlockSize));
        }
        if (debugOn) printf("Starting EsLoco with poolAddr=%p and poolSize=%ld, up to %p\n", poolAddr, poolSize, poolAddr+poolSize);
    }

    // Resets the metadata of the allocator back to its defaults
    void reset() {
        std::memset(poolAddr, 0, sizeof(block)*kMaxBlockSize);
        poolTop->pstore(nullptr);
    }

    // Returns the number of bytes that may (or may not) have allocated objects, from the base address to the top address
    uint64_t getUsedSize() {
        return poolTop->pload() - poolAddr;
    }

    // Takes the desired size of the object in bytes.
    // Returns pointer to memory in pool, or nullptr.
    // Does on average 1 store to persistent memory when re-utilizing blocks.
    void* malloc(size_t size) {
        P<uint8_t*>* top = (P<uint8_t*>*)(((uint8_t*)poolTop));
        block* flists = (block*)(((uint8_t*)freelists));
        // Adjust size to nearest (highest) power of 2
        uint64_t bsize = highestBit(size + sizeof(block));
        if (debugOn) printf("malloc(%ld) requested,  block size exponent = %ld\n", size, bsize);
        block* myblock = nullptr;
        // Check if there is a block of that size in the corresponding freelist
        if (flists[bsize].next.pload() != nullptr) {
            if (debugOn) printf("Found available block in freelist\n");
            // Unlink block
            myblock = flists[bsize].next;
            flists[bsize].next = myblock->next;          // pstore()
        } else {
            if (debugOn) printf("Creating new block from top, currently at %p\n", top->pload());
            // Couldn't find a suitable block, get one from the top of the pool if there is one available
            if (top->pload() + (1<<bsize) > poolSize + poolAddr) {
                printf("EsLoco: Out of memory for %ld bytes allocation\n", size);
                return nullptr;
            }
            myblock = (block*)top->pload();
            top->pstore(top->pload() + (1<<bsize));      // pstore()
            myblock->size = bsize;                       // pstore()
        }
        if (debugOn) printf("returning ptr = %p\n", (void*)((uint8_t*)myblock + sizeof(block)));
        // Return the block, minus the header
        return (void*)((uint8_t*)myblock + sizeof(block));
    }

    // Takes a pointer to an object and puts the block on the free-list.
    // Does on average 2 stores to persistent memory.
    void free(void* ptr) {
        if (ptr == nullptr) return;
        block* flists = (block*)(((uint8_t*)freelists));
        block* myblock = (block*)((uint8_t*)ptr - sizeof(block));
        if (debugOn) printf("free(%p)  block size exponent = %ld\n", ptr, myblock->size.pload());
        // Insert the block in the corresponding freelist
        myblock->next = flists[myblock->size].next;      // pstore()
        flists[myblock->size].next = myblock;            // pstore()
    }
};


// Needed by our benchmarks
struct tmbase {
};


// An entry in the persistent write-set
struct PWriteSetEntry {
    void*    addr;  // Address of value+sequence to change
    uint64_t val;   // Desired value to change to
};


// The persistent write-set (undo log)
struct PWriteSet {
    uint64_t              numStores {0};          // Number of stores in the writeSet for the current transaction
    std::atomic<uint64_t> request {0};            // Can be moved to CLOSED by other threads, using a CAS
    PWriteSetEntry        plog[TX_MAX_STORES];    // Redo log of stores

    // Applies all entries in the log. Called only by recover() which is non-concurrent.
    void applyFromRecover() {
        // We're assuming that 'val' is the size of a uint64_t
        for (uint64_t i = 0; i < numStores; i++) {
            *((uint64_t*)plog[i].addr) = plog[i].val;
            PWB(plog[i].addr);
        }
    }
};


// The persistent metadata is a 'header' that contains all the logs and the persistent curTx variable.
// It is located at the start of the persistent region, and the remaining region contains the data available for the allocator to use.
struct PMetadata {
    static const uint64_t   MAGIC_ID = 0x1337babe;
    std::atomic<uint64_t>   curTx {seqidx2trans(1,0)};
    std::atomic<uint64_t>   pad1[15];
    tmtypebase<void*>       rootPtrs[MAX_ROOT_POINTERS];
    PWriteSet               plog[REGISTRY_MAX_THREADS];
    uint64_t                id {0};
    uint64_t                pad2 {0};
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
    WriteSetEntry         log[TX_MAX_STORES];     // Redo log of stores
    uint64_t              numStores {0};          // Number of stores in the writeSet for the current transaction
    WriteSetEntry*        buckets[HASH_BUCKETS];  // Intrusive HashMap for fast lookup in large(r) transactions

    WriteSet() {
        numStores = 0;
        for (int i = 0; i < HASH_BUCKETS; i++) buckets[i] = &log[TX_MAX_STORES-1];
    }

    // Copies the current write set to persistent memory
    inline void persistAndFlushLog(PWriteSet* const pwset) {
        for (uint64_t i = 0; i < numStores; i++) {
            pwset->plog[i].addr = log[i].addr;
            pwset->plog[i].val = log[i].val;
        }
        pwset->numStores = numStores;
        // Flush the log and the numStores variable
        flushFromTo(&pwset->numStores, &pwset->plog[numStores+1]);
    }

    // Uses the log to flush the modifications to NVM.
    // We assume tmtype does not cross cache line boundaries.
    inline void flushModifications() {
        for (uint64_t i = 0; i < numStores; i++) PWB(log[i].addr);
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
            tmtypebase<uint64_t>* tmte = (tmtypebase<uint64_t>*)e.addr;
            uint64_t lval = tmte->val.load(std::memory_order_acquire);
            uint64_t lseq = tmte->seq.load(std::memory_order_acquire);
            if (lseq < seq) DCAS((uint64_t*)e.addr, lval, lseq, e.val, seq);
        }
    }
};


// Forward declaration
struct OpData;
// This is used by addOrReplace() to know which OpDesc instance to use for the current transaction
extern thread_local OpData* tl_opdata;


// Its purpose is to hold thread-local data
struct OpData {
    uint64_t      curTx {0};              // Used during a transaction to keep the value of curTx read in beginTx() (owner thread only)
    uint64_t      nestedTrans {0};        // Thread-local: Number of nested transactions
    PWriteSet*    pWriteSet {nullptr};    // Pointer to the redo log in persistent memory
    uint64_t      padding[16-3];          // Padding to avoid false-sharing in nestedTrans and curTx
};


// Used to identify aborted transactions
struct AbortedTx {};
static constexpr AbortedTx AbortedTxException {};

class OneFileLF;
extern OneFileLF gOFLF;


/**
 * <h1> One-File PTM (Lock-Free) </h1>
 *
 * One-File is a Persistent Software Transacional Memory with lock-free progress, meant to
 * implement lock-free data structures. It has integrated lock-free memory
 * reclamation using an optimistic memory scheme
 *
 * OF is a word-based PTM and it uses double-compare-and-swap (DCAS).
 *
 * Right now it has several limitations, some will be fixed in the future, some may be hard limitations of this approach:
 * - We can't have stack allocated tmtype<> variables. For example, we can't created inside a transaction "tmtpye<uint64_t> tmp = a;",
 *   it will give weird errors because of stack allocation.
 * - We need DCAS but it can be emulated with LL/SC or even with single-word CAS
 *   if we do redirection to a (lock-free) pool with SeqPtrs;
 */
class OneFileLF {
private:
    static const bool                    debug = false;
    OpData                              *opData;
    int                                  fd {-1};

public:
    EsLoco<tmtype>                       esloco {};
    PMetadata*                           pmd {nullptr};
    std::atomic<uint64_t>*               curTx {nullptr};              // Pointer to persistent memory location of curTx (it's in PMetadata)
    WriteSet*                            writeSets;                    // Two write-sets for each thread

    OneFileLF() {
        opData = new OpData[REGISTRY_MAX_THREADS];
        writeSets = new WriteSet[REGISTRY_MAX_THREADS];
        mapPersistentRegion(PFILE_NAME, PREGION_ADDR, PREGION_SIZE);
    }

    ~OneFileLF() {
        delete[] opData;
        delete[] writeSets;
    }

    static std::string className() { return "OneFilePTM-LF"; }

    void mapPersistentRegion(const char* filename, uint8_t* regionAddr, const uint64_t regionSize) {
        // Check that the header with the logs leaves at least half the memory available to the user
        if (sizeof(PMetadata) > regionSize/2) {
            printf("ERROR: the size of the logs in persistent memory is so large that it takes more than half the whole persistent memory\n");
            printf("Please reduce some of the settings in OneFilePTMLF.hpp and try again\n");
            assert(false);
        }
        bool reuseRegion = false;
        // Check if the file already exists or not
        struct stat buf;
        if (stat(filename, &buf) == 0) {
            // File exists
            fd = open(filename, O_RDWR|O_CREAT, 0755);
            assert(fd >= 0);
            reuseRegion = true;
        } else {
            // File doesn't exist
            fd = open(filename, O_RDWR|O_CREAT, 0755);
            assert(fd >= 0);
            if (lseek(fd, regionSize-1, SEEK_SET) == -1) {
                perror("lseek() error");
            }
            if (write(fd, "", 1) == -1) {
                perror("write() error");
            }
        }
        // mmap() memory range
        void* got_addr = (uint8_t *)mmap(regionAddr, regionSize, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, 0);
        if (got_addr == MAP_FAILED || got_addr != regionAddr) {
            printf("got_addr = %p  instead of %p\n", got_addr, regionAddr);
            perror("ERROR: mmap() is not working !!! ");
            assert(false);
        }
        // Check if the header is consistent and only then can we attempt to re-use, otherwise we clear everything that's there
        pmd = reinterpret_cast<PMetadata*>(regionAddr);
        if (reuseRegion) reuseRegion = (pmd->id == PMetadata::MAGIC_ID);
        // Map pieces of persistent Metadata to pointers in volatile memory
        for (uint64_t i = 0; i < REGISTRY_MAX_THREADS; i++) opData[i].pWriteSet = &(pmd->plog[i]);
        curTx = &(pmd->curTx);
        // If the file has just been created or if the header is not consistent, clear everything.
        // Otherwise, re-use and recover to a consistent state.
        if (reuseRegion) {
            esloco.init(regionAddr+sizeof(PMetadata), regionSize-sizeof(PMetadata), false);
            //recover(); // Not needed on x86
        } else {
            // Start by resetting all tmtypes::seq in the metadata region
            std::memset(regionAddr, 0, sizeof(PMetadata));
            new (regionAddr) PMetadata();
            esloco.init(regionAddr+sizeof(PMetadata), regionSize-sizeof(PMetadata), true);
            PFENCE();
            pmd->id = PMetadata::MAGIC_ID;
            PWB(&pmd->id);
            PFENCE();
        }
    }

    // Progress Condition: lock-free
    // The while-loop retarts only if there was at least one other thread completing a transaction
    void beginTx(OpData& myopd, const int tid) {
        tl_is_read_only = true;
        while (true) {
            myopd.curTx = curTx->load(std::memory_order_acquire);
            helpApply(myopd.curTx, tid);
            // Reset the write-set after (possibly) helping another transaction complete
            writeSets[tid].numStores = 0;
            // Start over if there is already a new transaction
            if (myopd.curTx == curTx->load(std::memory_order_acquire)) return;
        }
    }

    // Progress condition: wait-free population-oblivious
    // Attempts to publish our write-set (commit the transaction) and then applies the write-set.
    // Returns true if my transaction was committed.
    inline bool commitTx(OpData& myopd, const int tid) {
        // If it's a read-only transaction, then commit immediately
        if (writeSets[tid].numStores == 0) return true;
        // Give up if the curTx has changed sinced our transaction started
        if (myopd.curTx != curTx->load(std::memory_order_acquire)) return false;
        // Move our request to OPEN, using the sequence of the previous transaction +1
        const uint64_t seq = trans2seq(myopd.curTx);
        const uint64_t newTx = seqidx2trans(seq+1,tid);
        myopd.pWriteSet->request.store(newTx, std::memory_order_release);
        // Copy the write-set to persistent memory and flush it
        writeSets[tid].persistAndFlushLog(myopd.pWriteSet);
        // Attempt to CAS curTx to our OpDesc instance (tid) incrementing the seq in it
        uint64_t lcurTx = myopd.curTx;
        if (debug) printf("tid=%i  attempting CAS on curTx from (%ld,%ld) to (%ld,%ld)\n", tid, trans2seq(lcurTx), trans2idx(lcurTx), seq+1, (uint64_t)tid);
        if (!curTx->compare_exchange_strong(lcurTx, newTx)) return false;
        PWB(curTx);
        // Execute each store in the write-set using DCAS() and close the request
        helpApply(newTx, tid);
        // We should need a PSYNC() here to provide durable linearizabilty, but the CAS of the state in helpApply() acts as a PSYNC() (on x86).
        if (debug) printf("Committed transaction (%ld,%ld) with %ld stores\n", seq+1, (uint64_t)tid, writeSets[tid].numStores);
        return true;
    }

    // Same as beginTx/endTx transaction, but with lambdas, and it handles AbortedTx exceptions
    template<typename R, typename F> R transaction(F&& func) {
        const int tid = ThreadRegistry::getTID();
        OpData& myopd = opData[tid];
        if (myopd.nestedTrans > 0) return func();
        ++myopd.nestedTrans;
        tl_opdata = &myopd;
        R retval {};
        while (true) {
            beginTx(myopd, tid);
            try {
                retval = func();
            } catch (AbortedTx&) {
                continue;
            }
            if (commitTx(myopd, tid)) break;
        }
        tl_opdata = nullptr;
        --myopd.nestedTrans;
        return retval;
    }

    // Same as above, but returns void
    template<typename F> void transaction(F&& func) {
        const int tid = ThreadRegistry::getTID();
        OpData& myopd = opData[tid];
        if (myopd.nestedTrans > 0) {
            func();
            return;
        }
        ++myopd.nestedTrans;
        tl_opdata = &myopd;
        while (true) {
            beginTx(myopd, tid);
            try {
                func();
            } catch (AbortedTx&) {
                continue;
            }
            if (commitTx(myopd, tid)) break;
        }
        tl_opdata = nullptr;
        --myopd.nestedTrans;
    }

    // It's silly that these have to be static, but we need them for the (SPS) benchmarks due to templatization
    template<typename R, typename F> static R updateTx(F&& func) { return gOFLF.transaction<R>(func); }
    template<typename R, typename F> static R readTx(F&& func) { return gOFLF.transaction<R>(func); }
    template<typename F> static void updateTx(F&& func) { gOFLF.transaction(func); }
    template<typename F> static void readTx(F&& func) { gOFLF.transaction(func); }

    template <typename T, typename... Args> static T* tmNew(Args&&... args) {
    //template <typename T> static T* tmNew() {
        T* ptr = (T*)gOFLF.esloco.malloc(sizeof(T));
        //new (ptr) T;  // new placement
        new (ptr) T(std::forward<Args>(args)...);
        return ptr;
    }

    template<typename T> static void tmDelete(T* obj) {
        if (obj == nullptr) return;
        obj->~T(); // Execute destructor as part of the current transaction
        tmFree(obj);
    }

    static void* tmMalloc(size_t size) {
        if (tl_opdata == nullptr) {
            printf("ERROR: Can not allocate outside a transaction\n");
            return nullptr;
        }
        void* obj = gOFLF.esloco.malloc(size);
        return obj;
    }

    static void tmFree(void* obj) {
        if (obj == nullptr) return;
        if (tl_opdata == nullptr) {
            printf("ERROR: Can not de-allocate outside a transaction\n");
            return;
        }
        gOFLF.esloco.free(obj);
    }

    static void* pmalloc(size_t size) {
        return gOFLF.esloco.malloc(size);
    }

    static void pfree(void* obj) {
        if (obj == nullptr) return;
        gOFLF.esloco.free(obj);
    }

    template <typename T> static inline T* get_object(int idx) {
        tmtype<T*>* ptr = (tmtype<T*>*)&(gOFLF.pmd->rootPtrs[idx]);
        return ptr->pload();
    }

    template <typename T> static inline void put_object(int idx, T* obj) {
        tmtype<T*>* ptr = (tmtype<T*>*)&(gOFLF.pmd->rootPtrs[idx]);
        ptr->pstore(obj);
    }

private:
    // Progress condition: wait-free population oblivious
    inline void helpApply(uint64_t lcurTx, const int tid) {
        const uint64_t idx = trans2idx(lcurTx);
        const uint64_t seq = trans2seq(lcurTx);
        OpData& opd = opData[idx];
        // Nothing to apply unless the request matches the curTx
        if (lcurTx != opd.pWriteSet->request.load(std::memory_order_acquire)) return;
        if (idx != tid) {
            // Make a copy of the write-set and check if it is consistent
            writeSets[tid] = writeSets[idx];
            std::atomic_thread_fence(std::memory_order_acquire);
            if (lcurTx != curTx->load()) return;
            if (lcurTx != opd.pWriteSet->request.load(std::memory_order_acquire)) return;
        }
        if (debug) printf("Applying %ld stores in write-set\n", writeSets[tid].numStores);
        writeSets[tid].apply(seq, tid);
        writeSets[tid].flushModifications();
        const uint64_t newReq = seqidx2trans(seq+1,idx);
        if (opd.pWriteSet->request.load(std::memory_order_acquire) == lcurTx) {
            opd.pWriteSet->request.compare_exchange_strong(lcurTx, newReq);
        }
    }

    // Upon restart, re-applies the last transaction, so as to guarantee that
    // we have a consistent state in persistent memory.
    // This is not needed on x86, where the DCAS has atomicity writting to persistent memory.
    void recover() {
        uint64_t lcurTx = curTx->load(std::memory_order_acquire);
        opData[trans2idx(lcurTx)].pWriteSet->applyFromRecover();
        PSYNC();
    }
};



// T is typically a pointer to a node, but it can be integers or other stuff, as long as it fits in 64 bits
template<typename T> struct tmtype : tmtypebase<T> {
    tmtype() { }

    tmtype(T initVal) { pstore(initVal); }

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
        tmtypebase<T>::val.store((uint64_t)newVal, std::memory_order_relaxed);
    }

    // We don't need to check curTx here because we're not de-referencing
    // the val. It's only after a load() that the val may be de-referenced
    // (in user code), therefore we do the check on load() only.
    inline void pstore(T newVal) {
        OpData* const myopd = tl_opdata;
        if (myopd == nullptr) { // Looks like we're outside a transaction
            tmtypebase<T>::val.store((uint64_t)newVal, std::memory_order_relaxed);
        } else {
            gOFLF.writeSets[tl_tcico.tid].addOrReplace(this, (uint64_t)newVal);
        }
    }

    // We have to check if there is a new ongoing transaction and if so, abort
    // this execution immediately for two reasons:
    // 1. Memory Reclamation: the val we're returning may be a pointer to an
    // object that has since been retired and deleted, therefore we can't allow
    // user code to de-reference it;
    // 2. Invariant Conservation: The val we're reading may be from a newer
    // transaction, which implies that it may break an invariant in the user code.
    // See examples of invariant breaking in this post:
    // http://concurrencyfreaks.com/2013/11/stampedlocktryoptimisticread-and.html
    inline T pload() const {
        T lval = (T)tmtypebase<T>::val.load(std::memory_order_acquire);
        OpData* const myopd = tl_opdata;
        if (myopd == nullptr) return lval;
        if ((uint8_t*)this < PREGION_ADDR || (uint8_t*)this > PREGION_END) return lval;
        uint64_t lseq = tmtypebase<T>::seq.load(std::memory_order_acquire);
        if (lseq > trans2seq(myopd->curTx)) throw AbortedTxException;
        if (tl_is_read_only) return lval;
        return (T)gOFLF.writeSets[tl_tcico.tid].lookupAddr(this, (uint64_t)lval);
    }
};


//
// Wrapper methods to the global TM instance. The user should use these:
//
template<typename R, typename F> static R updateTx(F&& func) { return gOFLF.transaction<R>(func); }
template<typename R, typename F> static R readTx(F&& func) { return gOFLF.transaction<R>(func); }
template<typename F> static void updateTx(F&& func) { gOFLF.transaction(func); }
template<typename F> static void readTx(F&& func) { gOFLF.transaction(func); }
template<typename T, typename... Args> T* tmNew(Args&&... args) { return OneFileLF::tmNew<T>(std::forward<Args>(args)...); }
template<typename T> void tmDelete(T* obj) { OneFileLF::tmDelete<T>(obj); }
template<typename T> static T* get_object(int idx) { return OneFileLF::get_object<T>(idx); }
template<typename T> static void put_object(int idx, T* obj) { OneFileLF::put_object<T>(idx, obj); }
inline static void* tmMalloc(size_t size) { return OneFileLF::tmMalloc(size); }
inline static void tmFree(void* obj) { OneFileLF::tmFree(obj); }


//
// Place these in a .cpp if you include this header from multiple files (compilation units)
//
OneFileLF gOFLF {};
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

}
#endif /* _PERSISTENT_ONE_FILE_LOCK_FREE_TRANSACTIONAL_MEMORY_H_ */
