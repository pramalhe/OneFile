/*
 * File:
 *   intset.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Integer set stress test.
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
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#define RO                              1
#define RW                              0

#if defined(TM_GCC) 
# include "../../abi/gcc/tm_macros.h"
#elif defined(TM_DTMC) 
# include "../../abi/dtmc/tm_macros.h"
/* Make erand48 pure for DTMC (transaction_pure should work too). */
static double tanger_wrapperpure_erand48(unsigned short int __xsubi[3]) __attribute__ ((weakref("erand48")));
#elif defined(TM_INTEL)
# include "../../abi/intel/tm_macros.h"
#elif defined(TM_ABI)
# include "../../abi/tm_macros.h"
#endif /* defined(TM_ABI) */

#if defined(TM_GCC) || defined(TM_DTMC) || defined(TM_INTEL) || defined(TM_ABI)
# define TM_COMPILER
/* Add some attributes to library function */
TM_PURE 
void exit(int status);
TM_PURE 
void perror(const char *s);
#else /* Compile with explicit calls to tinySTM */

# include "stm.h"
# include "mod_mem.h"
# include "mod_ab.h"

/*
 * Useful macros to work with transactions. Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */
# define TM_START(tid, ro)                  { stm_tx_attr_t _a = {{.id = tid, .read_only = ro}}; \
                                              sigjmp_buf *_e = stm_start(_a); \
                                              if (_e != NULL) sigsetjmp(*_e, 0); 
# define TM_START_TS(ts, label)             { sigjmp_buf *_e = stm_start((stm_tx_attr_t)0); \
                                              if (_e != NULL && sigsetjmp(*_e, 0)) goto label; \
	                                      stm_set_extension(0, &ts)
# define TM_LOAD(addr)                      stm_load((stm_word_t *)addr)
# define TM_UNIT_LOAD(addr, ts)             stm_unit_load((stm_word_t *)addr, ts)
# define TM_STORE(addr, value)              stm_store((stm_word_t *)addr, (stm_word_t)value)
# define TM_UNIT_STORE(addr, value, ts)     stm_unit_store((stm_word_t *)addr, (stm_word_t)value, ts)
# define TM_COMMIT                          stm_commit(); }
# define TM_MALLOC(size)                    stm_malloc(size)
# define TM_FREE(addr)                      stm_free(addr, sizeof(*addr))
# define TM_FREE2(addr, size)               stm_free(addr, size)

# define TM_INIT                            stm_init(); mod_mem_init(0); mod_ab_init(0, NULL)
# define TM_EXIT                            stm_exit()
# define TM_INIT_THREAD                     stm_init_thread()
# define TM_EXIT_THREAD                     stm_exit_thread()

/* Annotations used in this benchmark */
# define TM_SAFE
# define TM_PURE

#endif /* Compile with explicit calls to tinySTM */

#ifdef DEBUG
# define IO_FLUSH                       fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#if !(defined(USE_LINKEDLIST) || defined(USE_RBTREE) || defined(USE_SKIPLIST) || defined(USE_HASHSET))
# error "Must define USE_LINKEDLIST or USE_RBTREE or USE_SKIPLIST or USE_HASHSET"
#endif /* !(defined(USE_LINKEDLIST) || defined(USE_RBTREE) || defined(USE_SKIPLIST) || defined(USE_HASHSET)) */


#define DEFAULT_DURATION                10000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   (DEFAULT_INITIAL * 2)
#define DEFAULT_SEED                    0
#define DEFAULT_UPDATE                  20

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

/* ################################################################### *
 * GLOBALS
 * ################################################################### */
static volatile int stop;
static unsigned short main_seed[3];

static inline void rand_init(unsigned short *seed)
{
  seed[0] = (unsigned short)rand();
  seed[1] = (unsigned short)rand();
  seed[2] = (unsigned short)rand();
}

static inline int rand_range(int n, unsigned short *seed)
{
  /* Return a random number in range [0;n) */
  int v = (int)(erand48(seed) * n);
  assert (v >= 0 && v < n);
  return v;
}

typedef struct thread_data {
  struct intset *set;
  struct barrier *barrier;
  unsigned long nb_add;
  unsigned long nb_remove;
  unsigned long nb_contains;
  unsigned long nb_found;
#ifndef TM_COMPILER
  unsigned long nb_aborts;
  unsigned long nb_aborts_1;
  unsigned long nb_aborts_2;
  unsigned long nb_aborts_locked_read;
  unsigned long nb_aborts_locked_write;
  unsigned long nb_aborts_validate_read;
  unsigned long nb_aborts_validate_write;
  unsigned long nb_aborts_validate_commit;
  unsigned long nb_aborts_invalid_memory;
  unsigned long nb_aborts_killed;
  unsigned long locked_reads_ok;
  unsigned long locked_reads_failed;
  unsigned long max_retries;
#endif /* ! TM_COMPILER */
  unsigned short seed[3];
  int diff;
  int range;
  int update;
  int alternate;
#ifdef USE_LINKEDLIST
  int unit_tx;
#endif /* LINKEDLIST */
  char padding[64];
} thread_data_t;

#if defined(USE_LINKEDLIST)

/* ################################################################### *
 * LINKEDLIST
 * ################################################################### */

# define INIT_SET_PARAMETERS            /* Nothing */

typedef intptr_t val_t;
# define VAL_MIN                        INT_MIN
# define VAL_MAX                        INT_MAX

typedef struct node {
  val_t val;
  struct node *next;
} node_t;

typedef struct intset {
  node_t *head;
} intset_t;

TM_SAFE
static node_t *new_node(val_t val, node_t *next, int transactional)
{
  node_t *node;

  if (!transactional) {
    node = (node_t *)malloc(sizeof(node_t));
  } else {
    node = (node_t *)TM_MALLOC(sizeof(node_t));
  }
  if (node == NULL) {
    perror("malloc");
    exit(1);
  }

  node->val = val;
  node->next = next;

  return node;
}

static intset_t *set_new()
{
  intset_t *set;
  node_t *min, *max;

  if ((set = (intset_t *)malloc(sizeof(intset_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  max = new_node(VAL_MAX, NULL, 0);
  min = new_node(VAL_MIN, max, 0);
  set->head = min;

  return set;
}

static void set_delete(intset_t *set)
{
  node_t *node, *next;

  node = set->head;
  while (node != NULL) {
    next = node->next;
    free(node);
    node = next;
  }
  free(set);
}

static int set_size(intset_t *set)
{
  int size = 0;
  node_t *node;

  /* We have at least 2 elements */
  node = set->head->next;
  while (node->next != NULL) {
    size++;
    node = node->next;
  }

  return size;
}

static int set_contains(intset_t *set, val_t val, thread_data_t *td)
{
  int result;
  node_t *prev, *next;
  val_t v;

# ifdef DEBUG
  printf("++> set_contains(%d)\n", val);
  IO_FLUSH;
# endif

  if (td == NULL) {
    prev = set->head;
    next = prev->next;
    while (next->val < val) {
      prev = next;
      next = prev->next;
    }
    result = (next->val == val);
  } else if (td->unit_tx == 0) {
    TM_START(0, RO);
    prev = (node_t *)TM_LOAD(&set->head);
    next = (node_t *)TM_LOAD(&prev->next);
    while (1) {
      v = TM_LOAD(&next->val);
      if (v >= val)
        break;
      prev = next;
      next = (node_t *)TM_LOAD(&prev->next);
    }
    result = (v == val);
    TM_COMMIT;
  } 
#ifndef TM_COMPILER
  else {
    /* Unit transactions */
    stm_word_t ts, start_ts, val_ts;
  restart:
    start_ts = stm_get_clock();
    /* Head node is never removed */
    prev = (node_t *)TM_UNIT_LOAD(&set->head, &ts);
    next = (node_t *)TM_UNIT_LOAD(&prev->next, &ts);
    if (ts > start_ts)
      start_ts = ts;
    while (1) {
      v = TM_UNIT_LOAD(&next->val, &val_ts);
      if (val_ts > start_ts) {
        /* Restart traversal (could also backtrack) */
        goto restart;
      }
      if (v >= val)
        break;
      prev = next;
      next = (node_t *)TM_UNIT_LOAD(&prev->next, &ts);
      if (ts > start_ts) {
        /* Verify that node has not been modified (value and pointer are updated together) */
        TM_UNIT_LOAD(&prev->val, &val_ts);
        if (val_ts > start_ts) {
          /* Restart traversal (could also backtrack) */
          goto restart;
        }
        start_ts = ts;
      }
    }
    result = (v == val);
  }
#endif /* TM_COMPILER */

  return result;
}

static int set_add(intset_t *set, val_t val, thread_data_t *td)
{
  int result;
  node_t *prev, *next;
  val_t v;

# ifdef DEBUG
  printf("++> set_add(%d)\n", val);
  IO_FLUSH;
# endif

  if (td == NULL) {
    prev = set->head;
    next = prev->next;
    while (next->val < val) {
      prev = next;
      next = prev->next;
    }
    result = (next->val != val);
    if (result) {
      prev->next = new_node(val, next, 0);
    }
  } else if (td->unit_tx == 0) {
    TM_START(1, RW);
    prev = (node_t *)TM_LOAD(&set->head);
    next = (node_t *)TM_LOAD(&prev->next);
    while (1) {
      v = TM_LOAD(&next->val);
      if (v >= val)
        break;
      prev = next;
      next = (node_t *)TM_LOAD(&prev->next);
    }
    result = (v != val);
    if (result) {
      TM_STORE(&prev->next, new_node(val, next, 1));
    }
    TM_COMMIT;
  } 
#ifndef TM_COMPILER
  else {
    /* Unit transactions */
    stm_word_t ts, start_ts, val_ts;
  restart:
    start_ts = stm_get_clock();
    /* Head node is never removed */
    prev = (node_t *)TM_UNIT_LOAD(&set->head, &ts);
    next = (node_t *)TM_UNIT_LOAD(&prev->next, &ts);
    if (ts > start_ts)
      start_ts = ts;
    while (1) {
      v = TM_UNIT_LOAD(&next->val, &val_ts);
      if (val_ts > start_ts) {
        /* Restart traversal (could also backtrack) */
        goto restart;
      }
      if (v >= val)
        break;
      prev = next;
      next = (node_t *)TM_UNIT_LOAD(&prev->next, &ts);
      if (ts > start_ts) {
        /* Verify that node has not been modified (value and pointer are updated together) */
        TM_UNIT_LOAD(&prev->val, &val_ts);
        if (val_ts > start_ts) {
          /* Restart traversal (could also backtrack) */
          goto restart;
        }
        start_ts = ts;
      }
    }
    result = (v != val);
    if (result) {
      node_t *n = new_node(val, next, 0);
      /* Make sure that there are no concurrent updates to that memory location */
      if (!TM_UNIT_STORE(&prev->next, n, &ts)) {
        free(n);
        goto restart;
      }
    }
  }
#endif /* ! TM_COMPILER */

  return result;
}

static int set_remove(intset_t *set, val_t val, thread_data_t *td)
{
  int result;
  node_t *prev, *next;
  val_t v;
  node_t *n;

# ifdef DEBUG
  printf("++> set_remove(%d)\n", val);
  IO_FLUSH;
# endif

  if (td == NULL) {
    prev = set->head;
    next = prev->next;
    while (next->val < val) {
      prev = next;
      next = prev->next;
    }
    result = (next->val == val);
    if (result) {
      prev->next = next->next;
      free(next);
    }
  } else if (td->unit_tx == 0) {
    TM_START(2, RW);
    prev = (node_t *)TM_LOAD(&set->head);
    next = (node_t *)TM_LOAD(&prev->next);
    while (1) {
      v = TM_LOAD(&next->val);
      if (v >= val)
        break;
      prev = next;
      next = (node_t *)TM_LOAD(&prev->next);
    }
    result = (v == val);
    if (result) {
      n = (node_t *)TM_LOAD(&next->next);
      TM_STORE(&prev->next, n);
      /* Free memory (delayed until commit) */
      TM_FREE2(next, sizeof(node_t));
    }
    TM_COMMIT;
  } 
#ifndef TM_COMPILER
  else {
    /* Unit transactions */
    stm_word_t ts, start_ts, val_ts;
  restart:
    start_ts = stm_get_clock();
    /* Head node is never removed */
    prev = (node_t *)TM_UNIT_LOAD(&set->head, &ts);
    next = (node_t *)TM_UNIT_LOAD(&prev->next, &ts);
    if (ts > start_ts)
      start_ts = ts;
    while (1) {
      v = TM_UNIT_LOAD(&next->val, &val_ts);
      if (val_ts > start_ts) {
        /* Restart traversal (could also backtrack) */
        goto restart;
      }
      if (v >= val)
        break;
      prev = next;
      next = (node_t *)TM_UNIT_LOAD(&prev->next, &ts);
      if (ts > start_ts) {
        /* Verify that node has not been modified (value and pointer are updated together) */
        TM_UNIT_LOAD(&prev->val, &val_ts);
        if (val_ts > start_ts) {
          /* Restart traversal (could also backtrack) */
          goto restart;
        }
        start_ts = ts;
      }
    }
    result = (v == val);
    if (result) {
      /* Make sure that the transaction does not access versions more recent than start_ts */
      TM_START_TS(start_ts, restart);
      n = (node_t *)TM_LOAD(&next->next);
      TM_STORE(&prev->next, n);
      /* Free memory (delayed until commit) */
      TM_FREE2(next, sizeof(node_t));
      TM_COMMIT;
    }
  }
#endif /* ! TM_COMPILER */
  return result;
}

#elif defined(USE_RBTREE)

/* ################################################################### *
 * RBTREE
 * ################################################################### */
/* TODO: comparison function as a pointer should be changed for TM compiler
 * (not supported or introduce a lot of overhead). */
# define INIT_SET_PARAMETERS            /* Nothing */

# define TM_ARGDECL_ALONE               /* Nothing */
# define TM_ARGDECL                     /* Nothing */
# define TM_ARG                         /* Nothing */
# define TM_ARG_ALONE                   /* Nothing */
# define TM_CALLABLE                    TM_SAFE

# define TM_SHARED_READ(var)            TM_LOAD(&(var))
# define TM_SHARED_READ_P(var)          TM_LOAD(&(var))

# define TM_SHARED_WRITE(var, val)      TM_STORE(&(var), val)
# define TM_SHARED_WRITE_P(var, val)    TM_STORE(&(var), val)

# include "rbtree.h"

# include "rbtree.c"

typedef struct intset intset_t;
typedef intptr_t val_t;

static long compare(const void *a, const void *b)
{
  return ((val_t)a - (val_t)b);
}

static intset_t *set_new()
{
  return (intset_t *)rbtree_alloc(&compare);
}

static void set_delete(intset_t *set)
{
  rbtree_free((rbtree_t *)set);
}

static int set_size(intset_t *set)
{
  int size;
  node_t *n;

  if (!rbtree_verify((rbtree_t *)set, 0)) {
    printf("Validation failed!\n");
    exit(1);
  }

  size = 0;
  for (n = firstEntry((rbtree_t *)set); n != NULL; n = successor(n))
    size++;

  return size;
}

static int set_contains(intset_t *set, val_t val, thread_data_t *td)
{
  int result;

# ifdef DEBUG
  printf("++> set_contains(%d)\n", val);
  IO_FLUSH;
# endif

  if (!td) {
    result = rbtree_contains((rbtree_t *)set, (void *)val);
  } else {
    TM_START(0, RO);
    result = TMrbtree_contains((rbtree_t *)set, (void *)val);
    TM_COMMIT;
  }

  return result;
}

static int set_add(intset_t *set, val_t val, thread_data_t *td)
{
  int result;

# ifdef DEBUG
  printf("++> set_add(%d)\n", val);
  IO_FLUSH;
# endif

  if (!td) {
    result = rbtree_insert((rbtree_t *)set, (void *)val, (void *)val);
  } else {
    TM_START(1, RW);
    result = TMrbtree_insert((rbtree_t *)set, (void *)val, (void *)val);
    TM_COMMIT;
  }

  return result;
}

static int set_remove(intset_t *set, val_t val, thread_data_t *td)
{
  int result;

# ifdef DEBUG
  printf("++> set_remove(%d)\n", val);
  IO_FLUSH;
# endif

  if (!td) {
    result = rbtree_delete((rbtree_t *)set, (void *)val);
  } else {
    TM_START(2, RW);
    result = TMrbtree_delete((rbtree_t *)set, (void *)val);
    TM_COMMIT;
  }

  return result;
}

#elif defined(USE_SKIPLIST)

/* ################################################################### *
 * SKIPLIST
 * ################################################################### */

# define MAX_LEVEL                      64

# define INIT_SET_PARAMETERS            32, 50

typedef intptr_t val_t;
typedef intptr_t level_t;
# define VAL_MIN                        INT_MIN
# define VAL_MAX                        INT_MAX

typedef struct node {
  val_t val;
  level_t level;
  struct node *forward[1];
} node_t;

typedef struct intset {
  node_t *head;
  node_t *tail;
  level_t level;
  int prob;
  int max_level;
} intset_t;

TM_PURE
static int random_level(intset_t *set, unsigned short *seed)
{
  int l = 0;
  while (l < set->max_level && rand_range(100, seed) < set->prob)
    l++;
  return l;
}

TM_SAFE
static node_t *new_node(val_t val, level_t level, int transactional)
{
  node_t *node;

  if (!transactional) {
    node = (node_t *)malloc(sizeof(node_t) + level * sizeof(node_t *));
  } else {
    node = (node_t *)TM_MALLOC(sizeof(node_t) + level * sizeof(node_t *));
  }
  if (node == NULL) {
    perror("malloc");
    exit(1);
  }

  node->val = val;
  node->level = level;

  return node;
}

static intset_t *set_new(level_t max_level, int prob)
{
  intset_t *set;
  int i;

  assert(max_level <= MAX_LEVEL);
  assert(prob >= 0 && prob <= 100);

  if ((set = (intset_t *)malloc(sizeof(intset_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  set->max_level = max_level;
  set->prob = prob;
  set->level = 0;
  /* Set head and tail are immutable */
  set->tail = new_node(VAL_MAX, max_level, 0);
  set->head = new_node(VAL_MIN, max_level, 0);
  for (i = 0; i <= max_level; i++) {
    set->head->forward[i] = set->tail;
    set->tail->forward[i] = NULL;
  }

  return set;
}

static void set_delete(intset_t *set)
{
  node_t *node, *next;

  node = set->head;
  while (node != NULL) {
    next = node->forward[0];
    free(node);
    node = next;
  }
  free(set);
}

static int set_size(intset_t *set)
{
  int size = 0;
  node_t *node;

  /* We have at least 2 elements */
  node = set->head->forward[0];
  while (node->forward[0] != NULL) {
    size++;
    node = node->forward[0];
  }

  return size;
}

static int set_contains(intset_t *set, val_t val, thread_data_t *td)
{
  int result, i;
  node_t *node, *next;
  val_t v;

# ifdef DEBUG
  printf("++> set_contains(%d)\n", val);
  IO_FLUSH;
# endif

  if (!td) {
    node = set->head;
    for (i = set->level; i >= 0; i--) {
      next = node->forward[i];
      while (next->val < val) {
        node = next;
        next = node->forward[i];
      }
    }
    node = node->forward[0];
    result = (node->val == val);
  } else {
    TM_START(0, RO);
    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
    node = set->head;
    for (i = TM_LOAD(&set->level); i >= 0; i--) {
      next = (node_t *)TM_LOAD(&node->forward[i]);
      while (1) {
        v = TM_LOAD(&next->val);
        if (v >= val)
          break;
        node = next;
        next = (node_t *)TM_LOAD(&node->forward[i]);
      }
    }
    result = (v == val);
    TM_COMMIT;
  }

  return result;
}

static int set_add(intset_t *set, val_t val, thread_data_t *td)
{
  int result, i;
  node_t *update[MAX_LEVEL + 1];
  node_t *node, *next;
  level_t level, l;
  val_t v;

# ifdef DEBUG
  printf("++> set_add(%d)\n", val);
  IO_FLUSH;
# endif

  if (!td) {
    node = set->head;
    for (i = set->level; i >= 0; i--) {
      next = node->forward[i];
      while (next->val < val) {
        node = next;
        next = node->forward[i];
      }
      update[i] = node;
    }
    node = node->forward[0];

    if (node->val == val) {
      result = 0;
    } else {
      l = random_level(set, main_seed);
      if (l > set->level) {
        for (i = set->level + 1; i <= l; i++)
          update[i] = set->head;
        set->level = l;
      }
      node = new_node(val, l, 0);
      for (i = 0; i <= l; i++) {
        node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = node;
      }
      result = 1;
    }
  } else {
    TM_START(1, RW);
    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
    node = set->head;
    level = TM_LOAD(&set->level);
    for (i = level; i >= 0; i--) {
      next = (node_t *)TM_LOAD(&node->forward[i]);
      while (1) {
        v = TM_LOAD(&next->val);
        if (v >= val)
          break;
        node = next;
        next = (node_t *)TM_LOAD(&node->forward[i]);
      }
      update[i] = node;
    }

    if (v == val) {
      result = 0;
    } else {
      l = random_level(set, td->seed);
      if (l > level) {
        for (i = level + 1; i <= l; i++)
          update[i] = set->head;
        TM_STORE(&set->level, l);
      }
      node = new_node(val, l, 1);
      for (i = 0; i <= l; i++) {
        node->forward[i] = (node_t *)TM_LOAD(&update[i]->forward[i]);
        TM_STORE(&update[i]->forward[i], node);
      }
      result = 1;
    }
    TM_COMMIT;
  }

  return result;
}

static int set_remove(intset_t *set, val_t val, thread_data_t *td)
{
  int result, i;
  node_t *update[MAX_LEVEL + 1];
  node_t *node, *next;
  level_t level;
  val_t v;

# ifdef DEBUG
  printf("++> set_remove(%d)\n", val);
  IO_FLUSH;
# endif

  if (!td) {
    node = set->head;
    for (i = set->level; i >= 0; i--) {
      next = node->forward[i];
      while (next->val < val) {
        node = next;
        next = node->forward[i];
      }
      update[i] = node;
    }
    node = node->forward[0];

    if (node->val != val) {
      result = 0;
    } else {
      for (i = 0; i <= set->level; i++) {
        if (update[i]->forward[i] == node)
          update[i]->forward[i] = node->forward[i];
      }
      while (set->level > 0 && set->head->forward[set->level]->forward[0] == NULL)
        set->level--;
      free(node);
      result = 1;
    }
  } else {
    TM_START(2, RW);
    v = VAL_MIN; /* Avoid compiler warning (should not be necessary) */
    node = set->head;
    level = TM_LOAD(&set->level);
    for (i = level; i >= 0; i--) {
      next = (node_t *)TM_LOAD(&node->forward[i]);
      while (1) {
        v = TM_LOAD(&next->val);
        if (v >= val)
          break;
        node = next;
        next = (node_t *)TM_LOAD(&node->forward[i]);
      }
      update[i] = node;
    }
    node = (node_t *)TM_LOAD(&node->forward[0]);

    if (v != val) {
      result = 0;
    } else {
      for (i = 0; i <= level; i++) {
        if ((node_t *)TM_LOAD(&update[i]->forward[i]) == node)
          TM_STORE(&update[i]->forward[i], (node_t *)TM_LOAD(&node->forward[i]));
      }
      i = level;
      while (i > 0 && (node_t *)TM_LOAD(&set->head->forward[i]) == set->tail)
        i--;
      if (i != level)
        TM_STORE(&set->level, i);
      /* Free memory (delayed until commit) */
      TM_FREE2(node, sizeof(node_t) + node->level * sizeof(node_t *));
      result = 1;
    }
    TM_COMMIT;
  }

  return result;
}

#elif defined(USE_HASHSET)

/* ################################################################### *
 * HASHSET
 * ################################################################### */

# define INIT_SET_PARAMETERS            /* Nothing */

# define NB_BUCKETS                     (1UL << 17)

# define HASH(a)                        (hash((uint32_t)a) & (NB_BUCKETS - 1))

typedef intptr_t val_t;

typedef struct bucket {
  val_t val;
  struct bucket *next;
} bucket_t;

typedef struct intset {
  bucket_t **buckets;
} intset_t;

TM_PURE
static uint32_t hash(uint32_t a)
{
  /* Knuth's multiplicative hash function */
  a *= 2654435761UL;
  return a;
}

TM_SAFE
static bucket_t *new_entry(val_t val, bucket_t *next, int transactional)
{
  bucket_t *b;

  if (!transactional) {
    b = (bucket_t *)malloc(sizeof(bucket_t));
  } else {
    b = (bucket_t *)TM_MALLOC(sizeof(bucket_t));
  }
  if (b == NULL) {
    perror("malloc");
    exit(1);
  }

  b->val = val;
  b->next = next;

  return b;
}

static intset_t *set_new()
{
  intset_t *set;

  if ((set = (intset_t *)malloc(sizeof(intset_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  if ((set->buckets = (bucket_t **)calloc(NB_BUCKETS, sizeof(bucket_t *))) == NULL) {
    perror("calloc");
    exit(1);
  }

  return set;
}

static void set_delete(intset_t *set)
{
  unsigned int i;
  bucket_t *b, *next;

  for (i = 0; i < NB_BUCKETS; i++) {
    b = set->buckets[i];
    while (b != NULL) {
      next = b->next;
      free(b);
      b = next;
    }
  }
  free(set->buckets);
  free(set);
}

static int set_size(intset_t *set)
{
  int size = 0;
  unsigned int i;
  bucket_t *b;

  for (i = 0; i < NB_BUCKETS; i++) {
    b = set->buckets[i];
    while (b != NULL) {
      size++;
      b = b->next;
    }
  }

  return size;
}

static int set_contains(intset_t *set, val_t val, thread_data_t *td)
{
  int result, i;
  bucket_t *b;

# ifdef DEBUG
  printf("++> set_contains(%d)\n", val);
  IO_FLUSH;
# endif

  if (!td) {
    i = HASH(val);
    b = set->buckets[i];
    result = 0;
    while (b != NULL) {
      if (b->val == val) {
        result = 1;
        break;
      }
      b = b->next;
    }
  } else {
    TM_START(0, RO);
    i = HASH(val);
    b = (bucket_t *)TM_LOAD(&set->buckets[i]);
    result = 0;
    while (b != NULL) {
      if (TM_LOAD(&b->val) == val) {
        result = 1;
        break;
      }
      b = (bucket_t *)TM_LOAD(&b->next);
    }
    TM_COMMIT;
  }

  return result;
}

static int set_add(intset_t *set, val_t val, thread_data_t *td)
{
  int result, i;
  bucket_t *b, *first;

# ifdef DEBUG
  printf("++> set_add(%d)\n", val);
  IO_FLUSH;
# endif

  if (!td) {
    i = HASH(val);
    first = b = set->buckets[i];
    result = 1;
    while (b != NULL) {
      if (b->val == val) {
        result = 0;
        break;
      }
      b = b->next;
    }
    if (result) {
      set->buckets[i] = new_entry(val, first, 0);
    }
  } else {
    TM_START(0, RW);
    i = HASH(val);
    first = b = (bucket_t *)TM_LOAD(&set->buckets[i]);
    result = 1;
    while (b != NULL) {
      if (TM_LOAD(&b->val) == val) {
        result = 0;
        break;
      }
      b = (bucket_t *)TM_LOAD(&b->next);
    }
    if (result) {
      TM_STORE(&set->buckets[i], new_entry(val, first, 1));
    }
    TM_COMMIT;
  }

  return result;
}

static int set_remove(intset_t *set, val_t val, thread_data_t *td)
{
  int result, i;
  bucket_t *b, *prev;

# ifdef DEBUG
  printf("++> set_remove(%d)\n", val);
  IO_FLUSH;
# endif

  if (!td) {
    i = HASH(val);
    prev = b = set->buckets[i];
    result = 0;
    while (b != NULL) {
      if (b->val == val) {
        result = 1;
        break;
      }
      prev = b;
      b = b->next;
    }
    if (result) {
      if (prev == b) {
        /* First element of bucket */
        set->buckets[i] = b->next;
      } else {
        prev->next = b->next;
      }
      free(b);
    }
  } else {
    TM_START(0, RW);
    i = HASH(val);
    prev = b = (bucket_t *)TM_LOAD(&set->buckets[i]);
    result = 0;
    while (b != NULL) {
      if (TM_LOAD(&b->val) == val) {
        result = 1;
        break;
      }
      prev = b;
      b = (bucket_t *)TM_LOAD(&b->next);
    }
    if (result) {
      if (prev == b) {
        /* First element of bucket */
        TM_STORE(&set->buckets[i], TM_LOAD(&b->next));
      } else {
        TM_STORE(&prev->next, TM_LOAD(&b->next));
      }
      /* Free memory (delayed until commit) */
      TM_FREE2(b, sizeof(bucket_t));
    }
    TM_COMMIT;
  }

  return result;
}

#endif /* defined(USE_HASHSET) */

/* ################################################################### *
 * BARRIER
 * ################################################################### */

typedef struct barrier {
  pthread_cond_t complete;
  pthread_mutex_t mutex;
  int count;
  int crossing;
} barrier_t;

static void barrier_init(barrier_t *b, int n)
{
  pthread_cond_init(&b->complete, NULL);
  pthread_mutex_init(&b->mutex, NULL);
  b->count = n;
  b->crossing = 0;
}

static void barrier_cross(barrier_t *b)
{
  pthread_mutex_lock(&b->mutex);
  /* One more thread through */
  b->crossing++;
  /* If not all here, wait */
  if (b->crossing < b->count) {
    pthread_cond_wait(&b->complete, &b->mutex);
  } else {
    pthread_cond_broadcast(&b->complete);
    /* Reset for next time */
    b->crossing = 0;
  }
  pthread_mutex_unlock(&b->mutex);
}

/* ################################################################### *
 * STRESS TEST
 * ################################################################### */

static void *test(void *data)
{
  int op, val, last = -1;
  thread_data_t *d = (thread_data_t *)data;

  /* Create transaction */
  TM_INIT_THREAD;
  /* Wait on barrier */
  barrier_cross(d->barrier);

  while (stop == 0) {
    op = rand_range(100, d->seed);
    if (op < d->update) {
      if (d->alternate) {
        /* Alternate insertions and removals */
        if (last < 0) {
          /* Add random value */
          val = rand_range(d->range, d->seed) + 1;
          if (set_add(d->set, val, d)) {
            d->diff++;
            last = val;
          }
          d->nb_add++;
        } else {
          /* Remove last value */
          if (set_remove(d->set, last, d))
            d->diff--;
          d->nb_remove++;
          last = -1;
        }
      } else {
        /* Randomly perform insertions and removals */
        val = rand_range(d->range, d->seed) + 1;
        if ((op & 0x01) == 0) {
          /* Add random value */
          if (set_add(d->set, val, d))
            d->diff++;
          d->nb_add++;
        } else {
          /* Remove random value */
          if (set_remove(d->set, val, d))
            d->diff--;
          d->nb_remove++;
        }
      }
    } else {
      /* Look for random value */
      val = rand_range(d->range, d->seed) + 1;
      if (set_contains(d->set, val, d))
        d->nb_found++;
      d->nb_contains++;
    }
  }
#ifndef TM_COMPILER
  stm_get_stats("nb_aborts", &d->nb_aborts);
  stm_get_stats("nb_aborts_1", &d->nb_aborts_1);
  stm_get_stats("nb_aborts_2", &d->nb_aborts_2);
  stm_get_stats("nb_aborts_locked_read", &d->nb_aborts_locked_read);
  stm_get_stats("nb_aborts_locked_write", &d->nb_aborts_locked_write);
  stm_get_stats("nb_aborts_validate_read", &d->nb_aborts_validate_read);
  stm_get_stats("nb_aborts_validate_write", &d->nb_aborts_validate_write);
  stm_get_stats("nb_aborts_validate_commit", &d->nb_aborts_validate_commit);
  stm_get_stats("nb_aborts_invalid_memory", &d->nb_aborts_invalid_memory);
  stm_get_stats("nb_aborts_killed", &d->nb_aborts_killed);
  stm_get_stats("locked_reads_ok", &d->locked_reads_ok);
  stm_get_stats("locked_reads_failed", &d->locked_reads_failed);
  stm_get_stats("max_retries", &d->max_retries);
#endif /* ! TM_COMPILER */
  /* Free transaction */
  TM_EXIT_THREAD;

  return NULL;
}

int main(int argc, char **argv)
{
  struct option long_options[] = {
    // These options don't set a flag
    {"help",                      no_argument,       NULL, 'h'},
    {"do-not-alternate",          no_argument,       NULL, 'a'},
#ifndef TM_COMPILER
    {"contention-manager",        required_argument, NULL, 'c'},
#endif /* ! TM_COMPILER */
    {"duration",                  required_argument, NULL, 'd'},
    {"initial-size",              required_argument, NULL, 'i'},
    {"num-threads",               required_argument, NULL, 'n'},
    {"range",                     required_argument, NULL, 'r'},
    {"seed",                      required_argument, NULL, 's'},
    {"update-rate",               required_argument, NULL, 'u'},
#ifdef USE_LINKEDLIST
    {"unit-tx",                   no_argument,       NULL, 'x'},
#endif /* LINKEDLIST */
    {NULL, 0, NULL, 0}
  };

  intset_t *set;
  int i, c, val, size, ret;
  unsigned long reads, updates;
#ifndef TM_COMPILER
  char *s;
  unsigned long aborts, aborts_1, aborts_2,
    aborts_locked_read, aborts_locked_write,
    aborts_validate_read, aborts_validate_write, aborts_validate_commit,
    aborts_invalid_memory, aborts_killed,
    locked_reads_ok, locked_reads_failed, max_retries;
  stm_ab_stats_t ab_stats;
#endif /* ! TM_COMPILER */
  thread_data_t *data;
  pthread_t *threads;
  pthread_attr_t attr;
  barrier_t barrier;
  struct timeval start, end;
  struct timespec timeout;
  int duration = DEFAULT_DURATION;
  int initial = DEFAULT_INITIAL;
  int nb_threads = DEFAULT_NB_THREADS;
  int range = DEFAULT_RANGE;
  int seed = DEFAULT_SEED;
  int update = DEFAULT_UPDATE;
  int alternate = 1;
#ifndef TM_COMPILER
  char *cm = NULL;
#endif /* ! TM_COMPILER */
#ifdef USE_LINKEDLIST
  int unit_tx = 0;
#endif /* LINKEDLIST */
  sigset_t block_set;

  while(1) {
    i = 0;
    c = getopt_long(argc, argv, "ha"
#ifndef TM_COMPILER
                    "c:"
#endif /* ! TM_COMPILER */
                    "d:i:n:r:s:u:"
#ifdef USE_LINKEDLIST
                    "x"
#endif /* LINKEDLIST */
                    , long_options, &i);

    if(c == -1)
      break;

    if(c == 0 && long_options[i].flag == 0)
      c = long_options[i].val;

    switch(c) {
     case 0:
       /* Flag is automatically set */
       break;
     case 'h':
       printf("intset -- STM stress test "
#if defined(USE_LINKEDLIST)
              "(linked list)\n"
#elif defined(USE_RBTREE)
              "(red-black tree)\n"
#elif defined(USE_SKIPLIST)
              "(skip list)\n"
#elif defined(USE_HASHSET)
              "(hash set)\n"
#endif /* defined(USE_HASHSET) */
              "\n"
              "Usage:\n"
              "  intset [options...]\n"
              "\n"
              "Options:\n"
              "  -h, --help\n"
              "        Print this message\n"
              "  -a, --do-not-alternate\n"
              "        Do not alternate insertions and removals\n"
#ifndef TM_COMPILER
	      "  -c, --contention-manager <string>\n"
              "        Contention manager for resolving conflicts (default=suicide)\n"
#endif /* ! TM_COMPILER */
	      "  -d, --duration <int>\n"
              "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
              "  -i, --initial-size <int>\n"
              "        Number of elements to insert before test (default=" XSTR(DEFAULT_INITIAL) ")\n"
              "  -n, --num-threads <int>\n"
              "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
              "  -r, --range <int>\n"
              "        Range of integer values inserted in set (default=" XSTR(DEFAULT_RANGE) ")\n"
              "  -s, --seed <int>\n"
              "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
              "  -u, --update-rate <int>\n"
              "        Percentage of update transactions (default=" XSTR(DEFAULT_UPDATE) ")\n"
#ifdef USE_LINKEDLIST
              "  -x, --unit-tx\n"
              "        Use unit transactions\n"
#endif /* LINKEDLIST */
         );
       exit(0);
     case 'a':
       alternate = 0;
       break;
#ifndef TM_COMPILER
     case 'c':
       cm = optarg;
       break;
#endif /* ! TM_COMPILER */
     case 'd':
       duration = atoi(optarg);
       break;
     case 'i':
       initial = atoi(optarg);
       break;
     case 'n':
       nb_threads = atoi(optarg);
       break;
     case 'r':
       range = atoi(optarg);
       break;
     case 's':
       seed = atoi(optarg);
       break;
     case 'u':
       update = atoi(optarg);
       break;
#ifdef USE_LINKEDLIST
     case 'x':
       unit_tx++;
       break;
#endif /* LINKEDLIST */
     case '?':
       printf("Use -h or --help for help\n");
       exit(0);
     default:
       exit(1);
    }
  }

  assert(duration >= 0);
  assert(initial >= 0);
  assert(nb_threads > 0);
  assert(range > 0 && range >= initial);
  assert(update >= 0 && update <= 100);

#if defined(USE_LINKEDLIST)
  printf("Set type     : linked list\n");
#elif defined(USE_RBTREE)
  printf("Set type     : red-black tree\n");
#elif defined(USE_SKIPLIST)
  printf("Set type     : skip list\n");
#elif defined(USE_HASHSET)
  printf("Set type     : hash set\n");
#endif /* defined(USE_HASHSET) */
#ifndef TM_COMPILER
  printf("CM           : %s\n", (cm == NULL ? "DEFAULT" : cm));
#endif /* ! TM_COMPILER */
  printf("Duration     : %d\n", duration);
  printf("Initial size : %d\n", initial);
  printf("Nb threads   : %d\n", nb_threads);
  printf("Value range  : %d\n", range);
  printf("Seed         : %d\n", seed);
  printf("Update rate  : %d\n", update);
  printf("Alternate    : %d\n", alternate);
#ifdef USE_LINKEDLIST
  printf("Unit tx      : %d\n", unit_tx);
#endif /* LINKEDLIST */
  printf("Type sizes   : int=%d/long=%d/ptr=%d/word=%d\n",
         (int)sizeof(int),
         (int)sizeof(long),
         (int)sizeof(void *),
         (int)sizeof(size_t));

  timeout.tv_sec = duration / 1000;
  timeout.tv_nsec = (duration % 1000) * 1000000;

  if ((data = (thread_data_t *)malloc(nb_threads * sizeof(thread_data_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  if ((threads = (pthread_t *)malloc(nb_threads * sizeof(pthread_t))) == NULL) {
    perror("malloc");
    exit(1);
  }

  if (seed == 0)
    srand((int)time(NULL));
  else
    srand(seed);

  set = set_new(INIT_SET_PARAMETERS);

  stop = 0;

  /* Thread-local seed for main thread */
  rand_init(main_seed);

  /* Init STM */
  printf("Initializing STM\n");
  TM_INIT;

#ifndef TM_COMPILER
  if (stm_get_parameter("compile_flags", &s))
    printf("STM flags    : %s\n", s);

  if (cm != NULL) {
    if (stm_set_parameter("cm_policy", cm) == 0)
      printf("WARNING: cannot set contention manager \"%s\"\n", cm);
  }
#endif /* ! TM_COMPILER */
  if (alternate == 0 && range != initial * 2)
    printf("WARNING: range is not twice the initial set size\n");

  /* Populate set */
  printf("Adding %d entries to set\n", initial);
  i = 0;
  while (i < initial) {
    val = rand_range(range, main_seed) + 1;
    if (set_add(set, val, 0))
      i++;
  }
  size = set_size(set);
  printf("Set size     : %d\n", size);

  /* Access set from all threads */
  barrier_init(&barrier, nb_threads + 1);
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  for (i = 0; i < nb_threads; i++) {
    printf("Creating thread %d\n", i);
    data[i].range = range;
    data[i].update = update;
    data[i].alternate = alternate;
#ifdef USE_LINKEDLIST
    data[i].unit_tx = unit_tx;
#endif /* LINKEDLIST */
    data[i].nb_add = 0;
    data[i].nb_remove = 0;
    data[i].nb_contains = 0;
    data[i].nb_found = 0;
#ifndef TM_COMPILER
    data[i].nb_aborts = 0;
    data[i].nb_aborts_1 = 0;
    data[i].nb_aborts_2 = 0;
    data[i].nb_aborts_locked_read = 0;
    data[i].nb_aborts_locked_write = 0;
    data[i].nb_aborts_validate_read = 0;
    data[i].nb_aborts_validate_write = 0;
    data[i].nb_aborts_validate_commit = 0;
    data[i].nb_aborts_invalid_memory = 0;
    data[i].nb_aborts_killed = 0;
    data[i].locked_reads_ok = 0;
    data[i].locked_reads_failed = 0;
    data[i].max_retries = 0;
#endif /* ! TM_COMPILER */
    data[i].diff = 0;
    rand_init(data[i].seed);
    data[i].set = set;
    data[i].barrier = &barrier;
    if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) {
      fprintf(stderr, "Error creating thread\n");
      exit(1);
    }
  }
  pthread_attr_destroy(&attr);

  /* Start threads */
  barrier_cross(&barrier);

  printf("STARTING...\n");
  gettimeofday(&start, NULL);
  if (duration > 0) {
    nanosleep(&timeout, NULL);
  } else {
    sigemptyset(&block_set);
    sigsuspend(&block_set);
  }
  stop = 1;
  gettimeofday(&end, NULL);
  printf("STOPPING...\n");

  /* Wait for thread completion */
  for (i = 0; i < nb_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      fprintf(stderr, "Error waiting for thread completion\n");
      exit(1);
    }
  }

  duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);
#ifndef TM_COMPILER
  aborts = 0;
  aborts_1 = 0;
  aborts_2 = 0;
  aborts_locked_read = 0;
  aborts_locked_write = 0;
  aborts_validate_read = 0;
  aborts_validate_write = 0;
  aborts_validate_commit = 0;
  aborts_invalid_memory = 0;
  aborts_killed = 0;
  locked_reads_ok = 0;
  locked_reads_failed = 0;
  max_retries = 0;
#endif /* ! TM_COMPILER */
  reads = 0;
  updates = 0;
  for (i = 0; i < nb_threads; i++) {
    printf("Thread %d\n", i);
    printf("  #add        : %lu\n", data[i].nb_add);
    printf("  #remove     : %lu\n", data[i].nb_remove);
    printf("  #contains   : %lu\n", data[i].nb_contains);
    printf("  #found      : %lu\n", data[i].nb_found);
#ifndef TM_COMPILER
    printf("  #aborts     : %lu\n", data[i].nb_aborts);
    printf("    #lock-r   : %lu\n", data[i].nb_aborts_locked_read);
    printf("    #lock-w   : %lu\n", data[i].nb_aborts_locked_write);
    printf("    #val-r    : %lu\n", data[i].nb_aborts_validate_read);
    printf("    #val-w    : %lu\n", data[i].nb_aborts_validate_write);
    printf("    #val-c    : %lu\n", data[i].nb_aborts_validate_commit);
    printf("    #inv-mem  : %lu\n", data[i].nb_aborts_invalid_memory);
    printf("    #killed   : %lu\n", data[i].nb_aborts_killed);
    printf("  #aborts>=1  : %lu\n", data[i].nb_aborts_1);
    printf("  #aborts>=2  : %lu\n", data[i].nb_aborts_2);
    printf("  #lr-ok      : %lu\n", data[i].locked_reads_ok);
    printf("  #lr-failed  : %lu\n", data[i].locked_reads_failed);
    printf("  Max retries : %lu\n", data[i].max_retries);
    aborts += data[i].nb_aborts;
    aborts_1 += data[i].nb_aborts_1;
    aborts_2 += data[i].nb_aborts_2;
    aborts_locked_read += data[i].nb_aborts_locked_read;
    aborts_locked_write += data[i].nb_aborts_locked_write;
    aborts_validate_read += data[i].nb_aborts_validate_read;
    aborts_validate_write += data[i].nb_aborts_validate_write;
    aborts_validate_commit += data[i].nb_aborts_validate_commit;
    aborts_invalid_memory += data[i].nb_aborts_invalid_memory;
    aborts_killed += data[i].nb_aborts_killed;
    locked_reads_ok += data[i].locked_reads_ok;
    locked_reads_failed += data[i].locked_reads_failed;
    if (max_retries < data[i].max_retries)
      max_retries = data[i].max_retries;
#endif /* ! TM_COMPILER */
    reads += data[i].nb_contains;
    updates += (data[i].nb_add + data[i].nb_remove);
    size += data[i].diff;
  }
  printf("Set size      : %d (expected: %d)\n", set_size(set), size);
  ret = (set_size(set) != size);
  printf("Duration      : %d (ms)\n", duration);
  printf("#txs          : %lu (%f / s)\n", reads + updates, (reads + updates) * 1000.0 / duration);
  printf("#read txs     : %lu (%f / s)\n", reads, reads * 1000.0 / duration);
  printf("#update txs   : %lu (%f / s)\n", updates, updates * 1000.0 / duration);
#ifndef TM_COMPILER
  printf("#aborts       : %lu (%f / s)\n", aborts, aborts * 1000.0 / duration);
  printf("  #lock-r     : %lu (%f / s)\n", aborts_locked_read, aborts_locked_read * 1000.0 / duration);
  printf("  #lock-w     : %lu (%f / s)\n", aborts_locked_write, aborts_locked_write * 1000.0 / duration);
  printf("  #val-r      : %lu (%f / s)\n", aborts_validate_read, aborts_validate_read * 1000.0 / duration);
  printf("  #val-w      : %lu (%f / s)\n", aborts_validate_write, aborts_validate_write * 1000.0 / duration);
  printf("  #val-c      : %lu (%f / s)\n", aborts_validate_commit, aborts_validate_commit * 1000.0 / duration);
  printf("  #inv-mem    : %lu (%f / s)\n", aborts_invalid_memory, aborts_invalid_memory * 1000.0 / duration);
  printf("  #killed     : %lu (%f / s)\n", aborts_killed, aborts_killed * 1000.0 / duration);
  printf("#aborts>=1    : %lu (%f / s)\n", aborts_1, aborts_1 * 1000.0 / duration);
  printf("#aborts>=2    : %lu (%f / s)\n", aborts_2, aborts_2 * 1000.0 / duration);
  printf("#lr-ok        : %lu (%f / s)\n", locked_reads_ok, locked_reads_ok * 1000.0 / duration);
  printf("#lr-failed    : %lu (%f / s)\n", locked_reads_failed, locked_reads_failed * 1000.0 / duration);
  printf("Max retries   : %lu\n", max_retries);

  for (i = 0; stm_get_ab_stats(i, &ab_stats) != 0; i++) {
    printf("Atomic block  : %d\n", i);
    printf("  #samples    : %lu\n", ab_stats.samples);
    printf("  Mean        : %f\n", ab_stats.mean);
    printf("  Variance    : %f\n", ab_stats.variance);
    printf("  Min         : %f\n", ab_stats.min); 
    printf("  Max         : %f\n", ab_stats.max);
    printf("  50th perc.  : %f\n", ab_stats.percentile_50);
    printf("  90th perc.  : %f\n", ab_stats.percentile_90);
    printf("  95th perc.  : %f\n", ab_stats.percentile_95);
  }
#endif /* ! TM_COMPILER */

  /* Delete set */
  set_delete(set);

  /* Cleanup STM */
  TM_EXIT;

  free(threads);
  free(data);

  return ret;
}
