/*
 * File:
 *   mod_cb_mem.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Module for user callback and for dynamic memory management.
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
#include <stdlib.h>

#include "mod_cb.h"
#include "mod_mem.h"

/* TODO use stm_internal.h for faster accesses */
#include "stm.h"
#include "utils.h"
#include "gc.h"


/* ################################################################### *
 * TYPES
 * ################################################################### */
#define DEFAULT_CB_SIZE                 16

typedef struct mod_cb_entry {           /* Callback entry */
  void (*f)(void *);                    /* Function */
  void *arg;                            /* Argument to be passed to function */
} mod_cb_entry_t;

typedef struct mod_cb_info {
  unsigned short commit_size;           /* Array size for commit callbacks */
  unsigned short commit_nb;             /* Number of commit callbacks */
  mod_cb_entry_t *commit;               /* Commit callback entries */
  unsigned short abort_size;            /* Array size for abort callbacks */
  unsigned short abort_nb;              /* Number of abort callbacks */
  mod_cb_entry_t *abort;                /* Abort callback entries */
} mod_cb_info_t;

/* TODO: to avoid false sharing, this should be in a dedicated cacheline.
 * Unfortunately this will cost one cache line for each module. Probably
 * mod_cb_mem could be included always in mainline stm since allocation is
 * common in transaction (?). */
static union {
  struct {
    int key;
    unsigned int use_gc;
  };
  char padding[CACHELINE_SIZE];
} ALIGNED mod_cb = {{.key = -1}};

/* ################################################################### *
 * CALLBACKS FUNCTIONS
 * ################################################################### */

static INLINE void
mod_cb_add_on_abort(mod_cb_info_t *icb, void (*f)(void *arg), void *arg)
{
  if (unlikely(icb->abort_nb >= icb->abort_size)) {
    icb->abort_size *= 2;
    icb->abort = xrealloc(icb->abort, sizeof(mod_cb_entry_t) * icb->abort_size);
  }
  icb->abort[icb->abort_nb].f = f;
  icb->abort[icb->abort_nb].arg = arg;
  icb->abort_nb++;
}

static INLINE void
mod_cb_add_on_commit(mod_cb_info_t *icb, void (*f)(void *arg), void *arg)
{
  if (unlikely(icb->commit_nb >= icb->commit_size)) {
    icb->commit_size *= 2;
    icb->commit = xrealloc(icb->commit, sizeof(mod_cb_entry_t) * icb->commit_size);
  }
  icb->commit[icb->commit_nb].f = f;
  icb->commit[icb->commit_nb].arg = arg;
  icb->commit_nb++;
}

/*
 * Register abort callback for the CURRENT transaction.
 */
int stm_on_abort(void (*on_abort)(void *arg), void *arg)
{
  mod_cb_info_t *icb;

  assert(mod_cb.key >= 0);
  icb = (mod_cb_info_t *)stm_get_specific(mod_cb.key);
  assert(icb != NULL);

  mod_cb_add_on_abort(icb, on_abort, arg);

  return 1;
}

/*
 * Register commit callback for the CURRENT transaction.
 */
int stm_on_commit(void (*on_commit)(void *arg), void *arg)
{
  mod_cb_info_t *icb;

  assert(mod_cb.key >= 0);
  icb = (mod_cb_info_t *)stm_get_specific(mod_cb.key);
  assert(icb != NULL);

  mod_cb_add_on_commit(icb, on_commit, arg);

  return 1;
}

/* ################################################################### *
 * MEMORY ALLOCATION FUNCTIONS
 * ################################################################### */
static INLINE void *
int_stm_malloc(struct stm_tx *tx, size_t size)
{
  /* Memory will be freed upon abort */
  mod_cb_info_t *icb;
  void *addr;

  assert(mod_cb.key >= 0);
  icb = (mod_cb_info_t *)stm_get_specific(mod_cb.key);
  assert(icb != NULL);

  /* Round up size */
  if (sizeof(stm_word_t) == 4) {
    size = (size + 3) & ~(size_t)0x03;
  } else {
    size = (size + 7) & ~(size_t)0x07;
  }

  addr = xmalloc(size);

  mod_cb_add_on_abort(icb, free, addr);

  return addr;
}

/*
 * Called by the CURRENT thread to allocate memory within a transaction.
 */
void *stm_malloc(size_t size)
{
  struct stm_tx *tx = stm_current_tx();
  return int_stm_malloc(tx, size);
}

void *stm_malloc_tx(struct stm_tx *tx, size_t size)
{
  return int_stm_malloc(tx, size);
}

static inline
void *int_stm_calloc(struct stm_tx *tx, size_t nm, size_t size)
{
  /* Memory will be freed upon abort */
  mod_cb_info_t *icb;
  void *addr;

  assert(mod_cb.key >= 0);
  icb = (mod_cb_info_t *)stm_get_specific(mod_cb.key);
  assert(icb != NULL);

  /* Round up size */
  if (sizeof(stm_word_t) == 4) {
    size = (size + 3) & ~(size_t)0x03;
  } else {
    size = (size + 7) & ~(size_t)0x07;
  }

  addr = xcalloc(nm, size);

  mod_cb_add_on_abort(icb, free, addr);

  return addr;
}

/*
 * Called by the CURRENT thread to allocate initialized memory within a transaction.
 */
void *stm_calloc(size_t nm, size_t size)
{
  struct stm_tx *tx = stm_current_tx();
  return int_stm_calloc(tx, nm, size);
}

void *stm_calloc_tx(struct stm_tx *tx, size_t nm, size_t size)
{
  return int_stm_calloc(tx, nm, size);
}

#ifdef EPOCH_GC
static void
epoch_free(void *addr)
{
  if (mod_cb.use_gc) {
    /* TODO use tx->end could be also used */
    stm_word_t t = stm_get_clock();
    gc_free(addr, t);
  } else {
    xfree(addr);
  }
}
#endif /* EPOCH_GC */

static inline
void int_stm_free2(struct stm_tx *tx, void *addr, size_t idx, size_t size)
{
  /* Memory disposal is delayed until commit */
  mod_cb_info_t *icb;

  assert(mod_cb.key >= 0);
  icb = (mod_cb_info_t *)stm_get_specific(mod_cb.key);
  assert(icb != NULL);

  /* TODO: if block allocated in same transaction => no need to overwrite */
  if (size > 0) {
    stm_word_t *a;
    /* Overwrite to prevent inconsistent reads */
    if (sizeof(stm_word_t) == 4) {
      idx = (idx + 3) >> 2;
      size = (size + 3) >> 2;
    } else {
      idx = (idx + 7) >> 3;
      size = (size + 7) >> 3;
    }
    a = (stm_word_t *)addr + idx;
    while (size-- > 0) {
      /* Acquire lock and update version number */
      stm_store2_tx(tx, a++, 0, 0);
    }
  }
  /* Schedule for removal */
#ifdef EPOCH_GC
  mod_cb_add_on_commit(icb, epoch_free, addr);
#else /* ! EPOCH_GC */
  mod_cb_add_on_commit(icb, free, addr);
#endif /* ! EPOCH_GC */
}

/*
 * Called by the CURRENT thread to free memory within a transaction.
 */
void stm_free2(void *addr, size_t idx, size_t size)
{
  struct stm_tx *tx = stm_current_tx();
  int_stm_free2(tx, addr, idx, size);
}

void stm_free2_tx(struct stm_tx *tx, void *addr, size_t idx, size_t size)
{
  int_stm_free2(tx, addr, idx, size);
}

/*
 * Called by the CURRENT thread to free memory within a transaction.
 */
void stm_free(void *addr, size_t size)
{
  struct stm_tx *tx = stm_current_tx();
  int_stm_free2(tx, addr, 0, size);
}

void stm_free_tx(struct stm_tx *tx, void *addr, size_t size)
{
  int_stm_free2(tx, addr, 0, size);
}


/*
 * Called upon transaction commit.
 */
static void mod_cb_on_commit(void *arg)
{
  mod_cb_info_t *icb;

  icb = (mod_cb_info_t *)stm_get_specific(mod_cb.key);
  assert(icb != NULL);

  /* Call commit callback */
  while (icb->commit_nb > 0) {
    icb->commit_nb--;
    icb->commit[icb->commit_nb].f(icb->commit[icb->commit_nb].arg);
  }
  /* Reset abort callback */
  icb->abort_nb = 0;
}

/*
 * Called upon transaction abort.
 */
static void mod_cb_on_abort(void *arg)
{
  mod_cb_info_t *icb;

  icb = (mod_cb_info_t *)stm_get_specific(mod_cb.key);
  assert(icb != NULL);

  /* Call abort callback */
  while (icb->abort_nb > 0) {
    icb->abort_nb--;
    icb->abort[icb->abort_nb].f(icb->abort[icb->abort_nb].arg);
  }
  /* Reset commit callback */
  icb->commit_nb = 0;
}

/*
 * Called upon thread creation.
 */
static void mod_cb_on_thread_init(void *arg)
{
  mod_cb_info_t *icb;

  icb = (mod_cb_info_t *)xmalloc(sizeof(mod_cb_info_t));
  icb->commit_nb = icb->abort_nb = 0;
  icb->commit_size = icb->abort_size = DEFAULT_CB_SIZE;
  icb->commit = xmalloc(sizeof(mod_cb_entry_t) * icb->commit_size);
  icb->abort = xmalloc(sizeof(mod_cb_entry_t) * icb->abort_size);

  stm_set_specific(mod_cb.key, icb);
}

/*
 * Called upon thread deletion.
 */
static void mod_cb_on_thread_exit(void *arg)
{
  mod_cb_info_t *icb;

  icb = (mod_cb_info_t *)stm_get_specific(mod_cb.key);
  assert(icb != NULL);

  xfree(icb->abort);
  xfree(icb->commit);
  xfree(icb);
}

static INLINE void
mod_cb_mem_init(void)
{
  /* Module is already initialized? */
  if (mod_cb.key >= 0)
    return;

  if (!stm_register(mod_cb_on_thread_init, mod_cb_on_thread_exit, NULL, NULL, mod_cb_on_commit, mod_cb_on_abort, NULL)) {
    fprintf(stderr, "Cannot register callbacks\n");
    exit(1);
  }
  mod_cb.key = stm_create_specific();
  if (mod_cb.key < 0) {
    fprintf(stderr, "Cannot create specific key\n");
    exit(1);
  }
}

/*
 * Initialize module.
 */
void mod_cb_init(void)
{
  mod_cb_mem_init();
}

void mod_mem_init(int use_gc)
{
  mod_cb_mem_init();
#ifdef EPOCH_GC
  mod_cb.use_gc = use_gc;
#endif /* EPOCH_GC */
}

