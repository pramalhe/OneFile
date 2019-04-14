/*
 * File:
 *   types.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Regression test for various data types.
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
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "stm.h"
#include "wrappers.h"

union {
  uint8_t u8[256];
  uint16_t u16[128];
  uint32_t u32[64];
  uint64_t u64[32];
  int8_t s8[256];
  int16_t s16[128];
  int32_t s32[64];
  int64_t s64[32];
  float f[64];
  double d[32];
} tab, tab_ro;

typedef union {
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
  uint64_t u64;
  int8_t s8;
  int16_t s16;
  int32_t s32;
  int64_t s64;
  float f;
  double d;
  void *p;
} val_t;

enum {
  TYPE_UINT8,
  TYPE_UINT16,
  TYPE_UINT32,
  TYPE_UINT64,
  TYPE_CHAR,
  TYPE_UCHAR,
  TYPE_SHORT,
  TYPE_USHORT,
  TYPE_INT,
  TYPE_UINT,
  TYPE_LONG,
  TYPE_ULONG,
  TYPE_FLOAT,
  TYPE_DOUBLE,
  TYPE_BYTES
};

#define NB_THREADS                      4
#define DURATION                        5000

volatile int verbose;
volatile int stop;

static void compare(int idx, val_t val, int type, int size)
{
  int i;
  val_t v;

  switch(type) {
   case TYPE_UINT8:
     for (i = 0; i < 256 / sizeof(uint8_t); i++) {
       v.u8 = stm_load_u8(&tab.u8[i]);
       assert(i == idx ? v.u8 == val.u8 : v.u8 == tab_ro.u8[i]);
     }
     break;
   case TYPE_UINT16:
     for (i = 0; i < 256 / sizeof(uint16_t); i++) {
       v.u16 = stm_load_u16(&tab.u16[i]);
       assert(i == idx ? v.u16 == val.u16 : v.u16 == tab_ro.u16[i]);
     }
     break;
   case TYPE_UINT32:
     for (i = 0; i < 256 / sizeof(uint32_t); i++) {
       v.u32 = stm_load_u32(&tab.u32[i]);
       assert(i == idx ? v.u32 == val.u32 : v.u32 == tab_ro.u32[i]);
     }
     break;
   case TYPE_UINT64:
     for (i = 0; i < 256 / sizeof(uint64_t); i++) {
       v.u64 = stm_load_u64(&tab.u64[i]);
       assert(i == idx ? v.u64 == val.u64 : v.u64 == tab_ro.u64[i]);
     }
     break;
   case TYPE_CHAR:
     for (i = 0; i < 256 / sizeof(unsigned char); i++) {
       v.s8 = (int8_t)stm_load_char((char *)&tab.s8[i]);
       assert(i == idx ? v.s8 == val.s8 : v.s8 == tab_ro.s8[i]);
     }
     break;
   case TYPE_UCHAR:
     for (i = 0; i < 256 / sizeof(char); i++) {
       v.u8 = (uint8_t)stm_load_uchar((unsigned char *)&tab.u8[i]);
       assert(i == idx ? v.u8 == val.u8 : v.u8 == tab_ro.u8[i]);
     }
     break;
   case TYPE_SHORT:
     for (i = 0; i < 256 / sizeof(short); i++) {
       v.s16 = (int16_t)stm_load_short((short *)&tab.s16[i]);
       assert(i == idx ? v.s16 == val.s16 : v.s16 == tab_ro.s16[i]);
     }
     break;
   case TYPE_USHORT:
     for (i = 0; i < 256 / sizeof(unsigned short); i++) {
       v.u16 = (uint16_t)stm_load_ushort((unsigned short *)&tab.u16[i]);
       assert(i == idx ? v.u16 == val.u16 : v.u16 == tab_ro.u16[i]);
     }
     break;
   case TYPE_INT:
     for (i = 0; i < 256 / sizeof(int); i++) {
       v.s32 = (int32_t)stm_load_int((int *)&tab.s32[i]);
       assert(i == idx ? v.s32 == val.s32 : v.s32 == tab_ro.s32[i]);
     }
     break;
   case TYPE_UINT:
     for (i = 0; i < 256 / sizeof(unsigned int); i++) {
       v.u32 = (uint32_t)stm_load_uint((unsigned int *)&tab.u32[i]);
       assert(i == idx ? v.u32 == val.u32 : v.u32 == tab_ro.u32[i]);
     }
     break;
   case TYPE_LONG:
     for (i = 0; i < 256 / sizeof(long); i++) {
       if (sizeof(long) == 4) {
         v.s32 = (int32_t)stm_load_long((long *)&tab.s32[i]);
         assert(i == idx ? v.s32 == val.s32 : v.s32 == tab_ro.s32[i]);
       } else {
         v.s64 = (int64_t)stm_load_long((long *)&tab.s64[i]);
         assert(i == idx ? v.s64 == val.s64 : v.s64 == tab_ro.s64[i]);
       }
     }
     break;
   case TYPE_ULONG:
     for (i = 0; i < 256 / sizeof(unsigned long); i++) {
       if (sizeof(long) == 4) {
         v.u32 = (uint32_t)stm_load_ulong((unsigned long *)&tab.u32[i]);
         assert(i == idx ? v.u32 == val.u32 : v.u32 == tab_ro.u32[i]);
       } else {
         v.u64 = (uint64_t)stm_load_ulong((unsigned long *)&tab.u64[i]);
         assert(i == idx ? v.u64 == val.u64 : v.u64 == tab_ro.u64[i]);
       }
     }
     break;
   case TYPE_FLOAT:
     for (i = 0; i < 256 / sizeof(float); i++) {
       v.f = stm_load_float(&tab.f[i]);
       assert(i == idx ? (isnan(v.f) && isnan(val.f)) || v.f == val.f : (isnan(v.f) && isnan(tab_ro.f[i])) || v.f == tab_ro.f[i]);
     }
     break;
   case TYPE_DOUBLE:
     for (i = 0; i < 256 / sizeof(double); i++) {
       v.d = stm_load_double(&tab.d[i]);
       assert(i == idx ? (isnan(v.d) && isnan(val.d)) || v.d == val.d : (isnan(v.d) && isnan(tab_ro.d[i])) || v.d == tab_ro.d[i]);
     }
     break;
   case TYPE_BYTES:
     for (i = 0; i < 256 / sizeof(uint8_t); i++) {
       v.u8 = stm_load_u8(&tab.u8[i]);
       assert(i >= idx && i < idx + size ? v.u8 == ((uint8_t *)val.p)[i - idx] : v.u8 == tab_ro.u8[i]);
     }
     break;
  }
}

static void test_loads()
{
  int i, j;
  val_t val;
  sigjmp_buf *e;

  e = stm_start((stm_tx_attr_t)0);
  if (e != NULL)
    sigsetjmp(*e, 0);

  if (verbose)
    printf("- Testing uint8_t\n");
  for (i = 0; i < 256 / sizeof(uint8_t); i++) {
    val.u8 = stm_load_u8(&tab.u8[i]);
    assert(val.u8 == tab_ro.u8[i]);
  }
  if (verbose)
    printf("- Testing uint16_t\n");
  for (i = 0; i < 256 / sizeof(uint16_t); i++) {
    val.u16 = stm_load_u16(&tab.u16[i]);
    assert(val.u16 == tab_ro.u16[i]);
  }
  if (verbose)
    printf("- Testing misaligned uint16_t\n");
  for (i = 1; i < 256 - sizeof(uint16_t); i += sizeof(uint16_t)) {
    val.u16 = stm_load_u16((uint16_t *)&tab.u8[i]);
    assert(val.u16 == *(uint16_t *)&tab_ro.u8[i]);
  }
  if (verbose)
    printf("- Testing uint32_t\n");
  for (i = 0; i < 256 / sizeof(uint32_t); i++) {
    val.u32 = stm_load_u32(&tab.u32[i]);
    assert(val.u32 == tab_ro.u32[i]);
  }
  if (verbose)
    printf("- Testing misaligned uint32_t\n");
  for (j = 1; j < sizeof(uint32_t); j++) {
    for (i = j; i < 256 - sizeof(uint32_t); i += sizeof(uint32_t)) {
      val.u32 = stm_load_u32((uint32_t *)&tab.u8[i]);
      assert(val.u32 == *(uint32_t *)&tab_ro.u8[i]);
    }
  }
  if (verbose)
    printf("- Testing uint64_t\n");
  for (i = 0; i < 256 / sizeof(uint64_t); i++) {
    val.u64 = stm_load_u64(&tab.u64[i]);
    assert(val.u64 == tab_ro.u64[i]);
  }
  if (verbose)
    printf("- Testing misaligned uint64_t\n");
  for (j = 1; j < sizeof(uint64_t); j++) {
    for (i = j; i < 256 - sizeof(uint64_t); i += sizeof(uint64_t)) {
      val.u64 = stm_load_u64((uint64_t *)&tab.u8[i]);
      assert(val.u64 == *(uint64_t *)&tab_ro.u8[i]);
    }
  }
  if (verbose)
    printf("- Testing char\n");
  for (i = 0; i < 256 / sizeof(char); i++) {
    val.s8 = (int8_t)stm_load_char((volatile char *)&tab.s8[i]);
    assert(val.s8 == tab_ro.s8[i]);
  }
  if (verbose)
    printf("- Testing unsigned char\n");
  for (i = 0; i < 256 / sizeof(unsigned char); i++) {
    val.u8 = (uint8_t)stm_load_uchar((volatile unsigned char *)&tab.u8[i]);
    assert(val.u8 == tab_ro.u8[i]);
  }
  if (verbose)
    printf("- Testing short\n");
  for (i = 0; i < 256 / sizeof(short); i++) {
    val.s16 = (int16_t)stm_load_short((volatile short *)&tab.s16[i]);
    assert(val.s16 == tab_ro.s16[i]);
  }
  if (verbose)
    printf("- Testing unsigned short\n");
  for (i = 0; i < 256 / sizeof(unsigned short); i++) {
    val.u16 = (uint16_t)stm_load_ushort((volatile unsigned short *)&tab.u16[i]);
    assert(val.u16 == tab_ro.u16[i]);
  }
  if (verbose)
    printf("- Testing int\n");
  for (i = 0; i < 256 / sizeof(int); i++) {
    val.s32 = (int32_t)stm_load_int((volatile int *)&tab.s32[i]);
    assert(val.s32 == tab_ro.s32[i]);
  }
  if (verbose)
    printf("- Testing unsigned int\n");
  for (i = 0; i < 256 / sizeof(unsigned int); i++) {
    val.u32 = (uint32_t)stm_load_uint((volatile unsigned int *)&tab.u32[i]);
    assert(val.u32 == tab_ro.u32[i]);
  }
  if (verbose)
    printf("- Testing long\n");
  for (i = 0; i < 256 / sizeof(long); i++) {
    if (sizeof(long) == 4) {
      val.s32 = (int32_t)stm_load_long((volatile long *)&tab.s32[i]);
      assert(val.s32 == tab_ro.s32[i]);
    } else {
      val.s64 = (int64_t)stm_load_long((volatile long *)&tab.s64[i]);
      assert(val.s64 == tab_ro.s64[i]);
    }
  }
  if (verbose)
    printf("- Testing unsigned long\n");
  for (i = 0; i < 256 / sizeof(unsigned long); i++) {
    if (sizeof(long) == 4) {
      val.u32 = (uint32_t)stm_load_ulong((volatile unsigned long *)&tab.u32[i]);
      assert(val.u32 == tab_ro.u32[i]);
    } else {
      val.u64 = (uint64_t)stm_load_ulong((volatile unsigned long *)&tab.u64[i]);
      assert(val.u64 == tab_ro.u64[i]);
    }
  }
  if (verbose)
    printf("- Testing float\n");
  for (i = 0; i < 256 / sizeof(float); i++) {
    val.f = stm_load_float(&tab.f[i]);
    assert((isnan(val.f) && isnan(tab_ro.f[i])) || val.f == tab_ro.f[i]);
  }
  if (verbose)
    printf("- Testing double\n");
  for (i = 0; i < 256 / sizeof(double); i++) {
    val.d = stm_load_double(&tab.d[i]);
    assert((isnan(val.d) && isnan(tab_ro.d[i])) || val.d == tab_ro.d[i]);
  }

  stm_commit();
}

static void test_stores()
{
  int i, j;
  val_t val, bytes;
  sigjmp_buf *e;

  e = stm_start((stm_tx_attr_t)0);
  if (e != NULL)
    sigsetjmp(*e, 0);

  if (verbose)
    printf("- Testing uint8_t\n");
  for (i = 0; i < 256; i++) {
    val.u8 = ~tab_ro.u8[i];
    stm_store_u8(&tab.u8[i], val.u8);
    compare(i, val, TYPE_UINT8, 0);
    stm_store_u8(&tab.u8[i], tab_ro.u8[i]);
  }
  if (verbose)
    printf("- Testing uint16_t\n");
  for (i = 0; i < 256 / sizeof(uint16_t); i++) {
    val.u16 = ~tab_ro.u16[i];
    stm_store_u16(&tab.u16[i], val.u16);
    compare(i, val, TYPE_UINT16, 0);
    stm_store_u16(&tab.u16[i], tab_ro.u16[i]);
  }
  if (verbose)
    printf("- Testing misaligned uint16_t\n");
  for (i = 1; i < 256 - sizeof(uint16_t); i += sizeof(uint16_t)) {
    val.u16 = ~*(uint16_t *)&tab_ro.u8[i];
    stm_store_u16((uint16_t *)&tab.u8[i], val.u16);
    bytes.p = &val.u16;
    compare(i, bytes, TYPE_BYTES, sizeof(uint16_t));
    stm_store_u16((uint16_t *)&tab.u8[i], *(uint16_t *)&tab_ro.u8[i]);
  }
  if (verbose)
    printf("- Testing uint32_t\n");
  for (i = 0; i < 256 / sizeof(uint32_t); i++) {
    val.u32 = ~tab_ro.u32[i];
    stm_store_u32(&tab.u32[i], val.u32);
    compare(i, val, TYPE_UINT32, 0);
    stm_store_u32(&tab.u32[i], tab_ro.u32[i]);
  }
  if (verbose)
    printf("- Testing misaligned uint32_t\n");
  for (j = 1; j < sizeof(uint32_t); j++) {
    for (i = j; i < 256 - sizeof(uint32_t); i += sizeof(uint32_t)) {
      val.u32 = ~*(uint32_t *)&tab_ro.u8[i];
      stm_store_u32((uint32_t *)&tab.u8[i], val.u32);
      bytes.p = &val.u32;
      compare(i, bytes, TYPE_BYTES, sizeof(uint32_t));
      stm_store_u32((uint32_t *)&tab.u8[i], *(uint32_t *)&tab_ro.u8[i]);
    }
  }
  if (verbose)
    printf("- Testing uint64_t\n");
  for (i = 0; i < 256 / sizeof(uint64_t); i++) {
    val.u64 = ~tab_ro.u64[i];
    stm_store_u64(&tab.u64[i], val.u64);
    compare(i, val, TYPE_UINT64, 0);
    stm_store_u64(&tab.u64[i], tab_ro.u64[i]);
  }
  if (verbose)
    printf("- Testing misaligned uint64_t\n");
  for (j = 1; j < sizeof(uint64_t); j++) {
    for (i = j; i < 256 - sizeof(uint32_t); i += sizeof(uint32_t)) {
      val.u32 = ~*(uint32_t *)&tab_ro.u8[i];
      stm_store_u32((uint32_t *)&tab.u8[i], val.u32);
      bytes.p = &val.u32;
      compare(i, bytes, TYPE_BYTES, sizeof(uint32_t));
      stm_store_u32((uint32_t *)&tab.u8[i], *(uint32_t *)&tab_ro.u8[i]);
    }
  }
  if (verbose)
    printf("- Testing char\n");
  for (i = 0; i < 256 / sizeof(char); i++) {
    val.s8 = ~tab_ro.s8[i];
    stm_store_char((volatile char *)&tab.s8[i], (char)val.s8);
    compare(i, val, TYPE_CHAR, 0);
    stm_store_char((volatile char *)&tab.s8[i], (char)tab_ro.s8[i]);
  }
  if (verbose)
    printf("- Testing unsigned char\n");
  for (i = 0; i < 256 / sizeof(unsigned char); i++) {
    val.u8 = ~tab_ro.u8[i];
    stm_store_uchar((volatile unsigned char *)&tab.u8[i], (unsigned char)val.u8);
    compare(i, val, TYPE_UCHAR, 0);
    stm_store_uchar((volatile unsigned char *)&tab.u8[i], (unsigned char)tab_ro.u8[i]);
  }
  if (verbose)
    printf("- Testing short\n");
  for (i = 0; i < 256 / sizeof(short); i++) {
    val.s16 = ~tab_ro.s16[i];
    stm_store_short((volatile short *)&tab.s16[i], (short)val.s16);
    compare(i, val, TYPE_SHORT, 0);
    stm_store_short((volatile short *)&tab.s16[i], (short)tab_ro.s16[i]);
  }
  if (verbose)
    printf("- Testing unsigned short\n");
  for (i = 0; i < 256 / sizeof(unsigned short); i++) {
    val.u16 = ~tab_ro.u16[i];
    stm_store_ushort((volatile unsigned short *)&tab.u16[i], (unsigned short)val.u16);
    compare(i, val, TYPE_USHORT, 0);
    stm_store_ushort((volatile unsigned short *)&tab.u16[i], (unsigned short)tab_ro.u16[i]);
  }
  if (verbose)
    printf("- Testing int\n");
  for (i = 0; i < 256 / sizeof(int); i++) {
    val.s32 = ~tab_ro.s32[i];
    stm_store_int((volatile int *)&tab.s32[i], (int)val.s32);
    compare(i, val, TYPE_INT, 0);
    stm_store_int((volatile int *)&tab.s32[i], (int)tab_ro.s32[i]);
  }
  if (verbose)
    printf("- Testing unsigned int\n");
  for (i = 0; i < 256 / sizeof(unsigned int); i++) {
    val.u32 = ~tab_ro.u32[i];
    stm_store_uint((volatile unsigned int *)&tab.u32[i], (unsigned int)val.u32);
    compare(i, val, TYPE_UINT, 0);
    stm_store_uint((volatile unsigned int *)&tab.u32[i], (unsigned int)tab_ro.u32[i]);
  }
  if (verbose)
    printf("- Testing long\n");
  for (i = 0; i < 256 / sizeof(long); i++) {
    if (sizeof(long) == 4) {
      val.s32 = ~tab_ro.s32[i];
      stm_store_long((volatile long *)&tab.s32[i], (long)val.s32);
      compare(i, val, TYPE_LONG, 0);
      stm_store_long((volatile long *)&tab.s32[i], (long)tab_ro.s32[i]);
    } else {
      val.s64 = ~tab_ro.s64[i];
      stm_store_long((volatile long *)&tab.s64[i], (long)val.s64);
      compare(i, val, TYPE_LONG, 0);
      stm_store_long((volatile long *)&tab.s64[i], (long)tab_ro.s64[i]);
    }
  }
  if (verbose)
    printf("- Testing unsigned long\n");
  for (i = 0; i < 256 / sizeof(unsigned long); i++) {
    if (sizeof(long) == 4) {
      val.u32 = ~tab_ro.u32[i];
      stm_store_ulong((volatile unsigned long *)&tab.u32[i], (unsigned long)val.u32);
      compare(i, val, TYPE_ULONG, 0);
      stm_store_ulong((volatile unsigned long *)&tab.u32[i], (unsigned long)tab_ro.u32[i]);
    } else {
      val.s64 = ~tab_ro.s64[i];
      stm_store_long((volatile long *)&tab.s64[i], (long)val.s64);
      compare(i, val, TYPE_LONG, 0);
      stm_store_long((volatile long *)&tab.s64[i], (long)tab_ro.s64[i]);
    }
  }
  if (verbose)
    printf("- Testing float\n");
  for (i = 0; i < 256 / sizeof(float); i++) {
    val.u32 = ~tab_ro.u32[i];
    stm_store_float(&tab.f[i], val.f);
    compare(i, val, TYPE_FLOAT, 0);
    stm_store_float(&tab.f[i], tab_ro.f[i]);
  }
  if (verbose)
    printf("- Testing double\n");
  for (i = 0; i < 256 / sizeof(double); i++) {
    val.u64 = ~tab_ro.u64[i];
    stm_store_double(&tab.d[i], val.d);
    compare(i, val, TYPE_DOUBLE, 0);
    stm_store_double(&tab.d[i], tab_ro.d[i]);
  }

  stm_commit();
}

static void *test(void *v)
{
  unsigned int seed;
  int nested, store;
  sigjmp_buf *e;

  seed = (unsigned int)time(NULL);
  stm_init_thread();
  while (stop == 0) {
    nested = (rand_r(&seed) < RAND_MAX / 3);
    store = (rand_r(&seed) < RAND_MAX / 3);
    if (nested) {
      e = stm_start((stm_tx_attr_t)0);
      if (e != NULL)
        sigsetjmp(*e, 0);
    }
    if (store)
      test_stores();
    else
      test_loads();
    if (nested) {
      stm_commit();
    }
  }
  stm_exit_thread();

  return NULL;
}

int main(int argc, char **argv)
{
  int i;
  pthread_t *threads;
  pthread_attr_t attr;
  struct timespec timeout;

  for (i = 0; i < 256; i++)
    tab_ro.u8[i] = tab.u8[i] = i;

  /* Init STM */
  printf("Initializing STM\n");
  stm_init();

  printf("int/long/ptr/word size: %d/%d/%d/%d\n",
         (int)sizeof(int),
         (int)sizeof(long),
         (int)sizeof(void *),
         (int)sizeof(stm_word_t));

  verbose = 1;
  stop = 0;

  stm_init_thread();

  printf("TESTING LOADS...\n");
  test_loads();
  printf("PASSED\n");

  printf("TESTING STORES...\n");
  test_stores();
  printf("PASSED\n");

  stm_exit_thread();

  printf("TESTING CONCURRENT LOADS AND STORES...\n");
  verbose = 0;
  timeout.tv_sec = DURATION / 1000;
  timeout.tv_nsec = (DURATION % 1000) * 1000000;
  if ((threads = (pthread_t *)malloc(NB_THREADS * sizeof(pthread_t))) == NULL) {
    perror("malloc");
    exit(1);
  }
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  for (i = 0; i < NB_THREADS; i++) {
    if (pthread_create(&threads[i], &attr, test, NULL) != 0) {
      fprintf(stderr, "Error creating thread\n");
      exit(1);
    }
  }
  pthread_attr_destroy(&attr);
  nanosleep(&timeout, NULL);
  printf("STOPPING...\n");
  stop = 1;
  for (i = 0; i < NB_THREADS; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      fprintf(stderr, "Error waiting for thread completion\n");
      exit(1);
    }
  }
  printf("PASSED\n");

  /* Cleanup STM */
  stm_exit();

  return 0;
}
