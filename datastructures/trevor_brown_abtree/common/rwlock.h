/* 
 * File:   rwlock.h
 * Author: trbot
 *
 * Created on June 29, 2017, 8:25 PM
 */

#ifndef RWLOCK_H
#define RWLOCK_H

#ifdef RWLOCK_PTHREADS
#elif defined RWLOCK_FAVOR_WRITERS
#elif defined RWLOCK_FAVOR_READERS
#else
//    #warning "No RWLOCK implementation specified... using default: favour READERS. See rwlock.h for options. Note that this setting only affects algorithms that use the lock-based range query provider in common/rq/rq_rwlock.h."
    #define RWLOCK_FAVOR_READERS
//    #error Must specify RWLOCK implementation; see rwlock.h
#endif

#ifdef RWLOCK_PTHREADS

class RWLock {
private:
    pthread_rwlock_t lock;
    
public:
    RWLock() {
        if (pthread_rwlock_init(&lock, NULL)) error("could not init rwlock");
    }
    ~RWLock() {
        if (pthread_rwlock_destroy(&lock)) error("could not destroy rwlock");
    }
    inline void readLock() {
        if (pthread_rwlock_rdlock(&lock)) error("could not read-lock rwlock");
    }
    inline void readUnlock() {
        if (pthread_rwlock_unlock(&lock)) error("could not read-unlock rwlock");
    }
    inline void writeLock() {
        if (pthread_rwlock_wrlock(&lock)) error("could not write-lock rwlock");
    }
    inline void writeUnlock() {
        if (pthread_rwlock_unlock(&lock)) error("could not write-unlock rwlock");
    }
    inline bool isWriteLocked() {
        cout<<"ERROR: isWriteLocked() is not implemented"<<endl;
        exit(-1);
    }
    inline bool isReadLocked() {
        cout<<"ERROR: isReadLocked() is not implemented"<<endl;
        exit(-1);
    }
    inline bool isLocked() {
        cout<<"ERROR: isReadLocked() is not implemented"<<endl;
        exit(-1);
    }
};

#elif defined RWLOCK_FAVOR_WRITERS

class RWLock {
private:
    volatile long long lock; // two bit fields: [ number of readers ] [ writer bit ]
    
public:
    RWLock() {
        lock = 0;
    }
    inline bool isWriteLocked() {
        return lock & 1;
    }
    inline bool isReadLocked() {
        return lock & ~1;
    }
    inline bool isLocked() {
        return lock;
    }
    inline void readLock() {
        while (1) {
            while (isLocked()) {}
            if ((__sync_add_and_fetch(&lock, 2) & 1) == 0) return; // when we tentatively read-locked, there was no writer
            __sync_add_and_fetch(&lock, -2); // release our tentative read-lock
        }
    }
    inline void readUnlock() {
        __sync_add_and_fetch(&lock, -2);
    }
    inline void writeLock() {
        while (1) {
            long long v = lock;
            if (__sync_bool_compare_and_swap(&lock, v & ~1, v | 1)) {
                while (v & ~1) { // while there are still readers
                    v = lock;
                }
                return;
            }
        }
    }
    inline void writeUnlock() {
        __sync_add_and_fetch(&lock, -1);
    }
};

#elif defined RWLOCK_FAVOR_READERS

class RWLock {
private:
    volatile long long lock; // two bit fields: [ number of readers ] [ writer bit ]
    
public:
    RWLock() {
        lock = 0;
    }
    inline bool isWriteLocked() {
        return lock & 1;
    }
    inline bool isReadLocked() {
        return lock & ~1;
    }
    inline bool isLocked() {
        return lock;
    }
    inline void readLock() {
        while (1) {
            __sync_add_and_fetch(&lock, 2);
            while (isWriteLocked());
            return;
        }
    }
    inline void readUnlock() {
        __sync_add_and_fetch(&lock, -2);
    }
    inline void writeLock() {
        while (1) {
            while (isLocked()) {}
            if (__sync_bool_compare_and_swap(&lock, 0, 1)) {
                return;
            }
        }
    }
    inline void writeUnlock() {
        __sync_add_and_fetch(&lock, -1);
    }
};

#endif

#endif /* RWLOCK_H */

