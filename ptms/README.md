We have in here the PTMs (Persistent Transactional Memories) and their wrappers.

### PMDK ###
Also known as NVML, it is undo-log based.
Uses a regular pthread_rwlock_t for concurrency.
Blocking progress.
You need to install this library from github.

### RomulusLog ###
For persistence, uses the Romulus technique with volatile redo log.
Uses a C-RW-WP reader-writer lock with integrated flat-combining.
Blocking with starvation-free progress for writers.
Uses 0x7fdd40000000 by default as the mapping address.

### RomulusLR ###
For persistence, uses the Romulus technique with volatile redo log.
For concurrency, uses the Left-Right universal construct with integrated flat-combining.
Blocking with starvation-free progress for writers and wait-free population oblivious for readers.
Uses 0x7fdd80000000 by default as the mapping address.

### PTM OneFile Lock-Free ###
Implementation of the OneFile STM (Lock-Free) with persistent memory support. Does 2 fences per transaction.
This is redo-log based and it uses EsLoco memory allocator.
Has lock-free progress for all transactions.
Does not use pfences.h
Uses 0x7fea00000000 by default as the mapping address.

### PTM OneFile Wait-Free ###
Implementation of the OneFile STM (Wait-Free) with persistent memory support. Does 2 fences per transaction.
This is redo-log based and it uses EsLoco memory allocator.
Has wait-free bounded progress for all transactions.
Does not use pfences.h
Uses 0x7feb00000000 by default as the mapping address.

The pfences.h file contains the definitions of the PWB(), PFENCE() and PSYNC() macros for Romulus, depending on the target cpu.
PMDK detects these at runtime, using the best possible one.
The concept of pwb/pfence/psync comes from the paper "Linearizability of Persistent Memory Objects Under a Full-System-Crash Failure Model" by Joseph Izraelevitz, Hammurabi Mendes, Michael L. Scott.
