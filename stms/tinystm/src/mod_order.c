/*
 * File:
 *   mod_order.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Module to force transactions to commit in order.
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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stm.h>
#include "atomic.h"
#include "utils.h"
#include "mod_order.h"

#define KILL_SELF                      0x00
#define KILL_OTHER                     0x01

/* XXX Maybe these two could be in the same cacheline. */
ALIGNED static stm_word_t mod_order_ts_next = 0;
ALIGNED static stm_word_t mod_order_ts_commit = 0;
static int mod_order_key;
static int mod_order_initialized = 0;

static void mod_order_on_start(void *arg)
{
  stm_word_t ts;
  /* Get a timestamp for commit */
  ts = ATOMIC_FETCH_INC_FULL(&mod_order_ts_next);
  stm_set_specific(mod_order_key, (void *)ts);
}

static void mod_order_on_precommit(void *arg)
{
  stm_word_t my_ts, current_ts;
  my_ts = (stm_word_t)stm_get_specific(mod_order_key);
  /* Wait its turn... */
  do {
    current_ts = ATOMIC_LOAD(&mod_order_ts_commit);
    /* Check that we are not killed to keep the liveness, the transaction will
     * abort before to commit. Note that if the kill feature is not present, the
     * transaction must abort if it is not its turn to guarantee progress. */
    if (stm_killed())
      return;
  } while (current_ts != my_ts);
}

static void mod_order_on_commit(void *arg)
{
  /* Release next transaction to commit */
  ATOMIC_FETCH_INC_FULL(&mod_order_ts_commit);
}

static int mod_order_cm(struct stm_tx *tx, struct stm_tx *other_tx, int conflict)
{
  stm_word_t my_order = (stm_word_t)stm_get_specific_tx(tx, mod_order_key);
  stm_word_t other_order = (stm_word_t)stm_get_specific_tx(other_tx, mod_order_key);

  if (my_order < other_order)
    return KILL_OTHER;

  return KILL_SELF;
}

/*
 * Initialize module.
 */
void mod_order_init(void)
{
  if (mod_order_initialized)
    return;
#if CM == CM_MODULAR
  if (!stm_register(NULL, NULL, mod_order_on_start, mod_order_on_precommit, mod_order_on_commit, NULL, NULL)) {
    fprintf(stderr, "Could not set callbacks for module 'mod_order'. Exiting.\n");
    goto err;
  }
  if (stm_set_parameter("cm_function", mod_order_cm) == 0) {
    fprintf(stderr, "Could not set contention manager for module 'mod_order'. Exiting.\n");
    goto err;
  }
  mod_order_key = stm_create_specific();
  if (mod_order_key < 0) {
    fprintf(stderr, "Cannot create specific key\n");
    goto err;
  }
  mod_order_initialized = 1;
  return;
 err:
#else /* CM != CM_MODULAR */
  fprintf(stderr, "The 'mod_order' module requires CM_MODULAR.\n");
#endif /* CM != CM_MODULAR */
  exit(1);
}
