/******************************************************************************
 * Copyright (c) 2017-2019, Pedro Ramalhete, Andreia Correia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Concurrency Freaks nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.

 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************
 */

#ifndef _C_RW_WP_TRANSACTIONAL_MEMORY_H_
#define _C_RW_WP_TRANSACTIONAL_MEMORY_H_

#include <atomic>
#include <thread>
#include <cassert>
#include <functional>

/* <h1> C-RW-WP Software Transactional Memory </h1>
 *
 * This is a Transactional Memory that uses C-RW-WP plus Flat-Combining.
 * It is blocking but with starvation-freedom.
 *
 * Transactions are irrevocable, therefore we don't provide an API for
 * aborting transactions.
 *
 * We have put everything in this header so that the end-user can include a
 * single header and gets a working Software Transaction Memory.
 * It isn't pretty, but it makes life easier for application developers.
 * And yes, it's kind of silly to make an STM out of a global lock, but we did
 * it to have a "baseline" against which to compare other STMs.
 *
 * C-RW-WP paper:                            http://dl.acm.org/citation.cfm?id=2442532
 * Flat Combining paper:                     http://dl.acm.org/citation.cfm?id=1810540
 * A post about C-RW-WP with Flat Combining: http://concurrencyfreaks.com/2017/07/left-right-and-c-rw-wp-with-flat.html
 *
 * We have the following classes in this file:
 * tmtype<T>         -> Annotation for TM types (not really needed but useful for the benchmarks)
 * CRWWPTM           -> The singleton TM
 *
 * How to use this TM:
 * crwwptm::tmtype<UserObject> obj {};
 * crwwptm::writeTx([&obj] () {obj.mutative_method()} );
 * crwwptm::readTx([&obj] () {obj.non_mutative_method()} );
 *
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
namespace crwwpstm {


// Needed by the TM benchmarks and tests infrastructure
template<typename T>
struct tmtype {
    T val {};

    tmtype() { }

    tmtype(T initVal) : val{initVal} {}

    // Casting operator
    operator T() {
        return load();
    }

    // Prefix increment operator: ++x
    void operator++ () {
        store(load()+1);
    }

    // Prefix decrement operator: --x
    void operator-- () {
        store(load()-1);
    }

    void operator++ (int) {
        store(load()+1);
    }

    void operator-- (int) {
        store(load()-1);
    }

    // Equals operator: first downcast to T and then compare
    bool operator == (const T& otherval) const {
        return load() == otherval;
    }

    // Difference operator: first downcast to T and then compare
    bool operator != (const T& otherval) const {
        return load() != otherval;
    }

    // Operator arrow ->
    T operator->() {
        return load();
    }

    // Copy constructor
    tmtype<T>(const tmtype<T>& other) {
        store(other.load());
    }

    // Assignment operator from an tmtype
    tmtype<T>& operator=(const tmtype<T>& other) {
        store(other.load());
        return *this;
    }

    // Assignment operator from a value
    tmtype<T>& operator=(T value) {
        store(value);
        return *this;
    }

    inline void store(T newVal) {
        val = newVal;
    }

    inline T load() const {
        return val;
    }
};


// Needed by the TM benchmarks and tests infrastructure
struct tmbase { };


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

extern thread_local ThreadCheckInCheckOut tl_gc_tcico;

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
public:
    static const int                    REGISTRY_MAX_THREADS = 128;

private:
    alignas(128) std::atomic<bool>      usedTID[REGISTRY_MAX_THREADS];   // Which TIDs are in use by threads
    alignas(128) std::atomic<int>       maxTid {-1};                     // Highest TID (+1) in use by threads

public:
    ThreadRegistry() {
        for (int it = 0; it < REGISTRY_MAX_THREADS; it++) {
            usedTID[it].store(false, std::memory_order_relaxed);
        }
    }

    // Progress Condition: wait-free bounded (by the number of threads)
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
            tl_gc_tcico.tid = tid;
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
        int tid = tl_gc_tcico.tid;
        if (tid != ThreadCheckInCheckOut::NOT_ASSIGNED) return tid;
        return gThreadRegistry.register_thread_new();
    }
};

// Forward declaration to be able to have static methods for updateTx() and readTx()
class CRWWPSTM;
extern CRWWPSTM gCRWWPSTM;


class CRWWPSTM {

private:
    class RIStaticPerThread {
    private:
        static const uint64_t NOT_READING = 0;
        static const uint64_t READING = 1;
        static const int CLPAD = 128/sizeof(uint64_t);
        const int maxThreads;
        alignas(128) std::atomic<uint64_t>* states;

    public:
        RIStaticPerThread(int maxThreads) : maxThreads{maxThreads} {
            states = new std::atomic<uint64_t>[maxThreads*CLPAD];
            for (int tid = 0; tid < maxThreads; tid++) {
                states[tid*CLPAD].store(NOT_READING, std::memory_order_relaxed);
            }
        }

        ~RIStaticPerThread() {
            delete[] states;
        }

        inline void arrive(const int tid) noexcept {
            states[tid*CLPAD].store(READING);
        }

        inline void depart(const int tid) noexcept {
            states[tid*CLPAD].store(NOT_READING, std::memory_order_release);
        }

        inline bool isEmpty() noexcept {
            for (int tid = 0; tid < ThreadRegistry::getMaxThreads(); tid++) {
                if (states[tid*CLPAD].load() != NOT_READING) return false;
            }
            return true;
        }
    };

    static const int CLPAD = 128/sizeof(uintptr_t);
    static const int MAX_THREADS = 1024;
    static const int LOCKED = 1;
    static const int UNLOCKED = 0;
    // Stuff use by the Flat Combining mechanism
    alignas(128) std::atomic< std::function<uint64_t()>* >* fc;
    alignas(128) uint64_t* results;
    alignas(128) std::atomic<int> cohort { UNLOCKED };
    RIStaticPerThread ri { MAX_THREADS };

public:
    CRWWPSTM(const int maxThreads=0) {
        fc = new std::atomic< std::function<uint64_t()>* >[MAX_THREADS*CLPAD];
        results = new uint64_t[MAX_THREADS*CLPAD];
        for (int i = 0; i < MAX_THREADS; i++) {
            fc[i*CLPAD].store(nullptr, std::memory_order_relaxed);
        }
    }

    ~CRWWPSTM() {
        delete[] fc;
        delete[] results;
    }

    static std::string className() { return "CRWWPSTM"; }

    template<typename R, typename F> static R updateTx(F&& func) { return gCRWWPSTM.ns_updateTx<R>(func); }
    template<typename R, typename F> static R readTx(F&& func) { return gCRWWPSTM.ns_readTx<R>(func); }
    template<typename F> static void updateTx(F&& func) { gCRWWPSTM.ns_updateTx(func); }
    template<typename F> static void readTx(F&& func) { gCRWWPSTM.ns_readTx(func); }


    // Progress: Blocking (starvation-free)
    template<typename R, typename F> R ns_updateTx(F&& mutativeFunc) {
        const int tid = ThreadRegistry::getTID();
        std::function<R()> uf = {mutativeFunc}; // Use a stack allocated std::function instead of heap allocated
        std::function<uint64_t()>* myfunc = (std::function<uint64_t()>*)&uf;
        // Add our mutation to the array of flat combining
        fc[tid*CLPAD].store(myfunc, std::memory_order_release);
        // Lock writersMutex
        while (true) {
            int unlocked = UNLOCKED;
            if (cohort.load() == UNLOCKED &&
                cohort.compare_exchange_strong(unlocked, LOCKED)) break;
            // Check if another thread executed my mutation
            if (fc[tid*CLPAD].load(std::memory_order_acquire) == nullptr) return (R)(results[tid*CLPAD]);
            std::this_thread::yield();
        }
        // Wait for all the readers to depart
        while (!ri.isEmpty()) {
            // Check if another thread executed my mutation
            if (fc[tid*CLPAD].load(std::memory_order_acquire) == nullptr) {
                cohort.store(UNLOCKED, std::memory_order_release);
                return (R)results[tid*CLPAD];
            }
            // spin
        }
        // Save a local copy of the flat combining array
        bool somethingToDo = false;
        const int maxThreads = ThreadRegistry::getMaxThreads();
        std::function<uint64_t()>* lfc[maxThreads];
        for (int i = 0; i < maxThreads; i++) {
            lfc[i] = fc[i*CLPAD].load(std::memory_order_acquire);
            if (lfc[i] != nullptr) somethingToDo = true;
        }
        // Check if there is at least one operation to apply
        if (somethingToDo) {
            // For each mutation in the flat combining array, apply it in the order
            // of the array, save the result, and set the entry in the array to nullptr
            for (int i = 0; i < maxThreads; i++) {
                auto mutation = fc[i*CLPAD].load(std::memory_order_acquire);
                if (mutation == nullptr) continue;
                results[i*CLPAD] = (uint64_t)((*mutation)());
                fc[i*CLPAD].store(nullptr, std::memory_order_release);
            }
        }
        // unlock()
        cohort.store(UNLOCKED, std::memory_order_release);
        return (R)(results[tid*CLPAD]);
    }

    // Progress: Blocking (starvation-free)
    template<typename F> void ns_updateTx(F&& mutativeFunc) {
        const int tid = ThreadRegistry::getTID();
        std::function<void()> vf {mutativeFunc};  // Use a stack allocated std::function instead of heap allocated
        std::function<uint64_t()>* myfunc = (std::function<uint64_t()>*)&vf;
        // Add our mutation to the array of flat combining
        fc[tid*CLPAD].store(myfunc);
        // Lock writersMutex
        while (true) {
            int unlocked = UNLOCKED;
            if (cohort.load() == UNLOCKED &&
                cohort.compare_exchange_strong(unlocked, LOCKED)) break;
            // Check if another thread executed my mutation
            if (fc[tid*CLPAD].load(std::memory_order_acquire) == nullptr) return;
            std::this_thread::yield();
        }
        // Wait for all the readers to depart
        while (!ri.isEmpty()) {
            // Check if another thread executed my mutation
            if (fc[tid*CLPAD].load(std::memory_order_acquire) == nullptr) {
                cohort.store(UNLOCKED, std::memory_order_release);
                return;
            }
            // spin
        }
        // Save a local copy of the flat combining array
        bool somethingToDo = false;
        const int maxThreads = ThreadRegistry::getMaxThreads();
        std::function<uint64_t()>* lfc[maxThreads];
        for (int i = 0; i < maxThreads; i++) {
            lfc[i] = fc[i*CLPAD].load(std::memory_order_acquire);
            if (lfc[i] != nullptr) somethingToDo = true;
        }
        // Check if there is at least one operation to apply
        if (somethingToDo) {
            // For each mutation in the flat combining array, apply it in the order
            // of the array, save the result, and set the entry in the array to nullptr
            for (int i = 0; i < maxThreads; i++) {
                auto mutation = fc[i*CLPAD].load(std::memory_order_acquire);
                if (mutation == nullptr) continue;
                results[i*CLPAD] = (uint64_t)((*mutation)());
                fc[i*CLPAD].store(nullptr, std::memory_order_release);
            }
        }
        // unlock()
        cohort.store(UNLOCKED, std::memory_order_release);
    }

    // Progress: Blocking (starvation-free)
    template<typename R, typename F> R ns_readTx(F&& readFunc) {
        const int tid = ThreadRegistry::getTID();
        bool announced = false;
        std::function<uint64_t()> myfunc = readFunc;
        // lock()
        while (true) {
            ri.arrive(tid);
            if (cohort.load() == UNLOCKED) break;
            ri.depart(tid);
            if (!announced) {
                // Put my operation in the flat combining array for a Writer to do it
                fc[tid*CLPAD].store(&myfunc, std::memory_order_release);
                announced = true;
            }
            // If a Writer set our entry to nullptr then the result is ready
            while (cohort.load() == LOCKED) {
                if (fc[tid*CLPAD].load(std::memory_order_acquire) == nullptr) {
                    return (R)(results[tid*CLPAD]);
                }
                std::this_thread::yield();  // pause()
            }
        }
        // Execute our read-only function
        R result = readFunc();
        if (announced) fc[tid*CLPAD].store(nullptr, std::memory_order_relaxed);
        // unlock()
        ri.depart(tid);
        return result;
    }

    template<typename F> void ns_readTx(F&& readFunc) {
        const int tid = ThreadRegistry::getTID();
        bool announced = false;
        std::function<void()> myvoidfunc = readFunc;
        std::function<uint64_t()> *myfunc = (std::function<uint64_t()>*)&myvoidfunc;
        // lock()
        while (true) {
            ri.arrive(tid);
            if (cohort.load() == UNLOCKED) break;
            ri.depart(tid);
            if (!announced) {
                // Put my operation in the flat combining array for a Writer to do it
                fc[tid*CLPAD].store(myfunc, std::memory_order_release);
                announced = true;
            }
            // If a Writer set our entry to nullptr then the result is ready
            while (cohort.load() == LOCKED) {
                if (fc[tid*CLPAD].load(std::memory_order_acquire) == nullptr) {
                    return;
                }
                std::this_thread::yield();  // pause()
            }
        }
        // Execute our read-only function
        readFunc();
        if (announced) fc[tid*CLPAD].store(nullptr, std::memory_order_relaxed);
        // unlock()
        ri.depart(tid);
    }


    template <typename T, typename... Args>
    T* tmNew(Args&&... args) {
        return new T(std::forward<Args>(args)...);
    }

    template<typename T>
    void tmDelete(T* obj) {
        delete obj;
    }

    // We snap a tmbase at the beginning of the allocation
    static void* tmMalloc(size_t size) {
        uint8_t* ptr = (uint8_t*)std::malloc(size+sizeof(tmbase));
        return ptr + sizeof(tmbase);
    }

    // We assume there is a tmbase allocated in the beginning of the allocation
    static void tmFree(void* obj) {
        if (obj == nullptr) return;
        uint8_t* ptr = (uint8_t*)obj - sizeof(tmbase);
        std::free(ptr);  // Outside a transaction, just free the object
    }

};


extern CRWWPSTM gCRWWPSTM;



template<typename R, typename F> static R updateTx(F&& func) { return gCRWWPSTM.updateTx<R>(func); }
template<typename R, typename F> static R readTx(F&& func) { return gCRWWPSTM.readTx<R>(func); }
template<typename F> static void updateTx(F&& func) { gCRWWPSTM.updateTx(func); }
template<typename F> static void readTx(F&& func) { gCRWWPSTM.readTx(func); }
template<typename T, typename... Args> static T* tmNew(Args&&... args) { return gCRWWPSTM.tmNew<T>(args...); }
template<typename T> static void tmDelete(T* obj) { gCRWWPSTM.tmDelete<T>(obj); }
inline void* tmMalloc(size_t size) { return CRWWPSTM::tmMalloc(size); }
inline void* tmFree(void* obj) { CRWWPSTM::tmFree(obj); }



//
// Place these in a .cpp if you include this header in multiple files
//
CRWWPSTM gCRWWPSTM {};
// Global/singleton to hold all the thread registry functionality
ThreadRegistry gThreadRegistry {};
// This is where every thread stores the tid it has been assigned when it calls getTID() for the first time.
// When the thread dies, the destructor of ThreadCheckInCheckOut will be called and de-register the thread.
thread_local ThreadCheckInCheckOut tl_gc_tcico {};
// Helper function for thread de-registration
void thread_registry_deregister_thread(const int tid) {
    gThreadRegistry.deregister_thread(tid);
}

} // end of namespace crwwp

#endif /* _C_RW_WP_TRANSACTIONAL_MEMORY_H_ */
