
# OneFile PTM / STM

OneFile is a Software Transactional Memory (STM) meant to make it easy to implement lock-free and wait-free data structures.
It is based on the paper "[OneFile: A Wait-free Persistent Transactional Memory](https://github.com/pramalhe/OneFile/blob/master/OneFile-2019.pdf)" by Ramalhete, Correia, Felber and Cohen
https://github.com/pramalhe/OneFile/blob/master/OneFile-2019.pdf

It provides multi-word atomic updates on *tmtype<T>* objects, where T must be word-size, typically a pointer or integer.
During a transaction, each store on an *tmtpye<T>* is transformed into a double-word-compare-and-swap DCAS() and one more regular CAS() is done to complete the transaction. It does this with a store-log (write-set) which other writers can help apply. 
This is a "redo-log" based technique, which means that both store and loads need to be interposed. Stores will be interposed to save them in the log and loads will be interposed to lookup on the log the most recent value.
If there is a transaction currently ongoing, the readers will have to check on each *tmtype::pload()* if the variable we're tying to read is part of the current transaction. If a value is read whose 'seq' is higher than the transaction we initially read, the whole read-only operation will be restarted, by throwing an exception in the *tmtype::pload()* interposing method and catching this exception by the TM. All of this logic is handled internally by OF without any explicit user interaction.

Because of operator overloading, the assignment and reading of *tmtype<T>* types is done transparently to the user, with a pure library implementation, without any need for compiler instrumentation. This means that the user can write the code as if it was a sequential implementation of the data structure, apart from the change of types (type annotation). In this sense, OneFile is a "quasi-universal construction" with lock-free progress.

Our design goal with OneFile was to provide a non-blocking STM so that non-experts could implement their own lock-free and wait-free data structures.
OneFile is not designed to transform regular everyday code into lock-free applications. Such use-cases require a lot more of engineering work and likely a completely different approach from what we took with OneFile (CX is a much better option for that purpose).

We've made two implementations in the form of Persistent Transactional Memory (PTM) which are STMs meant for Persistent Memory, like Intel's Optane DC Persistent Memory.

We've implemented four diferent variants of this design:
- OneFile-LF: The simplest of the four, has lock-free progress and lock-free memory reclamation using Hazard Eras;
- OneFile-WF: Uses aggregation (like flat-combining) and a new wait-free consensus to provide wait-free bounded progress. Has wait-free bounded memory reclamation;
- POneFile-LF: A PTM with durable transactions (ACID) and lock-free progress. Memory reclamation is lock-free using an optimistic technique. Allocation and de-allocation of user objects is lock-free;
- POnefile-WF: A PTM with durable transactions (ACID) and wait-free progress. Memory reclamation for user objects is wait-free using an optimistic technique, while memory reclamation of the transactional objects is done using Hazard Eras, also wait-free. Allocation and de-allocation of user-objects is wait-free;

See the respective .hpp files for implementation details.

Each implementation is a single header file. Yes, it's that small  :)


## Quickstart ##

If you just want to use OneFile in your own application or benchmarks then follow these steps:
- Choose one of the four OneFile implementations, depending on whether you want and STM, a PTM, lock-free or wait-free progress:  
  [stms/OneFileLF.hpp](https://github.com/pramalhe/OneFile/blob/master/stms/OneFileLF.hpp)      STM with lock-free transactions  
  [stms/OneFileWF.hpp](https://github.com/pramalhe/OneFile/blob/master/stms/OneFileWF.hpp)      STM with wait-free transactions  
  [ptms/POneFileLF.hpp](https://github.com/pramalhe/OneFile/blob/master/ptms/OneFilePTMLF.hpp)     PTM with lock-free transactions  
  [ptms/POneFileWF.hpp](https://github.com/pramalhe/OneFile/blob/master/ptms/OneFilePTMWF.hpp)     PTM with wait-free transactions  
- Copy the header to your development folder
- Include the header from a single .cpp. If you include from multiple compilation units (.cpp files) then move the last block in the .hpp to one of the .cpp files.
- If you want a data structure that is already made then take a look at what's on these folders:

    datastructures/         Data structures for volatile memory (needs one of the STMs)
    pdatastructures/        Data structures for persistent memory (needs one of the PTMs)


### Design ###

In OneFile STM a transaction goes through three phases. 
The first phase is to convert the operation (lambda) into a store-log (write-set). There is no need to save the loads (read-set) because unlike other approaches, a transaction does not need to re-check for changes at commit time: it does a check in-flight on each load of whether or not the value has changed since the beginning of the transaction, by looking at a sequence number associated with every read value, a technique similar to TL2 or TinySTM but without the need for keeping a read-set because all write transactions are executed one at a time, effectively serialized.
The second phase is to commit the transaction by advancing the current transaction (curTx).
The third phase is to apply the store-log using DCAS.

The first phase is implicitly serializable. Even if each thread publishes its operation, there is no way to parallelize this work among threads. The best that could be done would be that each thread to transform its own operation into its own store-log which it then appends to a global store-log. Unfortunately this is possible only for disjoint-access parallel transactions, and these are not easy to detect, therefore, our implementation of OneFile does not do this.
Instead, we attempt to parallelize the second stage, where the store-log is applied. This task is easier to split among multiple threads, thus parallelizing it.
Adding Flat-Combining or other similar aggregation techniques to the first stage, means that each thread will produce a store-log containing the operations of all other threads. This can be a bottleneck if the operations involves heavy computations and small store-logs. For data structures, this is not the case, and OneFile is designed to implement and work with data structures or other scenarios where the transactions are short in time, therefore, we found it acceptable to go with such an approach.

The parallelization of the third phase can be done with at least two different approaches: blocking and non-blocking.
In the blocking approach, the store-log can be divided into chunks (for example, one chunk per thread), each chunk having a lock, and the thread that takes the lock is responsible for applying that chunk.
In the non-blocking approach (OF-LF and OF-WF), each thread tries to apply an entry of the store-log at a time. To avoid ABA issues, a double-word compare-and-swap (DCAS) must be used.

In summary, OneFile does *not* do disjoint-access parallel transactions. If you absolutely need that functionality, then go and take a look at TinySTM. 



## Requirements ##

- OneFile needs a double-word CAS, which limits it to x86. The algorithm can be modified to use LL/SC or even single-word CAS at the cost of losing its generic capability because bits would have to be stolen from a 64 bit wordl
- The user must "instrument" the code where the atomic updates take place by wrapping the types with *tmtype<T>*. Even then, the operator overloading will not cover all the cases and there will be situations where the user has to annotate the code with .pload() or .pstore() respectively;
- The *T* type must be the size of a word, i.e. 64 bits. Anything bigger and it needs to be splitted into multiple *tmtype<T>* objects;
- If memory reclamation is needed, then the objects need to derive from the *tmbase* base class, need to be allocated with *tmNew()* or *tmMalloc()* and deallocated with *tmDelete()* or *tmFree()*;



## Memory Reclamation ##

We're using a customized implementation of Hazard Eras, lock-free/wait-free memory reclamation:  
[https://github.com/pramalhe/ConcurrencyFreaks/blob/master/papers/hazarderas-2017.pdf](https://github.com/pramalhe/ConcurrencyFreaks/blob/master/papers/hazarderas-2017.pdf)  
[https://dl.acm.org/citation.cfm?id=3087588](https://dl.acm.org/citation.cfm?id=3087588)  
See the HazardErasOF class in each implementation for more details.  

As far as we know, there is only one wait-free data structure that has integrated wait-free memory reclamation:  
[https://github.com/pramalhe/ConcurrencyFreaks/blob/master/papers/crturnqueue-2016.pdf]([https://github.com/pramalhe/ConcurrencyFreaks/blob/master/papers/crturnqueue-2016.pdf)  
OneFile and CX are the first time that someone has made a generic mechanism for wait-free memory reclamation.



## How to use this ##

1. Annotate all the objects that are shared among threads, namely, everything that is *std::atomic<T>* should be changed to *tmtype<T>*;
2. Use only *pstore()* and *pload()* (or just use '='). Do *not* call compare_exchange_strong(), exchange() or fetch_add();
3. Replace calls to "obj = new T(args)" with "obj = tmNew<T>(args)";
4. Replace calls to "delete obj" with "tmDelete<T>(obj)";
5. The T types must derive from the base class *tmbase*;
6. Place your methods in a lambda, capturing whatever you need, and pass the lambda to *updateTx()*;
That's it, you've now got your own lock-free data structure!
For an example of a simple linked-list set, take a look datastructures/linkedlists/OFLFLinkedListSet.hpp


## Disadvantages ##

- All mutative operations are serialized;
- Types must be broken down to 64 bit sizes;
- Requires Double-word-compare-and-swap (DCAS);


## Advantages ##

- Lock-free programming was never so easy, all the user code has to do is loads and stores on *tmtypes<T>* types and those get transformed into a DCAS() based transaction that provides correct linearizable lock-free progress, without ABA issues;
- Memory reclamation is also handled by OF using Hazard Eras, a lock-free/wait-free memory reclamation technique;
- Compared to hand-written lock-free data structures, on the uncontended case, we are replacing each CAS with a DCAS and adding one extra (regular) CAS on the currTrans, which is a small price to pay for the atomicity;
- This technique provides full linearizability for generic code, even mutative iterators, something which is nearly impossible to do with hand-written lock-free data structures;
- Multiple helping threads can help apply starting on different places. A good heuristic is to start from the (tid % numStores);
- OneFile-WF is the first STM with wait-free bounded progress and it's the first to have wait-free bounded progress with wait-free bounded memory reclamation.
- Read-only transactions are lightweight and they can run concurrently with write transactions as long as they're disjoint;

The biggest advantage of all is that it's way easier to use OneFile than it is to implement a hand-made lock-free or wait-free data structure.



## Examples ##

There are some working examples in the "datastructures/" folder:
OFLFBoundedQueue.hpp:     An array based queue (memory-bounded) 
OFLFLinkedListQueue.hpp:  A linked list based queue (memory unbounded) 
OFLFLinkedListSet.hpp:    A linked list based set 
OFLFRedBlackBST.hpp:      A Red-Black (balanced) tree map  



## Benchmarks ##
To build the benchmarks you need to build ESTM and TinySTM, and then you need to pull PMDK (PMEM/NVML) and build it:

    cd ~/onefile/stms/
    cd estm-0.3.0
    make clean ; make
    cd ..
    cd tinystm
    make clean ; make
    cd ..
    cd ~
    git clone https://github.com/pmem/pmdk.git
    cd pmdk
    make -j12
    sudo make install
    export PMEM_IS_PMEM_FORCE=1
    cd ~/onefile/graphs
    make -j12
    

## Tests ##
The four implementations of OneFile were executed during thousands of cpu hours and heavily stress tested with invariant checking and using tools like address sanitizer and valgrind. This is a lot more than what other STMs on github provide, but it doesn't mean there are no bugs in it  ;)
If you see a crash or invariant failure, run the same code under a global rw-lock to make sure the bug is not in your code. If you really believe it's in OneFile, then please open a bug on github and add as much information as you can, namely, stack trace and files needed to reproduce. 
We'll do our best to address it.

 