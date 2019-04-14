/*
 * Copyright 2017-2018
 *   Andreia Correia <andreia.veiga@unine.ch>
 *   Pedro Ramalhete <pramalhe@gmail.com>
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Nachshon Cohen <nachshonc@gmail.com>
 *
 * This work is published under the MIT license. See LICENSE.txt
 */
#ifndef _ELASTIC_STM_TRANSACTIONAL_MEMORY_WRAPPER_H_
#define _ELASTIC_STM_TRANSACTIONAL_MEMORY_WRAPPER_H_

#include <atomic>
#include <cassert>
#include <iostream>
#include <vector>
#include <functional>


namespace estm {

// Compile with explicit calls to ESTM
#include "estm-0.3.0/include/stm.h"
#include "estm-0.3.0/include/mod_mem.h"


struct tmbase {
};


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
        stm_exit_thread();    // Needed by ESTM
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
        stm_init_thread();   // Needed by TinySTM
        return gThreadRegistry.register_thread_new();
    }
};



class ESTM;
extern ESTM gESTM;

class ESTM {

private:
    // Maximum number of participating threads
    static const uint64_t MAX_THREADS = 128;

public:
    ESTM(unsigned int maxThreads=MAX_THREADS) {
        stm_init();
        mod_mem_init();
    }

    ~ESTM() {
        stm_exit();
    }

    static std::string className() { return "ESTM"; }

    template<typename R, class F>
    static R updateTx(F&& func) {
        ThreadRegistry::getTID();
        sigjmp_buf *_e = stm_get_env();
        sigsetjmp(*_e, 0);
        stm_start(_e, 0, NL);
        R retval = func();
        stm_commit();
        return retval;
    }

    template<class F>
    static void updateTx(F&& func) {
        ThreadRegistry::getTID();
        sigjmp_buf *_e = stm_get_env();
        sigsetjmp(*_e, 0);
        stm_start(_e, 0, NL);
        func();
        stm_commit();
    }

    template<typename R, class F>
    static R readTx(F&& func) {
        ThreadRegistry::getTID();
        sigjmp_buf *_e = stm_get_env();
        sigsetjmp(*_e, 0);
        stm_start(_e, 0, NL);
        R retval = func();
        stm_commit();
        return retval;
    }

    template<class F>
    static void readTx(F&& func) {
        ThreadRegistry::getTID();
        sigjmp_buf *_e = stm_get_env();
        sigsetjmp(*_e, 0);
        stm_start(_e, 0, NL);
        func();
        stm_commit();
    }

    template <typename T, typename... Args>
    static T* tmNew(Args&&... args) {
        void* addr = stm_malloc(sizeof(T));
        assert(addr != NULL);
        T* ptr = new (addr) T(std::forward<Args>(args)...);
        return ptr;
    }

    template<typename T>
    static void tmDelete(T* obj) {
        if (obj == nullptr) return;
        obj->~T();
        // Reset memory block to avoid allocator metadata corruption
        // TODO: be careful with the last word, the object better be a word multiple otherwise this can corrupt data from the next object
        stm_word_t* wptr = (stm_word_t *)obj;
        for (int i = 0; i < sizeof(T)/sizeof(stm_word_t) ;i++, wptr++) stm_store(wptr, 0);
        stm_free(obj, sizeof(T));
    }

    static void* tmMalloc(size_t size) {
        return stm_malloc(size);
    }

    static void tmFree(void* obj) {
        stm_free(obj, 0); // TODO: is it ok to pass zero here?
    }
};



// T is typically a pointer to a node, but it can be integers or other stuff, as long as it fits in 64 bits
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

    // Relational operators
    bool operator < (const T& rhs) {
        return load() < rhs;
    }
    bool operator > (const T& rhs) {
        return load() > rhs;
    }
    bool operator <= (const T& rhs) {
        return load() <= rhs;
    }
    bool operator >= (const T& rhs) {
        return load() >= rhs;
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
        stm_store((stm_word_t *)&val, (stm_word_t)newVal);
    }

    // Meant to be called when know we're the only ones touching
    // these contents, for example, in the constructor of an object, before
    // making the object visible to other threads.
    inline void isolated_store(T newVal) {
        val = newVal;
    }

    inline T load() const {
        return (T)stm_load((stm_word_t *)&val);
    }
};

extern ESTM gESTM;

// Wrapper methods to the global TM instance. The user should use these:
template<typename R, typename F> R updateTx(F&& mutativeFunc) { return gESTM.updateTx<R>(mutativeFunc); }
template<typename R, typename F> R readTx(F&& readFunc) { return gESTM.readTx<R>(readFunc); }
template<typename F> void updateTx(F&& mutativeFunc) { gESTM.updateTx(mutativeFunc); }
template<typename F> void readTx(F&& readFunc) { gESTM.readTx(readFunc); }

// Wrapper to not do any transaction
template<typename R, typename Func>
R notx(const int tid, Func&& func) {
    return func();
}

template<typename T, typename... Args> T* tmNew(Args&&... args) { return gESTM.tmNew<T>(args...); }
template<typename T> void tmDelete(T* obj) { gESTM.tmDelete<T>(obj); }
static void* tmMalloc(size_t size) { return stm_malloc(size); }
static void tmFree(void* obj) { stm_free(obj, 0); } // TODO: is it ok to pass zero here?

static int getTID(void) { return ThreadRegistry::getTID(); }


//
// Place these in a .cpp if you include this header in multiple files
//
ESTM gESTM {};
// Global/singleton to hold all the thread registry functionality
ThreadRegistry gThreadRegistry {};
// This is where every thread stores the tid it has been assigned when it calls getTID() for the first time.
// When the thread dies, the destructor of ThreadCheckInCheckOut will be called and de-register the thread.
thread_local ThreadCheckInCheckOut tl_gc_tcico {};
// Helper function for thread de-registration
void thread_registry_deregister_thread(const int tid) {
    gThreadRegistry.deregister_thread(tid);
}



}

#endif /* _ELASTIC_STM_TRANSACTIONAL_MEMORY_WRAPPER_H_ */
