/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef BLOCKLIST_H
#define	BLOCKLIST_H

#include <cassert>
#include <iostream>
#include "blockpool.h"
#include "plaf.h"
using namespace std;

template <typename T>
class blockpool;

template <typename T>
class blockbag;

template <typename T>
class block;

#include "lockfreeblockbag.h"

// BLOCK_SIZE must be a power of two, or else the bitwise math is invalid.
#define BLOCK_SIZE (1<<8)
    
    template <typename T>
    class block { // stack implemented as an array
        private:
            T * data[BLOCK_SIZE];
            int size;
        public:
            block<T> *next;
            
            block(block<T> * const _next) : next(_next) {
                size = 0;
            }
            ~block() {
                assert(size == 0);
            }
            
            bool isFull() {
                return size == BLOCK_SIZE;
            }
            bool isEmpty() {
                return size == 0;
            }
            // precondition: !isFull()
            void push(T * const obj) {
                assert(size < BLOCK_SIZE);
                const int sz = size;
                //assert(interruptible[((long) ((int *) pthread_getspecific(pthreadkey)))*PREFETCH_SIZE_WORDS] == false);
                data[size] = obj;
                SOFTWARE_BARRIER;
                size = sz+1;
            }
            // precondition: !isEmpty()
            T* pop() {
                assert(size > 0);
                const int sz = size-1;
                size = sz;
                return data[sz];
            }
            T* peek(const int ix) {
                assert(ix >= 0);
                //assert(ix < size);
                return data[ix];
            }
            // warning: linear time
            bool contains(T* const obj) {
                for (int i=0;i<size;++i) {
                    if (data[i] == obj) return true;
                }
                return false;
            }
            // warning: linear time
            // however, it is constant time to erase the last thing you pushed.
            void erase(T* const obj) {
                if (size == 0) return; // empty
                assert(size > 0);
                if (data[size-1] == obj) {
                    --size; // erase last pushed item
                    return;
                }
                // the things we want to remove are probably the oldest,
                // so we iterate forward (head of stack = data[size-1])
                for (int i=0;i<size-1;++i) {
                    if (data[i] == obj) {
                        data[i] = data[size-1];
                        SOFTWARE_BARRIER;
                        --size;
                        return;
                    }
                }
            }
            void erase(const int ix) {
                if (size) {
                    assert(size > 0);
                    if (ix != size-1) {
                        data[ix] = data[size-1];
                    }
                    SOFTWARE_BARRIER;
                    --size; // erase last item
                }
            }
            void replace(const int ix, T* const obj) {
                assert(ix >= 0);
                assert(ix < size);
                assert(obj);
                data[ix] = obj;
            }
            int computeSize() {
                return size;
            }
            // this function is occasionally useful if, for instance,
            // you use a bump allocator, which hands out objects from
            // a huge slab of memory.
            // then, in the destructor for a data structure, we can clear
            // a block without worrying about leaking memory,
            // since we will just free the whole slab at once.
            void clearWithoutFreeingElements() {
                SOFTWARE_BARRIER;
                size = 0;
                SOFTWARE_BARRIER;
            }
    };

    template <typename T>
    class blockbag_iterator {
    private:
        blockbag<T> * const bag;
        block<T> * const head;
        block<T> * curr;
        int ix;
//        long long reclaimCountStart;
#ifdef BLOCKBAG_ITERATOR_COUNT_BLOCKS_TRAVERSED
        int sizeInBlocks;
        int blocksTraversed;
#endif
#ifdef BLOCKBAG_ITERATOR_COUNT_STEPS
        int steps;
#endif
    public:
        block<T> *getCurr() const { return curr; }
        int getIndex() const { return ix; }
        
        blockbag_iterator(block<T> * const _head, blockbag<T> * const _bag) 
                : bag(_bag), head(_head) {
#ifdef BLOCKBAG_ITERATOR_COUNT_STEPS
            steps = 0;
#endif
//            reclaimCountStart = bag->getReclaimCount();
//            assert((reclaimCountStart % 1) == 0);
//            if (reclaimCountStart % 1) {
//                // bag is currently being reclaimed. we cannot traverse it.
//                curr = NULL;
//                ix = -1;
//            } else {
                curr = head;
                ix = -1;
                if (curr) {
                    ix = curr->computeSize(); // "linearize" here
                    (*this)++;
                }
//            }
#ifdef BLOCKBAG_ITERATOR_COUNT_BLOCKS_TRAVERSED
                sizeInBlocks = bag->sizeInBlocks;
                blocksTraversed = 0;
#endif
        }
        inline T* operator*() const {
#ifdef BLOCKBAG_ITERATOR_COUNT_STEPS
            if (ix < 0) std::cout<<"bag="<<bag<<" head="<<head<<" curr="<<curr<<" ix="<<ix<<" steps="<<steps<<std::endl;
#endif
//            /****** start consistency check for concurrent iteration ******/
//            assert(reclaimCountStart == bag->getReclaimCount());
//            if (reclaimCountStart != bag->getReclaimCount()) {
//                // bag is being/has been reclaimed, so we cannot iterate
//                return NULL;
//            }
//            /******* end consistency check for concurrent iteration *******/
            return curr->peek(ix);
        }
        inline blockbag_iterator<T>& operator++(int) {
#ifdef BLOCKBAG_ITERATOR_COUNT_STEPS
            ++steps;
#endif
            --ix;
            if (ix < 0) {
#ifdef BLOCKBAG_ITERATOR_COUNT_BLOCKS_TRAVERSED
                ++blocksTraversed;
                if (blocksTraversed > sizeInBlocks + 1) {
                    std::cout<<"ERROR: too many blocks traversed! traversed "<<blocksTraversed<<" when we expected at most 1+"<<sizeInBlocks<<std::endl;
                    exit(-1);
                }
                assert(blocksTraversed <= sizeInBlocks + 1);
#endif
                curr = curr->next;                                              // race condition: if reclamation happens AND curr is freed along with too many other blocks to fit in the blockpool, then this access might fault
//                /****** start consistency check for concurrent iteration ******/
//                assert(reclaimCountStart == bag->getReclaimCount());
//                if (reclaimCountStart != bag->getReclaimCount()) {
//                    // bag is being/has been reclaimed, so we cannot iterate
//                    curr = NULL;
//                    ix = -1;
//                    return *this;
//                }
//                /******* end consistency check for concurrent iteration *******/
                ix = (curr ? curr->computeSize()-1 : -1);                       // race condition: if reclamation happens AND curr is freed along with too many other blocks to fit in the blockpool, then this access might fault
//                /****** start consistency check for concurrent iteration ******/
//                assert(reclaimCountStart == bag->getReclaimCount());
//                if (reclaimCountStart != bag->getReclaimCount()) {
//                    // bag is being/has been reclaimed, so we cannot iterate
//                    curr = NULL;
//                    ix = -1;
//                    return *this;
//                }
//                /******* end consistency check for concurrent iteration *******/
            }
            return *this;
        }
        void swap(block<T> * const otherCurr, const int otherIx) {
            T * const temp = otherCurr->peek(otherIx);
            otherCurr->replace(otherIx, curr->peek(ix));
            curr->replace(ix, temp);
        }
        // erases the current item
        void erase() {
            assert(curr);
            assert(!curr->isEmpty());
            bool result = bag->erase(curr, ix);
            if (ix >= curr->computeSize()) {
                (*this)++;
            }
            if (result) {
                (*this)++;
            }
        }
    };
    template <typename T>
    inline bool operator==(const blockbag_iterator<T>& a, const blockbag_iterator<T>& b) {
        if (a.getCurr() != b.getCurr()) return false;
        if (a.getIndex() != b.getIndex()) return false;
        return true;
    }
    template <typename T>
    inline bool operator!=(const blockbag_iterator<T>& a, const blockbag_iterator<T>& b) {
        return !(a == b);
    }
    
    // bag implemented with linked list whose nodes are blocks.
    // invariant: head and tail are never NULL
    // invariant: head is not full (computeSize() < BLOCK_SIZE)
    // invariant: all blocks except for the head are full
    // invariant: the bag is empty iff head is empty and head->next is null
    template <typename T>
    class blockbag {
    private:
        int owner;
        volatile long long reclaimCount; // number of times this bag has been the oldest epoch bag and had its nodes reclaimed
        long debugFreed;
    public:
        int sizeInBlocks;
    private:
        
        block<T> *head;
        block<T> *tail;
        
        void validate() {
            // invariant: head and tail are never NULL
            assert(head);
            // invariant: head and tail are never NULL
            assert(tail);
            // invariant: head is not full (computeSize() < BLOCK_SIZE)
            assert(!head->isFull());
            // invariant: all blocks except for the head are full
            block<T> *curr = head->next;
            while (curr) {
                assert(curr->isFull());
                curr = curr->next;
            }
            // invariant: sizeInBlocks is correct
            assert(sizeInBlocks == computeSizeInBlocks());
        }
        
        blockpool<T> * const pool;
        
        void debugPrintBag() {
            std::cout<<"("<<computeSize()<<","<<computeSizeInBlocks()<<") =";
            block<T> * curr = head;
            while (curr) {
                std::cout<<" "<<curr->computeSize()<<"["<<((long)curr)<<"]";
                curr = curr->next;
            }
        }
        int computeSizeInBlocks() {
            int result = 0;
            block<T> *curr = head;
            while (curr) {
                ++result;
                curr = curr->next;
            }
            return result;
        }
        
    public:
        blockbag(const int tid, blockpool<T> * const _pool) : pool(_pool) {
//            VERBOSE DEBUG std::cout<<"constructor blockbag"<<std::endl;
            owner = tid;
//            std::cout<<"bag owner="<<owner<<std::endl;
            reclaimCount = 0;
            debugFreed = 0;
            sizeInBlocks = 1;
            head = pool->allocateBlock(NULL);
            tail = head;
            DEBUG2 assert(computeSizeInBlocks() == sizeInBlocks);
            DEBUG2 assert(computeSize() == 0);
            DEBUG2 validate();
        }
        ~blockbag() {
//            VERBOSE DEBUG std::cout<<"destructor blockbag;";
            assert(isEmpty());
            // clear the bag AND FREE EVERY BLOCK IN IT
            while (head) {
                block<T> * const temp = head;
                head = head->next;
                //DEBUG ++debugFreed;
                pool->deallocateBlock(temp);
            }
//            VERBOSE DEBUG std::cout<<" freed "<<debugFreed<<std::endl;
        }
        
        int getOwner() {
            return owner;
        }
        
        inline void incrementReclaimCount() {
            SOFTWARE_BARRIER;
            ++reclaimCount;
            SOFTWARE_BARRIER;
        }
        inline long long getReclaimCount() {
            SOFTWARE_BARRIER;
            return reclaimCount;
        }
        
        blockbag_iterator<T> begin() {
            return blockbag_iterator<T>(head, this);
        }
        blockbag_iterator<T> end() {
            return blockbag_iterator<T>(NULL, this);
        }

        void add(T * const obj) {
            DEBUG2 validate();
            int oldsize; DEBUG2 oldsize = computeSize();
            head->push(obj);
            if (head->isFull()) {
                int oldNumBlocks; DEBUG2 oldNumBlocks = computeSizeInBlocks();
                block<T> *newblock = pool->allocateBlock(head);
                ++sizeInBlocks;
                //DEBUG2 std::cout<<"((("<<((long)head)<<" full. prepending "<<((long)newblock)<<")))";
                SOFTWARE_BARRIER;
                head = newblock;
                DEBUG2 assert(oldNumBlocks + 1 == computeSizeInBlocks());
                DEBUG2 assert(sizeInBlocks == computeSizeInBlocks());
            }
            DEBUG2 assert(oldsize + 1 == computeSize());
            DEBUG2 validate();
        }
        
        template <typename Alloc>
        void add(const int tid, T * const obj, lockfreeblockbag<T> * const sharedBag, const int thresh, Alloc * const alloc) {
            DEBUG2 validate();
            int oldsize; DEBUG2 oldsize = computeSize();
            head->push(obj);
            if (head->isFull()) {
                int oldNumBlocks; DEBUG2 oldNumBlocks = computeSizeInBlocks();
                block<T> *newblock = pool->allocateBlock(head);
                ++sizeInBlocks;
                //DEBUG2 std::cout<<"((("<<((long)head)<<" full. prepending "<<((long)newblock)<<")))";
                head = newblock;
                DEBUG2 assert(oldNumBlocks + 1 == computeSizeInBlocks());
                DEBUG2 assert(sizeInBlocks == computeSizeInBlocks());
                DEBUG2 assert(oldsize + 1 == computeSize());
                if (sizeInBlocks > thresh) {
                    block<T> *b = removeFullBlock(); // returns NULL if freeBag has < 2 full blocks
                    assert(b);
                    sharedBag->addBlock(b);
                    MEMORY_STATS alloc->debug->addGiven(tid, 1);
                    //DEBUG2 COUTATOMIC("  thread "<<this->tid<<" sharedBag("<<(sizeof(T)==sizeof(Node<long,long>)?"Node":"SCXRecord")<<") now contains "<<sharedBag->size()<<" blocks"<<std::endl);
                    DEBUG2 assert(oldsize + 1 - BLOCK_SIZE == computeSize());
                }
            }
            DEBUG2 validate();
        }
        bool isEmpty() {
            return head->next == NULL && head->isEmpty();
        }
        // precondition: !isEmpty, !curr->isEmpty()
        // returns true if a subsequent invocation of curr->peek(ix) will return
        //         an item that was previously EARLIER in iterator order, and false otherwise.
        bool erase(block<T> * const curr, const int ix) {
            assert(!isEmpty());
            assert(!curr->isEmpty());
            DEBUG2 validate();
            if (head->isEmpty()) {
                // current block cannot be head, since head is empty
                assert(curr != head);
                
                // eliminate empty head block, since next block will now be non-full
                block<T> * const temp = head;
                head = head->next;
                pool->deallocateBlock(temp);
                --sizeInBlocks;
            }
            assert(!head->isEmpty());
            
            // case 1: curr is the new head
            if (curr == head) {
                // erase from head block
                head->erase(ix);
                DEBUG2 validate();
                return false;
            
            // case 2: curr is not the head
            } else {
                assert(!head->isEmpty());
                // we use head->pop() to retrieve
                // some object from the head block.
                // then, we replace the object to be erased
                // with the object taken from the head block.
                T* obj = head->pop();
                curr->replace(ix, obj);
                DEBUG2 validate();
                return true;
            }
        }
        // precondition: !isEmpty()
        T* remove() {
            assert(!isEmpty());
            DEBUG2 validate();
            int oldsize; DEBUG2 oldsize = computeSize();
            T *result;
            if (head->isEmpty()) {
                result = head->next->pop();
                int oldNumBlocks; DEBUG2 oldNumBlocks = computeSizeInBlocks();
                block<T> * const temp = head;
                head = head->next;
                pool->deallocateBlock(temp);
                --sizeInBlocks;
                DEBUG2 assert(oldNumBlocks - 1 == computeSizeInBlocks());
                DEBUG2 assert(sizeInBlocks == computeSizeInBlocks());
                DEBUG2 assert(oldsize - 1 == computeSize());
                DEBUG2 validate();
                return result;
            } else {
                result = head->pop();
                DEBUG2 validate();
                return result;
            }
        }
        
        
        ////////// not anymore // precondition: !isEmpty()
        template <typename Alloc>
        T* remove(const int tid, lockfreeblockbag<T> * const sharedBag, Alloc * const alloc) {
            //assert(!isEmpty());
            DEBUG2 validate();
            int oldsize; DEBUG2 oldsize = computeSize();
            T *result;
            if (head->isEmpty()) {
                if (head->next) {
                    result = head->next->pop();
                    int oldNumBlocks; DEBUG2 oldNumBlocks = computeSizeInBlocks();
                    block<T> * const temp = head;
                    head = head->next;
                    pool->deallocateBlock(temp);
                    --sizeInBlocks;
                    DEBUG2 assert(oldNumBlocks - 1 == computeSizeInBlocks());
                    DEBUG2 assert(sizeInBlocks == computeSizeInBlocks());
                    DEBUG2 assert(oldsize - 1 == computeSize());
//                    if (sizeInBlocks == 1) {
//                        block<T> *b = sharedBag->getBlock();
//                        if (b) {
//                            addFullBlock(b);
//                            //DEBUG this->debug->addTaken(tid, 1);
//                            //DEBUG2 COUTATOMIC("  thread "<<this->tid<<" took "<<b->computeSize()<<" objects from sharedBag"<<std::endl);
//                        } else {
//                            /** begin debug **/
//                            for (int i=0;i<BLOCK_SIZE-1;++i) {
//                                add(alloc->allocate(tid));
//                            }
//                            /** end debug **/
//                        }
//                    }
//                    assert(sizeInBlocks > 1);
                    DEBUG2 validate();
//                    MEMORY_STATS2 alloc->debug->addFromPool(tid, 1);
                    return result;
                } else {
                    block<T> *b = sharedBag->getBlock();
                    if (b) {
                        addFullBlock(b);
                        MEMORY_STATS alloc->debug->addTaken(tid, 1);
                        //DEBUG2 COUTATOMIC("  thread "<<this->tid<<" took "<<b->computeSize()<<" objects from sharedBag"<<std::endl);
                        return remove(/*tid, sharedBag, alloc*/);
                    } else {
//                        return alloc->allocate(tid);
                        /** begin debug **/
                        // allocate entire block worth of objects
                        for (int i=0;i<BLOCK_SIZE;++i) {
                            add(alloc->allocate(tid));
                        }
                        /** end debug **/
                        assert(sizeInBlocks > 1);
                        DEBUG2 validate();
                        return remove(/*tid, sharedBag, alloc*/);
                    }
                }
            } else {
//                MEMORY_STATS2 alloc->debug->addFromPool(tid, 1);
                result = head->pop();
                DEBUG2 validate();
                return result;
            }
        }
        
        // removes and returns a full block if the list contains
        // at least two full blocks. otherwise, this returns NULL;
        block<T>* removeFullBlock() {
            DEBUG2 validate();
            int oldsize; DEBUG2 oldsize = computeSize();
            int oldNumBlocks; DEBUG2 oldNumBlocks = computeSizeInBlocks();
            block<T> *second = head->next;
            if (second != NULL) {
                if (second->next != NULL) {
                    assert(second->computeSize() == BLOCK_SIZE);
                    head->next = second->next;
                    second->next = NULL; // not technically necessary, but safer
                    --sizeInBlocks;
                    DEBUG2 assert(oldNumBlocks - 1 == computeSizeInBlocks());
                    DEBUG2 assert(oldsize - BLOCK_SIZE == computeSize());
                    DEBUG2 assert(sizeInBlocks == computeSizeInBlocks());
                    DEBUG2 validate();
                    return second;
                }
            }
            DEBUG2 assert(oldsize == computeSize());
            DEBUG2 if (sizeInBlocks != computeSizeInBlocks()) { std::cout<<"sizeInBlocks="<<sizeInBlocks<<" compute="<<computeSizeInBlocks()<<std::endl; }
            DEBUG2 assert(sizeInBlocks == computeSizeInBlocks());
            DEBUG2 validate();
            return 0;
        }
        void addFullBlock(block<T> *b) {
            DEBUG2 validate();
            assert(b->computeSize() == BLOCK_SIZE);
            assert(b->next == NULL);
            int oldsize; DEBUG2 oldsize = computeSize();
            int oldNumBlocks; DEBUG2 oldNumBlocks = computeSizeInBlocks();
            tail->next = b;
            tail = b;
            ++sizeInBlocks;
            DEBUG2 assert(oldNumBlocks + 1 == computeSizeInBlocks());
            DEBUG2 assert(oldsize + BLOCK_SIZE == computeSize());
            DEBUG2 assert(sizeInBlocks == computeSizeInBlocks());
            DEBUG2 validate();
        }
//        void appendMoveFullBlocks(blockbag<T> * const other) {
//            assert(other);
//            assert(other->head);
//            DEBUG2 validate();
//            
//            // other consists of one non-full block followed by
//            // zero or more full blocks.
//            // we want this list to contain only full blocks,
//            // except for the head block, so we move any full blocks
//            // from other to this list.
//            // if the other blockbag has a non-empty first block, we simply
//            // ignore it.
//            // (it doesn't matter if we leave a small amount of objects in
//            //  the other bag; they'll be appended to another list later,
//            //  when more blocks become full.)
//
//            // if other contains any full blocks, we append them to this list.
//            if (other->head->next != NULL) {
//                DEBUG2 assert(other->head->next->computeSize() == BLOCK_SIZE);
//                assert(other->head->next->isFull());
//                // append all but the head of the other bag to the end of this bag
//                sizeInBlocks += (other->getSizeInBlocks() - 1);
//                tail->next = other->head->next;
//                tail = other->tail;
//                assert(head && tail);
//                // remove all but the head of the other bag
//                other->head->next = NULL;
//                other->tail = other->head;
//                other->sizeInBlocks = 1;
//                assert(other->head && other->tail);
//            }
//            DEBUG2 other->validate();
//            DEBUG2 validate();
//        }
//        block<T> * const getPredecessorBlock(block<T> * const curr) {
//            block<T> * result = head;
//            while (result && result != curr) {
//                result = result->next;
//            }
//            return result;
//        }
        void appendMoveFullBlocks(blockbag<T> * const other, block<T> * predecessor) {
            assert(other);
            assert(other->head);
            assert(predecessor);
            DEBUG2 validate();
            
            // other consists of one maybe-full block followed by
            // zero or more full blocks.
            // our goal is to append all blocks in the other bag
            // starting with predecessor->next to our own bag.
            if (predecessor->next != NULL) {
                DEBUG2 assert(predecessor->next->computeSize() == BLOCK_SIZE);
                assert(predecessor->next->isFull());
                tail->next = predecessor->next;
                tail = other->tail;
                assert(head && tail);
                sizeInBlocks = computeSizeInBlocks();
                // remove all blocks after predecessor in the other bag
                predecessor->next = NULL;
                other->tail = predecessor;
                other->sizeInBlocks = other->computeSizeInBlocks();
                assert(other->head && other->tail);
            }
            DEBUG2 other->validate();
            DEBUG2 validate();
        }
        void appendMoveFullBlocks(blockbag<T> * const other) {
            appendMoveFullBlocks(other, other->head);
        }
        void appendMoveAll(blockbag<T> * const other) {
            assert(other);
            DEBUG2 validate();
            appendMoveFullBlocks(other);
            while (!other->isEmpty()) {
                add(other->remove());
            }
            sizeInBlocks = computeSizeInBlocks();
            DEBUG2 validate();
        }
        int computeSize() {
            int result = 0;
            block<T> *curr = head;
            while (curr) {
                result += curr->computeSize();
                curr = curr->next;
            }
            return result;
        }
        int getSizeInBlocks() {
            return sizeInBlocks;
        }
        // this function is occasionally useful if, for instance,
        // you use a bump allocator, which hands out objects from
        // a huge slab of memory.
        // then, in the destructor for a data structure, we can clear
        // a blockbag without worrying about leaking memory,
        // since we will just free the whole slab at once.
        void clearWithoutFreeingElements() {
            // free all blocks except for head.
            // we still have to do this, even if we don't have to
            // free elements of type T, since blocks are always
            // allocated using a blockpool, and we will leak memory
            // if we don't return blocks to the pool.
            DEBUG2 validate();
            block<T> * curr = head->next;
            while (curr) {
                block<T> * const temp = curr;
                curr = curr->next;
                temp->clearWithoutFreeingElements();
                this->pool->deallocateBlock(temp);
            }
            // fix up the head/tail pointers
            // and clear the remaining head block
            head->next = NULL;
            tail = head;
            head->clearWithoutFreeingElements();
            sizeInBlocks = 1;
            DEBUG2 validate();
        }
    };

#endif	/* BLOCKBAG_H */

