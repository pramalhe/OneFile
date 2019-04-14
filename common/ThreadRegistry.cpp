/*
 * Contains all global variables.
 */
#include "common/ThreadRegistry.hpp"

// Global/singleton to hold all the thread registry functionality
ThreadRegistry gThreadRegistry {};

// This is where every thread stores the tid it has been assigned when it calls getTID() for the first time.
// When the thread dies, the destructor of ThreadCheckInCheckOut will be called and de-register the thread.
thread_local ThreadCheckInCheckOut tl_tcico {};

void thread_registry_deregister_thread(const int tid) {
    gThreadRegistry.deregister_thread(tid);
}
