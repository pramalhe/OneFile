/*
 * File:
 *   mod_ab.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Module for gathering statistics about atomic blocks.
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
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <pthread.h>

#include "mod_ab.h"

#include "atomic.h"
#include "stm.h"
#include "utils.h"

/* ################################################################### *
 * TYPES
 * ################################################################### */

#define NB_ATOMIC_BLOCKS                64
#define BUFFER_SIZE                     1024
#define RESERVOIR_SIZE                  "RESERVOIR_SIZE"
#define RESERVOIR_SIZE_DEFAULT          1000
#define SAMPLING_PERIOD_DEFAULT         1024

typedef struct smart_counter {          /* Smart counter */
  unsigned long samples;                /* Number of samples */
  double mean;                          /* Mean */
  double variance;                      /* Variance */
  double min;                           /* Minimum */
  double max;                           /* Maximum */
  double *reservoir;                    /* Vitter's reservoir */
  int sorted;                           /* Is the reservoir sorted? */
} smart_counter_t;

typedef struct ab_stats {               /* Atomic block statistics */
  int id;                               /* Atomic block identifier */
  struct ab_stats *next;                /* Next atomic block */
  smart_counter_t stats;                /* Length statistics */
} ab_stats_t;

typedef struct samples_buffer {         /* Buffer to hold samples */
  struct {
    int id;                             /* Atomic block identifier */
    unsigned long length;               /* Transaction length */
  } buffer[BUFFER_SIZE];                /* Buffer */
  unsigned int nb;                      /* Number of samples */
  unsigned int total;                   /* Total number of valid samples seen by thread so far */
  uint64_t start;                       /* Start time of the current transaction */
  unsigned short seed[3];               /* Thread-local PNRG's seed */
} samples_buffer_t;

static int mod_ab_key;
static int mod_ab_initialized = 0;
static int sampling_period;             /* Inverse sampling frequency */
static unsigned int reservoir_size;     /* Size of Vitter's reservoir */
static int (*check_fn)(void);           /* Function to check sample validity */

static unsigned int seed;               /* Global PNRG's seed */

static pthread_mutex_t ab_mutex;        /* Mutex to update global statistics */

static ab_stats_t *ab_list[NB_ATOMIC_BLOCKS];

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

/*
 * Round double to int.
 */
static int sc_round(double d)
{
  return (int)(d + 0.5);
}

/*
 * Compare doubles.
 */
static int compare_doubles(const void *a, const void *b)
{
  const double *da = (const double *)a;
  const double *db = (const double *)b;
  return (*da < *db ? -1 : (*da > *db ? 1 : 0));
}

/*
 * Initialize smart counter.
 */
static void sc_init(smart_counter_t *c)
{
  c->samples = 0;
  c->mean = c->variance = c->min = c->max = 0;
  c->sorted = 1;
  c->reservoir = (double *)xmalloc(reservoir_size * sizeof(double));
}

/*
 * Add sample in smart counter.
 */
static void sc_add_sample(smart_counter_t *c, double n, unsigned short *seed)
{
  double prev, prob;

  /* Update mean, variance, min, max */
  prev = c->mean;
  if (c->samples == 0)
    c->min = c->max = n;
  else if (n < c->min)
    c->min = n;
  else if (n > c->max)
    c->max = n;
  c->mean = c->mean + (n - c->mean) / (double)(c->samples + 1);
  c->variance = c->variance + (n - prev) * (n - c->mean);

  /* Add sample to reservoir */
  if (c->samples < reservoir_size) {
    c->reservoir[c->samples] = n;
    c->sorted = 0;
  } else {
    prob = reservoir_size / (double)(c->samples + 1);
    if (erand48(seed) <= prob) {
      /* Replace random element */
      c->reservoir[nrand48(seed) % reservoir_size] = n;
    }
    c->sorted = 0;
  }

  c->samples++;
}

/*
 * Get number of samples of smart counter.
 */
static unsigned long sc_samples(smart_counter_t *c)
{
  return c->samples;
}

/*
 * Get mean of smart counter.
 */
static double sc_mean(smart_counter_t *c)
{
  return c->mean;
}

/*
 * Get variance of smart counter.
 */
static double sc_variance(smart_counter_t *c)
{
  if(c->samples <= 1)
    return 0.0;
  return c->variance / (c->samples - 1);
}

/*
 * Get min of smart counter.
 */
static double sc_min(smart_counter_t *c)
{
  return c->min;
}

/*
 * Get max of smart counter.
 */
static double sc_max(smart_counter_t *c)
{
  return c->max;
}

/*
 * Get specific percentile.
 */
static double sc_percentile(smart_counter_t *c, int percentile)
{
  int length, i;

  length = c->samples < reservoir_size ? c->samples : reservoir_size;
  i = sc_round(length * percentile / 100.0);

  if (i <= 0)
    return c->min;
  if (i >= length)
    return c->max;

  /* Sort array (if not yet sorted) */
  if (!c->sorted) {
    qsort(c->reservoir, length, sizeof(double), compare_doubles);
    c->sorted = 1;
  }

  return c->reservoir[i];
}

/*
 * Returns a time measurement (clock ticks for x86).
 */
static inline uint64_t get_time(void) {
#if defined(__x86_64__) || defined(__amd64__) || defined(__i386__)
  uint32_t lo, hi;
  /* Note across cores the counter is not fully synchronized.
   * The serializing instruction is rdtscp. */
  __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
  /* __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi) :: "ecx" ); */
  return (((uint64_t)hi) << 32) | (((uint64_t)lo) & 0xffffffff);
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint64_t)(tv.tv_sec * 1000000 + tv.tv_usec);
#endif
}

/*
 * Add samples to global stats.
 */
static void sc_add_samples(samples_buffer_t *samples)
{
  int i, id, bucket;
  ab_stats_t *ab;

  pthread_mutex_lock(&ab_mutex);
  for (i = 0; i < samples->nb; i++) {
    id = samples->buffer[i].id;
    /* Find bucket */
    bucket = abs(id) % NB_ATOMIC_BLOCKS;
    /* Search for entry in bucket */
    ab = ab_list[bucket];
    while (ab != NULL && ab->id != id)
      ab = ab->next;
    if (ab == NULL) {
      /* No entry yet: create one */
      ab = (ab_stats_t *)xmalloc(sizeof(ab_stats_t));
      ab->id = id;
      ab->next = ab_list[bucket];
      sc_init(&ab->stats);
      ab_list[bucket] = ab;
    }
    sc_add_sample(&ab->stats, (double)samples->buffer[i].length, samples->seed);
  }
  samples->nb = 0;
  pthread_mutex_unlock(&ab_mutex);
}

/*
 * Clean up module.
 */
static void cleanup(void)
{
  int i;
  ab_stats_t *ab, *n;

  pthread_mutex_lock(&ab_mutex);
  for (i = 0; i < NB_ATOMIC_BLOCKS; i++) {
    ab = ab_list[i];
    while (ab != NULL) {
      n = ab->next;
      xfree(ab->stats.reservoir);
      xfree(ab);
      ab = n;
    }
  }
  pthread_mutex_unlock(&ab_mutex);

  pthread_mutex_destroy(&ab_mutex);
}

/*
 * Called upon thread creation.
 */
static void mod_ab_on_thread_init(void *arg)
{
  samples_buffer_t *samples;

  samples = (samples_buffer_t *)xmalloc(sizeof(samples_buffer_t));
  samples->nb = 0;
  samples->total = 0;
  /* Initialize thread-local seed in mutual exclution */
  pthread_mutex_lock(&ab_mutex);
  samples->seed[0] = (unsigned short)rand_r(&seed);
  samples->seed[1] = (unsigned short)rand_r(&seed);
  samples->seed[2] = (unsigned short)rand_r(&seed);
  pthread_mutex_unlock(&ab_mutex);
  stm_set_specific(mod_ab_key, samples);
}

/*
 * Called upon thread deletion.
 */
static void mod_ab_on_thread_exit(void *arg)
{
  samples_buffer_t *samples;

  samples = (samples_buffer_t *)stm_get_specific(mod_ab_key);
  assert(samples != NULL);

  sc_add_samples(samples);

  xfree(samples);
}

/*
 * Called upon transaction start.
 */
static void mod_ab_on_start(void *arg)
{
  samples_buffer_t *samples;

  samples = (samples_buffer_t *)stm_get_specific(mod_ab_key);
  assert(samples != NULL);

  samples->start = get_time();
}

/*
 * Called upon transaction commit.
 */
static void mod_ab_on_commit(void *arg)
{
  samples_buffer_t *samples;
  stm_tx_attr_t attrs;
  unsigned long length;

  samples = (samples_buffer_t *)stm_get_specific(mod_ab_key);
  assert(samples != NULL);

  if (check_fn == NULL || check_fn()) {
    length = get_time() - samples->start;
    samples->total++;
    /* Should be keep this sample? */
    if ((samples->total % sampling_period) == 0) {
      attrs = stm_get_attributes();
      samples->buffer[samples->nb].id = attrs.id;
      samples->buffer[samples->nb].length = length;
      /* Is buffer full? */
      if (++samples->nb == BUFFER_SIZE) {
        /* Accumulate in global stats (and empty buffer) */
        sc_add_samples(samples);
      }
    }
  }
}

/*
 * Called upon transaction abort.
 */
static void mod_ab_on_abort(void *arg)
{
  samples_buffer_t *samples;

  samples = (samples_buffer_t *)stm_get_specific(mod_ab_key);
  assert(samples != NULL);

  samples->start = get_time();
}

/*
 * Return statistics about atomic block.
 */
int stm_get_ab_stats(int id, stm_ab_stats_t *stats)
{
  int bucket, result;
  ab_stats_t *ab;

  result = 0;
  pthread_mutex_lock(&ab_mutex);
  /* Find bucket */
  bucket = abs(id) % NB_ATOMIC_BLOCKS;
  /* Search for entry in bucket */
  ab = ab_list[bucket];
  while (ab != NULL && ab->id != id)
    ab = ab->next;
  if (ab != NULL) {
    stats->samples = sc_samples(&ab->stats);
    stats->mean = sc_mean(&ab->stats);
    stats->variance = sc_variance(&ab->stats);
    stats->min = sc_min(&ab->stats);
    stats->max = sc_max(&ab->stats);
    stats->percentile_50 = sc_percentile(&ab->stats, 50);
    stats->percentile_90 = sc_percentile(&ab->stats, 90);
    stats->percentile_95 = sc_percentile(&ab->stats, 95);
    /* At this point, the reservoir is sorted */
    stats->reservoir = ab->stats.reservoir;
    stats->reservoir_size = ab->stats.samples < reservoir_size ? ab->stats.samples : reservoir_size;
    result = 1;
  }
  pthread_mutex_unlock(&ab_mutex);

  return result;
}

/*
 * Initialize module.
 */
void mod_ab_init(int freq, int (*check)(void))
{
  int i;
  char *s;

  if (mod_ab_initialized)
    return;

  /* Use random seed (we are in main thread) */
  seed = (unsigned int)rand();

  sampling_period = (freq <= 0 ? SAMPLING_PERIOD_DEFAULT : freq);
  s = getenv(RESERVOIR_SIZE);
  if (s != NULL)
    reservoir_size = (unsigned int)strtol(s, NULL, 10);
  else
    reservoir_size = RESERVOIR_SIZE_DEFAULT;
  check_fn = check;

  if (!stm_register(mod_ab_on_thread_init, mod_ab_on_thread_exit, mod_ab_on_start, NULL, mod_ab_on_commit, mod_ab_on_abort, NULL)) {
    fprintf(stderr, "Cannot register callbacks\n");
    exit(1);
  }
  mod_ab_key = stm_create_specific();
  if (mod_ab_key < 0) {
    fprintf(stderr, "Cannot create specific key\n");
    exit(1);
  }
  if (pthread_mutex_init(&ab_mutex, NULL) != 0) {
    fprintf(stderr, "Error creating mutex\n");
    exit(1);
  }
  for (i = 0; i < NB_ATOMIC_BLOCKS; i++)
    ab_list[i] = NULL;
  atexit(cleanup);
  mod_ab_initialized = 1;
}
