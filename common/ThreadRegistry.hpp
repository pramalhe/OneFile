#ifndef _THREAD_REGISTRY_H_
#define _THREAD_REGISTRY_H_

#include <atomic>
#include <thread>
#include <iostream>
#include <cassert>

// Increase this if 128 threads is not enough
static const int REGISTRY_MAX_THREADS = 128;


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
 * RomulusLR relies on this to work properly.
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

    /*
     * Progress Condition: wait-free bounded (by the number of threads)
     */
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

    /*
     * Progress condition: wait-free population oblivious
     */
    inline void deregister_thread(const int tid) {
        usedTID[tid].store(false, std::memory_order_release);
    }

    /*
     * Progress condition: wait-free population oblivious
     */
    static inline uint64_t getMaxThreads(void) {
        return gThreadRegistry.maxTid.load(std::memory_order_acquire);
    }

    /*
     * Progress condition: wait-free bounded (by the number of threads)
     */
    static inline int getTID(void) {
        int tid = tl_tcico.tid;
        if (tid != ThreadCheckInCheckOut::NOT_ASSIGNED) return tid;
        return gThreadRegistry.register_thread_new();
    }
};

#endif /* _THREAD_REGISTRY_H_ */
