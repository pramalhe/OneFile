/*
 * File:
 *   alloc_cpp.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Module for C++ dynamic memory management.
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

#include "stm.h"

/* ################################################################### *
 * TYPES
 * ################################################################### */

typedef struct mod_alloc_block {          /* Block of allocated memory */
  void *addr;                             /* Address of memory */
  void (*rev_func)(void*);                /* Undo/Defered function */
  struct mod_alloc_block *next;           /* Next block */
} mod_alloc_block_t;

typedef struct mod_alloc_info {           /* Memory descriptor */
  mod_alloc_block_t *allocated;           /* Memory allocated by this transation (freed upon abort) */
  mod_alloc_block_t *freed;               /* Memory freed by this transation (freed upon commit) */
} mod_alloc_info_t;

static int mod_alloc_key;
static int mod_alloc_initialized = 0;

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

/* TODO this is only true on linux amd64/ia32 */
#ifdef __LP64__
# define CREATENAME(A,B) A##m##B
#else /* ! __LP64__ */
# define CREATENAME(A,B) A##j##B
#endif /* ! __LP64__ */


typedef const struct nothrow_t { } *c_nothrow_p;
extern void *CREATENAME(_Znw,) (size_t) __attribute__((weak));
extern void *CREATENAME(_Zna,) (size_t) __attribute__((weak));
extern void *CREATENAME(_Znw,RKSt9nothrow_t) (size_t, c_nothrow_p) __attribute__((weak));
extern void *CREATENAME(_Zna,RKSt9nothrow_t) (size_t, c_nothrow_p) __attribute__((weak));

extern void _ZdlPv (void *) __attribute__((weak));
extern void _ZdaPv (void *) __attribute__((weak));
extern void _ZdlPvRKSt9nothrow_t (void *, c_nothrow_p) __attribute__((weak));
extern void _ZdaPvRKSt9nothrow_t (void *, c_nothrow_p) __attribute__((weak));

static void mod_alloc_record(void *ptr, void (*rev_func)(void*))
{
  /* Memory will be freed upon abort */
  mod_alloc_info_t *mi;
  mod_alloc_block_t *mb;

  if (!mod_alloc_initialized) {
    fprintf(stderr, "Module mod_alloc not initialized\n");
    exit(1);
  }

  mi = (mod_alloc_info_t *)stm_get_specific(mod_alloc_key);
  assert(mi != NULL);

  if ((mb = (mod_alloc_block_t *)malloc(sizeof(mod_alloc_block_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  mb->addr = ptr;
  mb->rev_func = rev_func;
  mb->next = mi->allocated;
  mi->allocated = mb;
}

static void mod_free_record(void *addr, void (*rev_func)(void*))
{
  /* Memory disposal is delayed until commit */
  mod_alloc_info_t *mi;
  mod_alloc_block_t *mb;

  if (!mod_alloc_initialized) {
    fprintf(stderr, "Module mod_alloc not mod_alloc_initialized\n");
    exit(1);
  }

  mi = (mod_alloc_info_t *)stm_get_specific(mod_alloc_key);
  assert(mi != NULL);

  /* Overwrite to prevent inconsistent reads */
  /* TODO delete operators doesn't give the allocated size */
  /* Acquire lock and update version number */
  stm_store2(addr, 0, 0);
  /* Schedule for removal */
  if ((mb = (mod_alloc_block_t *)malloc(sizeof(mod_alloc_block_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  mb->addr = addr;
  mb->rev_func = rev_func;
  mb->next = mi->freed;
  mi->freed = mb;
}

/*
 * Called upon thread creation.
 */
static void mod_alloc_on_thread_init(void *arg)
{
  mod_alloc_info_t *mi;

  if ((mi = (mod_alloc_info_t *)malloc(sizeof(mod_alloc_info_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  mi->allocated = mi->freed = NULL;

  stm_set_specific(mod_alloc_key, mi);
}

/*
 * Called upon thread deletion.
 */
static void mod_alloc_on_thread_exit(void *arg)
{
  free(stm_get_specific(mod_alloc_key));
}

/*
 * Called upon transaction commit.
 */
static void mod_alloc_on_commit(void *arg)
{
  mod_alloc_info_t *mi;
  mod_alloc_block_t *mb, *next;

  mi = (mod_alloc_info_t *)stm_get_specific(mod_alloc_key);
  assert(mi != NULL);

  /* Keep memory allocated during transaction */
  if (mi->allocated != NULL) {
    mb = mi->allocated;
    while (mb != NULL) {
      next = mb->next;
      free(mb);
      mb = next;
    }
    mi->allocated = NULL;
  }

  /* Dispose of memory freed during transaction */
  if (mi->freed != NULL) {
    mb = mi->freed;
    while (mb != NULL) {
      next = mb->next;
      mb->rev_func(mb->addr);
      free(mb);
      mb = next;
    }
    mi->freed = NULL;
  }
}

/*
 * Called upon transaction abort.
 */
static void mod_alloc_on_abort(void *arg)
{
  mod_alloc_info_t *mi;
  mod_alloc_block_t *mb, *next;

  mi = (mod_alloc_info_t *)stm_get_specific(mod_alloc_key);
  assert (mi != NULL);

  /* Dispose of memory allocated during transaction */
  if (mi->allocated != NULL) {
    mb = mi->allocated;
    while (mb != NULL) {
      next = mb->next;
      mb->rev_func(mb->addr);
      free(mb);
      mb = next;
    }
    mi->allocated = NULL;
  }

  /* Keep memory freed during transaction */
  if (mi->freed != NULL) {
    mb = mi->freed;
    while (mb != NULL) {
      next = mb->next;
      free(mb);
      mb = next;
    }
    mi->freed = NULL;
  }
}

/* New operators */

void *CREATENAME(_ZGTtnw,) (size_t sz)
{
  void *alloc;
  alloc = CREATENAME(_Znw,)(sz);
  mod_alloc_record(alloc, _ZdlPv);
  return alloc;
}

void *CREATENAME(_ZGTtna,) (size_t sz)
{
  void *alloc;
  alloc = CREATENAME(_Zna,)(sz);
  mod_alloc_record(alloc, _ZdaPv);
  return alloc;
}

static void _ZdlPvRKSt9nothrow_t1(void *ptr)
{ 
  _ZdlPvRKSt9nothrow_t (ptr, NULL);
}

void *CREATENAME(_ZGTtnw,RKSt9nothrow_t) (size_t sz, c_nothrow_p nt)
{
  void *alloc;
  alloc = CREATENAME(_Znw,RKSt9nothrow_t)(sz, nt);
  mod_alloc_record(alloc, _ZdlPvRKSt9nothrow_t1);
  return alloc;
}

static void _ZdaPvRKSt9nothrow_t1(void *ptr)
{
  _ZdaPvRKSt9nothrow_t(ptr, NULL);
}

void *CREATENAME(_ZGTtna,RKSt9nothrow_t)(size_t sz, c_nothrow_p nt)
{
  void *alloc;
  alloc = CREATENAME(_Zna,RKSt9nothrow_t)(sz, nt);
  mod_alloc_record(alloc, _ZdaPvRKSt9nothrow_t1);
  return alloc;
}

/* Delete operators */

void
_ZGTtdlPv (void *ptr)
{
  mod_free_record(ptr, _ZdlPv);
}

void
_ZGTtdlPvRKSt9nothrow_t (void *ptr, c_nothrow_p nt)
{ 
  mod_free_record(ptr, _ZdlPvRKSt9nothrow_t1);
}

void _ZGTtdaPv(void *ptr)
{
  mod_free_record(ptr, _ZdaPv);
}

void
_ZGTtdaPvRKSt9nothrow_t (void *ptr, c_nothrow_p nt)
{
  mod_free_record(ptr, _ZdaPvRKSt9nothrow_t1);
}

/*
 * Initialize module.
 */
void mod_alloc_cpp()
{
  if (mod_alloc_initialized)
    return;

  stm_register(mod_alloc_on_thread_init, mod_alloc_on_thread_exit, NULL, NULL, mod_alloc_on_commit, mod_alloc_on_abort, NULL);
  mod_alloc_key = stm_create_specific();
  if (mod_alloc_key < 0) {
    fprintf(stderr, "Cannot create specific key\n");
    exit(1);
  }
  mod_alloc_initialized = 1;
}
