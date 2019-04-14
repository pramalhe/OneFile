/*
 * File:
 *   gc.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Epoch-based garbage collector.
 *
 * Copyright (c) 2007-2014.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program has a dual license and can also be distributed
 * under the terms of the MIT license.
 */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>

#include <pthread.h>

#include "tls.h"
#include "gc.h"

#include "atomic.h"
#include "stm.h"

/* TODO: could be made much more efficient by allocating large chunks. */

/* ################################################################### *
 * DEFINES
 * ################################################################### */

#define MAX_GC_THREADS                  1024
#define EPOCH_MAX                       (~(gc_word_t)0)

#ifndef NO_PERIODIC_CLEANUP
# ifndef CLEANUP_FREQUENCY
#  define CLEANUP_FREQUENCY             1
# endif /* ! CLEANUP_FREQUENCY */
#endif /* ! NO_PERIODIC_CLEANUP */

#ifdef DEBUG
/* Note: stdio is thread-safe */
# define IO_FLUSH                       fflush(NULL)
# define PRINT_DEBUG(...)               printf(__VA_ARGS__); fflush(NULL)
#else /* ! DEBUG */
# define IO_FLUSH
# define PRINT_DEBUG(...)
#endif /* ! DEBUG */

/* ################################################################### *
 * TYPES
 * ################################################################### */

enum {                                  /* Descriptor status */
  GC_NULL = 0,
  GC_BUSY = 1,
  GC_FREE_EMPTY = 2,
  GC_FREE_FULL = 3
};

typedef struct gc_block {               /* Block of allocated memory */
  void *addr;                           /* Address of memory */
  struct gc_block *next;                /* Next block */
} gc_block_t;

typedef struct gc_region {              /* A list of allocated memory blocks */
  struct gc_block *blocks;              /* Memory blocks */
  gc_word_t ts;                         /* Deallocation timestamp */
  struct gc_region *next;               /* Next region */
} gc_region_t;

typedef struct gc_thread {              /* Descriptor of an active thread */
  union {                               /* For padding... */
    struct {
      gc_word_t used;                   /* Is this entry used? */
      gc_word_t ts;                     /* Start timestamp */
      gc_region_t *head;                /* First memory region(s) assigned to thread */
      gc_region_t *tail;                /* Last memory region(s) assigned to thread */
#ifndef NO_PERIODIC_CLEANUP
      unsigned int frees;               /* How many blocks have been freed? */
#endif /* ! NO_PERIODIC_CLEANUP */
    };
    char padding[CACHELINE_SIZE];       /* Padding (should be at least a cache line) */
  };
} gc_thread_t;

static struct {                         /* Descriptors of active threads */
  volatile gc_thread_t *slots;          /* Array of thread slots */
  volatile gc_word_t nb_active;         /* Number of used thread slots */
} gc_threads;

static gc_word_t (*gc_current_epoch)(void); /* Read the value of the current epoch */

/* ################################################################### *
 * STATIC
 * ################################################################### */

/*
 * Returns the index of the CURRENT thread.
 */
static inline int gc_get_idx(void)
{
  return tls_get_gc();
}

/*
 * Compute a lower bound on the minimum start time of all active transactions.
 */
static inline gc_word_t gc_compute_min(gc_word_t now)
{
  int i;
  gc_word_t min, ts;
  stm_word_t used;

  PRINT_DEBUG("==> gc_compute_min(%d)\n", gc_get_idx());

  min = now;
  for (i = 0; i < MAX_GC_THREADS; ) {
    used = (gc_word_t)ATOMIC_LOAD(&gc_threads.slots[i].used);
    if (used == GC_BUSY) {
      /* Used entry */
      ts = (gc_word_t)ATOMIC_LOAD(&gc_threads.slots[i].ts);
      if (ts == EPOCH_MAX) {
        /* Wait until thread has set a safe lower bound */
        continue;
      }
      if (ts < min)
        min = ts;
    } else if (used == GC_NULL) {
      /* No more threads */
      break;
    }
    /* Move to next entry only if entry is not used or it has a safe lower bound */
    i++;
  }

  PRINT_DEBUG("==> gc_compute_min(%d,m=%lu)\n", gc_get_idx(), (unsigned long)min);

  return min;
}

/*
 * Free block list.
 */
static inline void gc_clean_blocks(gc_block_t *mb)
{
  gc_block_t *next_mb;

  while (mb != NULL) {
    PRINT_DEBUG("==> free(%d,a=%p)\n", gc_get_idx(), mb->addr);
    xfree(mb->addr);
    next_mb = mb->next;
    xfree(mb);
    mb = next_mb;
  }
}

/*
 * Free region list.
 */
static inline void gc_clean_regions(gc_region_t *mr)
{
  gc_region_t *next_mr;

  while (mr != NULL) {
    gc_clean_blocks(mr->blocks);
    next_mr = mr->next;
    xfree(mr);
    mr = next_mr;
  }
}

/*
 * Garbage-collect old data associated with a thread.
 */
void gc_cleanup_thread(int idx, gc_word_t min)
{
  gc_region_t *mr;

  PRINT_DEBUG("==> gc_cleanup_thread(%d,m=%lu)\n", idx, (unsigned long)min);

  if (gc_threads.slots[idx].head == NULL) {
    /* Nothing to clean up */
    return;
  }

  while (min > gc_threads.slots[idx].head->ts) {
    gc_clean_blocks(gc_threads.slots[idx].head->blocks);
    mr = gc_threads.slots[idx].head->next;
    xfree(gc_threads.slots[idx].head);
    gc_threads.slots[idx].head = mr;
    if(mr == NULL) {
      /* All memory regions deleted */
      gc_threads.slots[idx].tail = NULL;
      break;
    }
  }
}

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

/*
 * Initialize GC library (to be called from main thread).
 */
void gc_init(gc_word_t (*epoch)(void))
{
  int i;

  PRINT_DEBUG("==> gc_init()\n");

  gc_current_epoch = epoch;
  gc_threads.slots = (gc_thread_t *)xmalloc(MAX_GC_THREADS * sizeof(gc_thread_t));
  for (i = 0; i < MAX_GC_THREADS; i++) {
    gc_threads.slots[i].used = GC_NULL;
    gc_threads.slots[i].ts = EPOCH_MAX;
    gc_threads.slots[i].head = gc_threads.slots[i].tail = NULL;
#ifndef NO_PERIODIC_CLEANUP
    gc_threads.slots[i].frees = 0;
#endif /* ! NO_PERIODIC_CLEANUP */
  }
  gc_threads.nb_active = 0;
}

/*
 * Clean up GC library (to be called from main thread).
 */
void gc_exit(void)
{
  int i;

  PRINT_DEBUG("==> gc_exit()\n");

  /* Make sure that all threads have been stopped */
  if (ATOMIC_LOAD(&gc_threads.nb_active) != 0) {
    fprintf(stderr, "Error: some threads have not been cleaned up\n");
    exit(1);
  }
  /* Clean up memory */
  for (i = 0; i < MAX_GC_THREADS; i++)
    gc_clean_regions(gc_threads.slots[i].head);

  xfree((void *)gc_threads.slots);
}

/*
 * Initialize thread-specific GC resources (to be called once by each thread).
 */
void gc_init_thread(void)
{
  int i, idx;
  gc_word_t used;

  PRINT_DEBUG("==> gc_init_thread()\n");

  if (ATOMIC_FETCH_INC_FULL(&gc_threads.nb_active) >= MAX_GC_THREADS) {
    fprintf(stderr, "Error: too many concurrent threads created\n");
    exit(1);
  }
  /* Find entry in threads array */
  i = 0;
  /* TODO: not wait-free */
  while (1) {
    used = (gc_word_t)ATOMIC_LOAD(&gc_threads.slots[i].used);
    if (used != GC_BUSY) {
      if (ATOMIC_CAS_FULL(&gc_threads.slots[i].used, used, GC_BUSY) != 0) {
        idx = i;
        /* Set safe lower bound */
        ATOMIC_STORE(&gc_threads.slots[idx].ts, gc_current_epoch());
        break;
      }
      /* CAS failed: another thread must have acquired slot */
      assert (gc_threads.slots[i].used != GC_NULL);
    }
    if (++i >= MAX_GC_THREADS)
      i = 0;
  }
  tls_set_gc(idx);

  PRINT_DEBUG("==> gc_init_thread(i=%d)\n", idx);
}

/*
 * Clean up thread-specific GC resources (to be called once by each thread).
 */
void gc_exit_thread(void)
{
  int idx = gc_get_idx();
  /* NOTA: if gc_exit_thread is not called when it finishes, others threads will not free chunks. */

  PRINT_DEBUG("==> gc_exit_thread(%d)\n", idx);

  /* No more lower bound for this thread */
  ATOMIC_STORE(&gc_threads.slots[idx].ts, EPOCH_MAX);
  /* Release slot */
  ATOMIC_STORE(&gc_threads.slots[idx].used, gc_threads.slots[idx].head == NULL ? GC_FREE_EMPTY : GC_FREE_FULL);
  ATOMIC_FETCH_DEC_FULL(&gc_threads.nb_active);
  /* Leave memory for next thread to cleanup */
}

/*
 * Set new epoch (to be called by each thread, typically when starting
 * new transactions to indicate their start timestamp).
 */
void gc_set_epoch(gc_word_t epoch)
{
  int idx = gc_get_idx();

  PRINT_DEBUG("==> gc_set_epoch(%d,%lu)\n", idx, (unsigned long)epoch);

  if (epoch >= EPOCH_MAX) {
    fprintf(stderr, "Exceeded maximum epoch number: 0x%lx\n", (unsigned long)epoch);
    /* Do nothing (will prevent data from being garbage collected) */
    return;
  }

  /* Do not need a barrier as we only compute lower bounds */
  ATOMIC_STORE(&gc_threads.slots[idx].ts, epoch);
}

/*
 * Free memory (the thread must indicate the current timestamp).
 */
void gc_free(void *addr, gc_word_t epoch)
{
  gc_region_t *mr;
  gc_block_t *mb;
  int idx = gc_get_idx();

  PRINT_DEBUG("==> gc_free(%d,%lu)\n", idx, (unsigned long)epoch);

  /* Function must be called with non-decreasing epoch numbers for any given thread! */
  if (gc_threads.slots[idx].head == NULL || gc_threads.slots[idx].tail->ts < epoch) {
    /* Allocate a new region */
    mr = (gc_region_t *)xmalloc(sizeof(gc_region_t));
    mr->ts = epoch;
    mr->blocks = NULL;
    mr->next = NULL;
    if (gc_threads.slots[idx].head == NULL) {
      gc_threads.slots[idx].head = gc_threads.slots[idx].tail = mr;
    } else {
      gc_threads.slots[idx].tail->next = mr;
      gc_threads.slots[idx].tail = mr;
    }
  } else {
    /* Add to current region */
    assert(gc_threads.slots[idx].tail->ts == epoch);
    mr = gc_threads.slots[idx].tail;
  }

  /* Allocate block */
  mb = (gc_block_t *)xmalloc(sizeof(gc_block_t));
  mb->addr = addr;
  mb->next = mr->blocks;
  mr->blocks = mb;

#ifndef NO_PERIODIC_CLEANUP
  gc_threads.slots[idx].frees++;
  if (gc_threads.slots[idx].frees % CLEANUP_FREQUENCY == 0)
    gc_cleanup();
#endif /* ! NO_PERIODIC_CLEANUP */
}

/*
 * Garbage-collect old data associated with the current thread (should
 * be called periodically).
 */
void gc_cleanup(void)
{
  gc_word_t min;
  int idx = gc_get_idx();

  PRINT_DEBUG("==> gc_cleanup(%d)\n", idx);

  if (gc_threads.slots[idx].head == NULL) {
    /* Nothing to clean up */
    return;
  }

  min = gc_compute_min(gc_current_epoch());

  gc_cleanup_thread(idx, min);
}

/*
 * Garbage-collect old data associated with all threads (should be
 * called periodically).
 */
void gc_cleanup_all(void)
{
  int i;
  gc_word_t min = EPOCH_MAX;

  PRINT_DEBUG("==> gc_cleanup_all()\n");

  for (i = 0; i < MAX_GC_THREADS; i++) {
    if ((gc_word_t)ATOMIC_LOAD(&gc_threads.slots[i].used) == GC_NULL)
      break;
    if ((gc_word_t)ATOMIC_LOAD(&gc_threads.slots[i].used) == GC_FREE_FULL) {
      if (ATOMIC_CAS_FULL(&gc_threads.slots[i].used, GC_FREE_FULL, GC_BUSY) != 0) {
        if (min == EPOCH_MAX)
          min = gc_compute_min(gc_current_epoch());
        gc_cleanup_thread(i, min);
        ATOMIC_STORE(&gc_threads.slots[i].used, gc_threads.slots[i].head == NULL ? GC_FREE_EMPTY : GC_FREE_FULL);
      }
    }
  }
}

/*
 * Reset all epochs for all threads (must be called with all threads
 * stopped and out of transactions, e.g., upon roll-over).
 */
void gc_reset(void)
{
  int i;

  PRINT_DEBUG("==> gc_reset()\n");

  for (i = 0; i < MAX_GC_THREADS; i++) {
    if (gc_threads.slots[i].used == GC_NULL)
      break;
    gc_clean_regions(gc_threads.slots[i].head);
    gc_threads.slots[i].ts = EPOCH_MAX;
    gc_threads.slots[i].head = gc_threads.slots[i].tail = NULL;
#ifndef NO_PERIODIC_CLEANUP
    gc_threads.slots[i].frees = 0;
#endif /* ! NO_PERIODIC_CLEANUP */
  }
}
