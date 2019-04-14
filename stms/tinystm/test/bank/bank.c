/*
 * File:
 *   bank.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Bank stress test.
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
#include <sys/time.h>
#include <time.h>

#ifdef DEBUG
# define IO_FLUSH                       fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#define RO                              1
#define RW                              0

#if defined(TM_GCC) 
# include "../../abi/gcc/tm_macros.h"
#elif defined(TM_DTMC) 
# include "../../abi/dtmc/tm_macros.h"
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

#include "stm.h"
#include "mod_ab.h"

/*
 * Useful macros to work with transactions. Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */

#define TM_START(tid, ro)               { stm_tx_attr_t _a = {{.id = tid, .read_only = ro}}; sigjmp_buf *_e = stm_start(_a); if (_e != NULL) sigsetjmp(*_e, 0)
#define TM_LOAD(addr)                   stm_load((stm_word_t *)addr)
#define TM_STORE(addr, value)           stm_store((stm_word_t *)addr, (stm_word_t)value)
#define TM_COMMIT                       stm_commit(); }

#define TM_INIT                         stm_init(); mod_ab_init(0, NULL)
#define TM_EXIT                         stm_exit()
#define TM_INIT_THREAD                  stm_init_thread()
#define TM_EXIT_THREAD                  stm_exit_thread()

#endif /* Compile with explicit calls to tinySTM */

#define DEFAULT_DURATION                10000
#define DEFAULT_NB_ACCOUNTS             1024
#define DEFAULT_NB_THREADS              1
#define DEFAULT_READ_ALL                20
#define DEFAULT_SEED                    0
#define DEFAULT_WRITE_ALL               0
#define DEFAULT_READ_THREADS            0
#define DEFAULT_WRITE_THREADS           0
#define DEFAULT_DISJOINT                0

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

static volatile int stop;

/* ################################################################### *
 * BANK ACCOUNTS
 * ################################################################### */

typedef struct account {
  long number;
  long balance;
} account_t;

typedef struct bank {
  account_t *accounts;
  long size;
} bank_t;

static int transfer(account_t *src, account_t *dst, int amount)
{
  long i;

  /* Allow overdrafts */
  TM_START(0, RW);
  i = TM_LOAD(&src->balance);
  i -= amount;
  TM_STORE(&src->balance, i);
  i = TM_LOAD(&dst->balance);
  i += amount;
  TM_STORE(&dst->balance, i);
  TM_COMMIT;

  return amount;
}

static int total(bank_t *bank, int transactional)
{
  long i, total;

  if (!transactional) {
    total = 0;
    for (i = 0; i < bank->size; i++) {
      total += bank->accounts[i].balance;
    }
  } else {
    TM_START(1, RO);
    total = 0;
    for (i = 0; i < bank->size; i++) {
      total += TM_LOAD(&bank->accounts[i].balance);
    }
    TM_COMMIT;
  }

  return total;
}

static void reset(bank_t *bank)
{
  long i;

  TM_START(2, RW);
  for (i = 0; i < bank->size; i++) {
    TM_STORE(&bank->accounts[i].balance, 0);
  }
  TM_COMMIT;
}

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

typedef struct thread_data {
  bank_t *bank;
  barrier_t *barrier;
  unsigned long nb_transfer;
  unsigned long nb_read_all;
  unsigned long nb_write_all;
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
  unsigned int seed;
  int id;
  int read_all;
  int read_threads;
  int write_all;
  int write_threads;
  int disjoint;
  int nb_threads;
  char padding[64];
} thread_data_t;

static void *test(void *data)
{
  int src, dst, nb;
  int rand_max, rand_min;
  thread_data_t *d = (thread_data_t *)data;
  unsigned short seed[3];

  /* Initialize seed (use rand48 as rand is poor) */
  seed[0] = (unsigned short)rand_r(&d->seed);
  seed[1] = (unsigned short)rand_r(&d->seed);
  seed[2] = (unsigned short)rand_r(&d->seed);

  /* Prepare for disjoint access */
  if (d->disjoint) {
    rand_max = d->bank->size / d->nb_threads;
    rand_min = rand_max * d->id;
    if (rand_max <= 2) {
      fprintf(stderr, "can't have disjoint account accesses");
      return NULL;
    }
  } else {
    rand_max = d->bank->size;
    rand_min = 0;
  }

  /* Create transaction */
  TM_INIT_THREAD;
  /* Wait on barrier */
  barrier_cross(d->barrier);

  while (stop == 0) {
    if (d->id < d->read_threads) {
      /* Read all */
      total(d->bank, 1);
      d->nb_read_all++;
    } else if (d->id < d->read_threads + d->write_threads) {
      /* Write all */
      reset(d->bank);
      d->nb_write_all++;
    } else {
      nb = (int)(erand48(seed) * 100);
      if (nb < d->read_all) {
        /* Read all */
        total(d->bank, 1);
        d->nb_read_all++;
      } else if (nb < d->read_all + d->write_all) {
        /* Write all */
        reset(d->bank);
        d->nb_write_all++;
      } else {
        /* Choose random accounts */
        src = (int)(erand48(seed) * rand_max) + rand_min;
        dst = (int)(erand48(seed) * rand_max) + rand_min;
        if (dst == src)
          dst = ((src + 1) % rand_max) + rand_min;
        transfer(&d->bank->accounts[src], &d->bank->accounts[dst], 1);
        d->nb_transfer++;
      }
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

static void catcher(int sig)
{
  static int nb = 0;
  printf("CAUGHT SIGNAL %d\n", sig);
  if (++nb >= 3)
    exit(1);
}

int main(int argc, char **argv)
{
  struct option long_options[] = {
    // These options don't set a flag
    {"help",                      no_argument,       NULL, 'h'},
    {"accounts",                  required_argument, NULL, 'a'},
    {"contention-manager",        required_argument, NULL, 'c'},
    {"duration",                  required_argument, NULL, 'd'},
    {"num-threads",               required_argument, NULL, 'n'},
    {"read-all-rate",             required_argument, NULL, 'r'},
    {"read-threads",              required_argument, NULL, 'R'},
    {"seed",                      required_argument, NULL, 's'},
    {"write-all-rate",            required_argument, NULL, 'w'},
    {"write-threads",             required_argument, NULL, 'W'},
    {"disjoint",                  no_argument,       NULL, 'j'},
    {NULL, 0, NULL, 0}
  };

  bank_t *bank;
  int i, c, ret;
  unsigned long reads, writes, updates;
#ifndef TM_COMPILER
  char *s;
  unsigned long aborts, aborts_1, aborts_2,
    aborts_locked_read, aborts_locked_write,
    aborts_validate_read, aborts_validate_write, aborts_validate_commit,
    aborts_invalid_memory, aborts_killed,
    locked_reads_ok, locked_reads_failed, max_retries;
  stm_ab_stats_t ab_stats;
  char *cm = NULL;
#endif /* ! TM_COMPILER */
  thread_data_t *data;
  pthread_t *threads;
  pthread_attr_t attr;
  barrier_t barrier;
  struct timeval start, end;
  struct timespec timeout;
  int duration = DEFAULT_DURATION;
  int nb_accounts = DEFAULT_NB_ACCOUNTS;
  int nb_threads = DEFAULT_NB_THREADS;
  int read_all = DEFAULT_READ_ALL;
  int read_threads = DEFAULT_READ_THREADS;
  int seed = DEFAULT_SEED;
  int write_all = DEFAULT_WRITE_ALL;
  int write_threads = DEFAULT_WRITE_THREADS;
  int disjoint = DEFAULT_DISJOINT;
  sigset_t block_set;

  while(1) {
    i = 0;
    c = getopt_long(argc, argv, "ha:c:d:n:r:R:s:w:W:j", long_options, &i);

    if(c == -1)
      break;

    if(c == 0 && long_options[i].flag == 0)
      c = long_options[i].val;

    switch(c) {
     case 0:
       /* Flag is automatically set */
       break;
     case 'h':
       printf("bank -- STM stress test\n"
              "\n"
              "Usage:\n"
              "  bank [options...]\n"
              "\n"
              "Options:\n"
              "  -h, --help\n"
              "        Print this message\n"
              "  -a, --accounts <int>\n"
              "        Number of accounts in the bank (default=" XSTR(DEFAULT_NB_ACCOUNTS) ")\n"
#ifndef TM_COMPILER
              "  -c, --contention-manager <string>\n"
              "        Contention manager for resolving conflicts (default=suicide)\n"
#endif /* ! TM_COMPILER */
              "  -d, --duration <int>\n"
              "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
              "  -n, --num-threads <int>\n"
              "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
              "  -r, --read-all-rate <int>\n"
              "        Percentage of read-all transactions (default=" XSTR(DEFAULT_READ_ALL) ")\n"
              "  -R, --read-threads <int>\n"
              "        Number of threads issuing only read-all transactions (default=" XSTR(DEFAULT_READ_THREADS) ")\n"
              "  -s, --seed <int>\n"
              "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
              "  -w, --write-all-rate <int>\n"
              "        Percentage of write-all transactions (default=" XSTR(DEFAULT_WRITE_ALL) ")\n"
              "  -W, --write-threads <int>\n"
              "        Number of threads issuing only write-all transactions (default=" XSTR(DEFAULT_WRITE_THREADS) ")\n"
         );
       exit(0);
     case 'a':
       nb_accounts = atoi(optarg);
       break;
#ifndef TM_COMPILER
     case 'c':
       cm = optarg;
       break;
#endif /* ! TM_COMPILER */
     case 'd':
       duration = atoi(optarg);
       break;
     case 'n':
       nb_threads = atoi(optarg);
       break;
     case 'r':
       read_all = atoi(optarg);
       break;
     case 'R':
       read_threads = atoi(optarg);
       break;
     case 's':
       seed = atoi(optarg);
       break;
     case 'w':
       write_all = atoi(optarg);
       break;
     case 'W':
       write_threads = atoi(optarg);
       break;
     case 'j':
       disjoint = 1;
       break;
     case '?':
       printf("Use -h or --help for help\n");
       exit(0);
     default:
       exit(1);
    }
  }

  assert(duration >= 0);
  assert(nb_accounts >= 2);
  assert(nb_threads > 0);
  assert(read_all >= 0 && write_all >= 0 && read_all + write_all <= 100);
  assert(read_threads + write_threads <= nb_threads);

  printf("Nb accounts    : %d\n", nb_accounts);
#ifndef TM_COMPILER
  printf("CM             : %s\n", (cm == NULL ? "DEFAULT" : cm));
#endif /* ! TM_COMPILER */
  printf("Duration       : %d\n", duration);
  printf("Nb threads     : %d\n", nb_threads);
  printf("Read-all rate  : %d\n", read_all);
  printf("Read threads   : %d\n", read_threads);
  printf("Seed           : %d\n", seed);
  printf("Write-all rate : %d\n", write_all);
  printf("Write threads  : %d\n", write_threads);
  printf("Type sizes     : int=%d/long=%d/ptr=%d/word=%d\n",
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

  bank = (bank_t *)malloc(sizeof(bank_t));
  bank->accounts = (account_t *)malloc(nb_accounts * sizeof(account_t));
  bank->size = nb_accounts;
  for (i = 0; i < bank->size; i++) {
    bank->accounts[i].number = i;
    bank->accounts[i].balance = 0;
  }

  stop = 0;

  /* Init STM */
  printf("Initializing STM\n");
  TM_INIT;

#ifndef TM_COMPILER
  if (stm_get_parameter("compile_flags", &s))
    printf("STM flags      : %s\n", s);

  if (cm != NULL) {
    if (stm_set_parameter("cm_policy", cm) == 0)
      printf("WARNING: cannot set contention manager \"%s\"\n", cm);
  }
#endif /* ! TM_COMPILER */

  /* Access set from all threads */
  barrier_init(&barrier, nb_threads + 1);
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  for (i = 0; i < nb_threads; i++) {
    printf("Creating thread %d\n", i);
    data[i].id = i;
    data[i].read_all = read_all;
    data[i].read_threads = read_threads;
    data[i].write_all = write_all;
    data[i].write_threads = write_threads;
    data[i].disjoint = disjoint;
    data[i].nb_threads = nb_threads;
    data[i].nb_transfer = 0;
    data[i].nb_read_all = 0;
    data[i].nb_write_all = 0;
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
    data[i].seed = rand();
    data[i].bank = bank;
    data[i].barrier = &barrier;
    if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) {
      fprintf(stderr, "Error creating thread\n");
      exit(1);
    }
  }
  pthread_attr_destroy(&attr);

  /* Catch some signals */
  if (signal(SIGHUP, catcher) == SIG_ERR ||
      signal(SIGINT, catcher) == SIG_ERR ||
      signal(SIGTERM, catcher) == SIG_ERR) {
    perror("signal");
    exit(1);
  }
 
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
  writes = 0;
  updates = 0;
  for (i = 0; i < nb_threads; i++) {
    printf("Thread %d\n", i);
    printf("  #transfer   : %lu\n", data[i].nb_transfer);
    printf("  #read-all   : %lu\n", data[i].nb_read_all);
    printf("  #write-all  : %lu\n", data[i].nb_write_all);
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
    updates += data[i].nb_transfer;
    reads += data[i].nb_read_all;
    writes += data[i].nb_write_all;
  }
  /* Sanity check */
  ret = total(bank, 0);
  printf("Bank total    : %d (expected: 0)\n", ret);
  printf("Duration      : %d (ms)\n", duration);
  printf("#txs          : %lu (%f / s)\n", reads + writes + updates, (reads + writes + updates) * 1000.0 / duration);
  printf("#read txs     : %lu (%f / s)\n", reads, reads * 1000.0 / duration);
  printf("#write txs    : %lu (%f / s)\n", writes, writes * 1000.0 / duration);
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

  /* Delete bank and accounts */
  free(bank->accounts);
  free(bank);

  /* Cleanup STM */
  TM_EXIT;

  free(threads);
  free(data);

  return ret;
}
