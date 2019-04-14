/*
 * File:
 *   irrevocability.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Regression test for irrevocability.
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

#ifdef NDEBUG
# undef NDEBUG
#endif

#include <assert.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "stm.h"
#include "wrappers.h"

#define DEFAULT_DURATION                5000
#define DEFAULT_IRREVOCABLE_PERCENT     25
#define DEFAULT_NB_THREADS              4

#define NB_ELEMENTS                     64
#define NB_SHUFFLES                     16

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

static volatile int stop;

long data[64];

volatile long nb_irrevocable_serial = 0;
volatile long nb_irrevocable_parallel = 0;

typedef struct thread_data {
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
  unsigned short seed[3];
  int irrevocable_percent;
  char padding[64];
} thread_data_t;

static void *test(void *v)
{
  unsigned int seed;
  int i, n, irrevocable, serial, path;
  long l;
  sigjmp_buf *e;
  thread_data_t *d = (thread_data_t *)v;

  seed = (unsigned int)time(NULL);
  stm_init_thread();
  while (stop == 0) {
    irrevocable = (rand_r(&seed) < RAND_MAX / 100 * d->irrevocable_percent ? 1 : 0);
    serial = (rand_r(&seed) < RAND_MAX / 2 ? 1 : 0);
//    irrevocable = 1;
//    serial = 1;
    e = stm_start((stm_tx_attr_t)0);
    path = sigsetjmp(*e, 0);
    if (irrevocable == 4) {
      /* Aborted while in irrevocable mode => error */
      fprintf(stderr, "ERROR: aborted while in irrevocable mode\n");
      exit(1);
    }
    if (path & STM_PATH_UNINSTRUMENTED) {
      for (n = rand_r(&seed) % NB_SHUFFLES; n > 0; n--) {
        i = rand_r(&seed) % NB_ELEMENTS;
        data[i] = data[i] + 1;
        i = rand_r(&seed) % NB_ELEMENTS;
        data[i] = data[i] - 1;
      }
    } else {
      for (n = rand_r(&seed) % NB_SHUFFLES; n > 0; n--) {
        i = rand_r(&seed) % NB_ELEMENTS;
        stm_store_long(&data[i], stm_load_long(&data[i]) + 1);
        i = rand_r(&seed) % NB_ELEMENTS;
        stm_store_long(&data[i], stm_load_long(&data[i]) - 1);
      }
    }
    if (irrevocable) {
      if (irrevocable == 3) {
        /* Already tried entering irrevocable mode once => error */
        fprintf(stderr, "ERROR: failed entering irrevocable mode upon retry\n");
        exit(1);
      }
      irrevocable++;
      if (!stm_set_irrevocable(serial)) {
        fprintf(stderr, "ERROR: cannot enter irrevocable mode\n");
        exit(1);
      }
      irrevocable = 4;
      /* Once in irrevocable mode, we cannot abort */
      if (path & STM_PATH_UNINSTRUMENTED) {
        /* No other transaction can execute concurrently */
        for (i = 0, l = 0; i < NB_ELEMENTS; i++)
          l += data[i];
        assert(l == 0);
        for (i = 0; i < NB_ELEMENTS; i++)
          data[i] = 0;
        nb_irrevocable_serial++;
      } else {
        /* Non-conflicting transactions can execute concurrently */
        for (i = 0, l = 0; i < NB_ELEMENTS; i++)
          l += stm_load_long(&data[i]);
        assert(l == 0);
        for (i = 0; i < NB_ELEMENTS; i++)
          stm_store_long(&data[i], 0);
        nb_irrevocable_parallel++;
      }
    }
    if (path & STM_PATH_UNINSTRUMENTED) {
      for (i = 0, l = 0; i < NB_ELEMENTS; i++)
        l += data[i];
    } else {
      for (i = 0, l = 0; i < NB_ELEMENTS; i++)
        l += stm_load_long(&data[i]);
    }
    assert(l == 0);
    stm_commit();
  }

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

  stm_exit_thread();

  return NULL;
}

int main(int argc, char **argv)
{
  struct option long_options[] = {
    // These options don't set a flag
    {"help",                      no_argument,       NULL, 'h'},
    {"contention-manager",        required_argument, NULL, 'c'},
    {"duration",                  required_argument, NULL, 'd'},
    {"irrevocable-percent",       required_argument, NULL, 'i'},
    {"num-threads",               required_argument, NULL, 'n'},
    {NULL, 0, NULL, 0}
  };

  int i, c;
  unsigned long aborts, aborts_1, aborts_2,
    aborts_locked_read, aborts_locked_write,
    aborts_validate_read, aborts_validate_write, aborts_validate_commit,
    aborts_invalid_memory, aborts_killed,
    locked_reads_ok, locked_reads_failed, max_retries;
  thread_data_t *td;
  pthread_t *threads;
  pthread_attr_t attr;
  struct timespec timeout;
  int duration = DEFAULT_DURATION;
  int irrevocable_percent = DEFAULT_IRREVOCABLE_PERCENT;
  int nb_threads = DEFAULT_NB_THREADS;
  char *cm = NULL;

  while(1) {
    i = 0;
    c = getopt_long(argc, argv, "hc:d:i:n:", long_options, &i);

    if(c == -1)
      break;

    if(c == 0 && long_options[i].flag == 0)
      c = long_options[i].val;

    switch(c) {
     case 0:
       /* Flag is automatically set */
       break;
     case 'h':
       printf("irrevocability -- STM stress test "
              "\n"
              "Usage:\n"
              "  irrevocability [options...]\n"
              "\n"
              "Options:\n"
              "  -h, --help\n"
              "        Print this message\n"
              "  -c, --contention-manager <string>\n"
              "        Contention manager for resolving conflicts (default=suicide)\n"
              "  -d, --duration <int>\n"
              "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
              "  -i, --irrevocable-percent <int>\n"
              "         (default=" XSTR(DEFAULT_IRREVOCABLE_PERCENT) ")\n"
              "  -n, --num-threads <int>\n"
              "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
         );
       exit(0);
     case 'c':
       cm = optarg;
       break;
     case 'd':
       duration = atoi(optarg);
       break;
     case 'n':
       nb_threads = atoi(optarg);
       break;
     case 'i':
       irrevocable_percent = atoi(optarg);
       break;
     case '?':
       printf("Use -h or --help for help\n");
       exit(0);
     default:
       exit(1);
    }
  }

  assert(duration >= 0);
  assert(nb_threads > 0);
  assert(irrevocable_percent >= 0 && irrevocable_percent <= 100);

  printf("CM           : %s\n", (cm == NULL ? "DEFAULT" : cm));
  printf("Duration     : %d\n", duration);
  printf("Irrevocable  : %d%%\n", irrevocable_percent);
  printf("Nb threads   : %d\n", nb_threads);

  for (i = 0; i < NB_ELEMENTS; i++)
    data[i] = 0;

  /* Init STM */
  printf("Initializing STM\n");
  stm_init();

  /* Set contention manager */
  if (cm != NULL) {
    if (stm_set_parameter("cm_policy", cm) == 0)
      printf("WARNING: cannot set contention manager \"%s\"\n", cm);
  }

  printf("int/long/ptr/word size: %d/%d/%d/%d\n",
         (int)sizeof(int),
         (int)sizeof(long),
         (int)sizeof(void *),
         (int)sizeof(stm_word_t));

  stop = 0;

  printf("TESTING CONCURRENT UPDATES...\n");

  timeout.tv_sec = duration / 1000;
  timeout.tv_nsec = (duration % 1000) * 1000000;

  if ((td = (thread_data_t *)malloc(nb_threads * sizeof(thread_data_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  if ((threads = (pthread_t *)malloc(nb_threads * sizeof(pthread_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  for (i = 0; i < nb_threads; i++) {
    td[i].nb_aborts = 0;
    td[i].nb_aborts_1 = 0;
    td[i].nb_aborts_2 = 0;
    td[i].nb_aborts_locked_read = 0;
    td[i].nb_aborts_locked_write = 0;
    td[i].nb_aborts_validate_read = 0;
    td[i].nb_aborts_validate_write = 0;
    td[i].nb_aborts_validate_commit = 0;
    td[i].nb_aborts_invalid_memory = 0;
    td[i].nb_aborts_killed = 0;
    td[i].locked_reads_ok = 0;
    td[i].locked_reads_failed = 0;
    td[i].max_retries = 0;
    td[i].irrevocable_percent = irrevocable_percent;
    if (pthread_create(&threads[i], &attr, test, (void *)(&td[i])) != 0) {
      fprintf(stderr, "Error creating thread\n");
      exit(1);
    }
  }
  pthread_attr_destroy(&attr);
  nanosleep(&timeout, NULL);
  printf("STOPPING...\n");
  stop = 1;
  for (i = 0; i < nb_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      fprintf(stderr, "Error waiting for thread completion\n");
      exit(1);
    }
  }

  printf("PASSED\n");
  printf("Number of successful irrevocable-serial executions   : %ld\n", nb_irrevocable_serial);
  printf("Number of successful irrevocable-parallel executions : %ld\n", nb_irrevocable_parallel);

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
  for (i = 0; i < nb_threads; i++) {
    printf("Thread %d\n", i);
    printf("  #aborts     : %lu\n", td[i].nb_aborts);
    printf("    #lock-r   : %lu\n", td[i].nb_aborts_locked_read);
    printf("    #lock-w   : %lu\n", td[i].nb_aborts_locked_write);
    printf("    #val-r    : %lu\n", td[i].nb_aborts_validate_read);
    printf("    #val-w    : %lu\n", td[i].nb_aborts_validate_write);
    printf("    #val-c    : %lu\n", td[i].nb_aborts_validate_commit);
    printf("    #inv-mem  : %lu\n", td[i].nb_aborts_invalid_memory);
    printf("    #killed   : %lu\n", td[i].nb_aborts_killed);
    printf("  #aborts>=1  : %lu\n", td[i].nb_aborts_1);
    printf("  #aborts>=2  : %lu\n", td[i].nb_aborts_2);
    printf("  #lr-ok      : %lu\n", td[i].locked_reads_ok);
    printf("  #lr-failed  : %lu\n", td[i].locked_reads_failed);
    printf("  Max retries : %lu\n", td[i].max_retries);
    aborts += td[i].nb_aborts;
    aborts_1 += td[i].nb_aborts_1;
    aborts_2 += td[i].nb_aborts_2;
    aborts_locked_read += td[i].nb_aborts_locked_read;
    aborts_locked_write += td[i].nb_aborts_locked_write;
    aborts_validate_read += td[i].nb_aborts_validate_read;
    aborts_validate_write += td[i].nb_aborts_validate_write;
    aborts_validate_commit += td[i].nb_aborts_validate_commit;
    aborts_invalid_memory += td[i].nb_aborts_invalid_memory;
    aborts_killed += td[i].nb_aborts_killed;
    locked_reads_ok += td[i].locked_reads_ok;
    locked_reads_failed += td[i].locked_reads_failed;
    if (max_retries < td[i].max_retries)
      max_retries = td[i].max_retries;
  }
  printf("Duration      : %d (ms)\n", duration);
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

  /* Cleanup STM */
  stm_exit();

  return 0;
}
