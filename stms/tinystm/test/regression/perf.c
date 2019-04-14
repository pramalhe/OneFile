/*
 * File:
 *   perf.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Performance regression test.
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

#include <stdlib.h>
#include <stdio.h>

#include "stm.h"
#include "mod_mem.h"

/* Increment the value of the global clock (used for timestamps).
 * Hidden to tinySTM users. */
void stm_inc_clock(void);

__attribute__((aligned(64)))
stm_word_t global_ctr[1000] = {0};

#define MEASURE_NB 1000

static inline uint64_t
rdtsc(void)
{
  uint32_t a, d;
  asm volatile( "rdtsc\n\t" : "=a" (a), "=d" (d));
  return (((uint64_t)d) << 32) | (((uint64_t)a) & 0xffffffff);
}

static int compar(const void *a, const void *b)
{
  return *((uint64_t *)a) - *((uint64_t *)b);
}

static void remove_cst_cost(uint64_t *m, size_t size, uint64_t cost)
{
  size_t i;
  for (i = 0; i < size; i++) {
    m[i] -= cost;
  }
}

static void stats(uint64_t *m, size_t size, uint64_t *min, double *avg, uint64_t *median)
{
  size_t i;
  /* Find median value */
  qsort(m, size, sizeof(uint64_t), compar);
  *median = m[(size/2)-1];
  /* Find minimal and calculate average */
  *min = ~0UL;
  *avg = 0.0;
  for (i = 0; i < size; i++) {
    *avg += m[i];
    if (m[i] < *min)
      *min = m[i];
  }
  *avg = *avg / size;
}

static void test1load(int ro)
{
  uint64_t m_s[MEASURE_NB];
  uint64_t m_r[MEASURE_NB];
  uint64_t m_c[MEASURE_NB];
  uint64_t m_rdtsc;
  uint64_t start;
  uint64_t min;
  double avg;
  uint64_t med;
  unsigned long i;
  stm_tx_attr_t _a = {{.read_only = ro}};

  m_rdtsc = ~0UL;
  for (i = 0; i < MEASURE_NB; i++) {
    start = rdtsc();
    start = rdtsc() - start;
    if (start < m_rdtsc)
      m_rdtsc = start;
  } 

  for (i = 0; i < MEASURE_NB; i++) {
    sigjmp_buf *_e;
    start = rdtsc();
    _e = stm_start(_a);
    m_s[i] = rdtsc() - start;
    sigsetjmp(*_e, 0); 
    stm_load(&global_ctr[0]);
    stm_inc_clock();
    stm_commit();
  }

  for (i = 0; i < MEASURE_NB; i++) {
    sigjmp_buf *_e = stm_start(_a);
    sigsetjmp(*_e, 0); 
    start = rdtsc();
    stm_load(&global_ctr[0]);
    m_r[i] = rdtsc() - start;
    stm_inc_clock();
    stm_commit();
  }
  
  for (i = 0; i < MEASURE_NB; i++) {
    sigjmp_buf *_e = stm_start(_a);
    sigsetjmp(*_e, 0); 
    stm_load(&global_ctr[0]);
    stm_inc_clock();
    start = rdtsc();
    stm_commit();
    m_c[i] = rdtsc() - start;
  }
 
  remove_cst_cost(m_s, MEASURE_NB, m_rdtsc);
  remove_cst_cost(m_r, MEASURE_NB, m_rdtsc);
  remove_cst_cost(m_c, MEASURE_NB, m_rdtsc);

  if (ro) 
    printf("RO transaction - 1 load\n");
  else
    printf("RW transaction - 1 load\n");

  printf("%12s %12s %12s %12s\n", "", "min", "avg", "med");
  stats(m_s, MEASURE_NB, &min, &avg, &med); 
  printf("%12s %12lu %12.2f %12lu\n", "start", (unsigned long)min, avg, (unsigned long)med);
  stats(m_r, MEASURE_NB, &min, &avg, &med); 
  printf("%12s %12lu %12.2f %12lu\n", "load", (unsigned long)min, avg, (unsigned long)med);
  stats(m_c, MEASURE_NB, &min, &avg, &med); 
  printf("%12s %12lu %12.2f %12lu\n", "commit", (unsigned long)min, avg, (unsigned long)med);
}

static void testnload(int ro, size_t load_nb)
{
  uint64_t m_s[MEASURE_NB];
  uint64_t m_r[MEASURE_NB];
  uint64_t m_c[MEASURE_NB];
  uint64_t m_rdtsc;
  uint64_t start;
  uint64_t min;
  double avg;
  uint64_t med;
  unsigned long i;
  size_t j;
  stm_tx_attr_t _a = {{.read_only = ro}};

  m_rdtsc = ~0UL;
  for (i = 0; i < MEASURE_NB; i++) {
    start = rdtsc();
    start = rdtsc() - start;
    if (start < m_rdtsc)
      m_rdtsc = start;
  } 

  for (i = 0; i < MEASURE_NB; i++) {
    sigjmp_buf *_e;
    start = rdtsc();
    _e = stm_start(_a);
    m_s[i] = rdtsc() - start;
    sigsetjmp(*_e, 0); 
    for (j = 0; j < load_nb; j++)
      stm_load(&global_ctr[j]);
    stm_inc_clock();
    stm_commit();
  }

  for (i = 0; i < MEASURE_NB; i++) {
    sigjmp_buf *_e = stm_start(_a);
    sigsetjmp(*_e, 0); 
    start = rdtsc();
    for (j = 0; j < load_nb; j++)
      stm_load(&global_ctr[j]);
    m_r[i] = rdtsc() - start;
    stm_inc_clock();
    stm_commit();
  }
  
  for (i = 0; i < MEASURE_NB; i++) {
    sigjmp_buf *_e = stm_start(_a);
    sigsetjmp(*_e, 0); 
    for (j = 0; j < load_nb; j++)
      stm_load(&global_ctr[j]);
    stm_inc_clock();
    start = rdtsc();
    stm_commit();
    m_c[i] = rdtsc() - start;
  }
 
  remove_cst_cost(m_s, MEASURE_NB, m_rdtsc);
  remove_cst_cost(m_r, MEASURE_NB, m_rdtsc);
  remove_cst_cost(m_c, MEASURE_NB, m_rdtsc);

  if (ro) 
    printf("RO transaction - %lu load\n", (unsigned long)load_nb);
  else
    printf("RW transaction - %lu load\n", (unsigned long)load_nb);

  printf("%12s %12s %12s %12s\n", "", "min", "avg", "med");
  stats(m_s, MEASURE_NB, &min, &avg, &med); 
  printf("%12s %12lu %12.2f %12lu\n", "start", (unsigned long)min, avg, (unsigned long)med);
  stats(m_r, MEASURE_NB, &min, &avg, &med); 
  if (load_nb)
    printf("%12s %12lu %12.2f %12lu\n", "load", (unsigned long)min/load_nb, avg/load_nb, (unsigned long)med/load_nb);
  stats(m_c, MEASURE_NB, &min, &avg, &med); 
  printf("%12s %12lu %12.2f %12lu\n", "commit", (unsigned long)min, avg, (unsigned long)med);
}

static void testnloadnstore(size_t load_nb, size_t store_nb)
{
  uint64_t m_s[MEASURE_NB];
  uint64_t m_r[MEASURE_NB];
  uint64_t m_w[MEASURE_NB];
  uint64_t m_c[MEASURE_NB];
  uint64_t m_rdtsc;
  uint64_t start;
  uint64_t min;
  double avg;
  uint64_t med;
  unsigned long i;
  size_t j;
  stm_tx_attr_t _a = {{.read_only = 0}};

  m_rdtsc = ~0UL;
  for (i = 0; i < MEASURE_NB; i++) {
    start = rdtsc();
    start = rdtsc() - start;
    if (start < m_rdtsc)
      m_rdtsc = start;
  } 

  for (i = 0; i < MEASURE_NB; i++) {
    sigjmp_buf *_e;
    start = rdtsc();
    _e = stm_start(_a);
    m_s[i] = rdtsc() - start;
    sigsetjmp(*_e, 0); 
    for (j = 0; j < load_nb; j++)
      stm_load(&global_ctr[j]);
    for (j = 0; j < store_nb; j++)
      stm_store(&global_ctr[j], (stm_word_t)0);
    stm_inc_clock();
    stm_commit();
  }

  for (i = 0; i < MEASURE_NB; i++) {
    sigjmp_buf *_e = stm_start(_a);
    sigsetjmp(*_e, 0); 
    start = rdtsc();
    for (j = 0; j < load_nb; j++)
      stm_load(&global_ctr[j]);
    m_r[i] = rdtsc() - start;
    for (j = 0; j < store_nb; j++)
      stm_store(&global_ctr[j], (stm_word_t)0);
    stm_inc_clock();
    stm_commit();
  }
  
  for (i = 0; i < MEASURE_NB; i++) {
    sigjmp_buf *_e = stm_start(_a);
    sigsetjmp(*_e, 0); 
    for (j = 0; j < load_nb; j++)
      stm_load(&global_ctr[j]);
    start = rdtsc();
    for (j = 0; j < store_nb; j++)
      stm_store(&global_ctr[j], (stm_word_t)0);
    m_w[i] = rdtsc() - start;
    stm_inc_clock();
    stm_commit();
  }
  
  for (i = 0; i < MEASURE_NB; i++) {
    sigjmp_buf *_e = stm_start(_a);
    sigsetjmp(*_e, 0); 
    for (j = 0; j < load_nb; j++)
      stm_load(&global_ctr[j]);
    for (j = 0; j < store_nb; j++)
      stm_store(&global_ctr[j], (stm_word_t)0);
    stm_inc_clock();
    start = rdtsc();
    stm_commit();
    m_c[i] = rdtsc() - start;
  }
 
  remove_cst_cost(m_s, MEASURE_NB, m_rdtsc);
  remove_cst_cost(m_r, MEASURE_NB, m_rdtsc);
  remove_cst_cost(m_w, MEASURE_NB, m_rdtsc);
  remove_cst_cost(m_c, MEASURE_NB, m_rdtsc);

  printf("RW transaction - %lu load - %lu store\n", (unsigned long)load_nb, (unsigned long)store_nb);

  printf("%12s %12s %12s %12s\n", "", "min", "avg", "med");
  stats(m_s, MEASURE_NB, &min, &avg, &med); 
  printf("%12s %12lu %12.2f %12lu\n", "start", (unsigned long)min, avg, (unsigned long)med);
  stats(m_r, MEASURE_NB, &min, &avg, &med); 
  if (load_nb)
    printf("%12s %12lu %12.2f %12lu\n", "load", (unsigned long)min/load_nb, avg/load_nb, (unsigned long)med/load_nb);
  stats(m_w, MEASURE_NB, &min, &avg, &med); 
  if (store_nb)
    printf("%12s %12lu %12.2f %12lu\n", "store", (unsigned long)min/store_nb, avg/store_nb, (unsigned long)med/store_nb);
  stats(m_c, MEASURE_NB, &min, &avg, &med); 
  printf("%12s %12lu %12.2f %12lu\n", "commit", (unsigned long)min, avg, (unsigned long)med);
}

/* TODO
 *  Add clock perturbation to avoid fast commit
 *  Add write after write / load after write measurements
 *  Add stm_malloc/stm_free measurements
 */

int main(int argc, char **argv)
{
  /* Init STM */
  stm_init();
  mod_mem_init(0);
  /* Create transaction */
  stm_init_thread();

  /* Testing */
  test1load(1);
  test1load(0);
  testnload(1, 100);
  testnload(0, 100);
  testnloadnstore(100, 20);
  testnloadnstore(100, 20);

  /* Free transaction */
  stm_exit_thread();
  /* Cleanup STM */
  stm_exit();
  return 0;
}
