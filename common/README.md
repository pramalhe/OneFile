Here are some files that are needed by other libraries and data structures:

    HazardEras.hpp              Used by some of the lock-free data structures for memory reclamation
    HazardPointers.hpp          Used by some of the lock-free data structures for memory reclamation
    HazardPointersSimQueue.hpp  Used by SimQueue for memory reclamation. Notice that the original SimQueue implementation in C does not ha memory reclamation. This implementation in C++ with this modified version of Hazard Pointers was done by Correia and Ramalhete
    pfences.h                   Used by Romulus
    RIStaticPerThread.hpp       Used by Romulus
    ThreadRegistry.cpp          Used by Romulus
    ThreadRegistry.hpp          Used by Romulus