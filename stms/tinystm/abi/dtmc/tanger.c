/*
 * File:
 *   tanger.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Tanger adapter for tinySTM.
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

/* This file is designed to work with DTMC (Tanger/LLVM).
 * DTMC is not 100% compatible with Intel ABI yet thus this file 
 * permits to propose a workaround.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <bits/wordsize.h>

/* A transaction descriptor/handle/... */
//typedef void tanger_stm_tx_t;

#ifndef TANGER_LOADSTORE_ATTR
/* FIXME: LLVM seems to have a bug when inlining these functions. Check if fixed with newer version. */
//# define TANGER_LOADSTORE_ATTR __attribute__((nothrow,always_inline))
# define TANGER_LOADSTORE_ATTR __attribute__((nothrow,noinline))
#endif /* TANGER_LOADSTORE_ATTR */

#define TM_LOAD    stm_load
#define TM_STORE   stm_store

/* TODO manage properly TLS but llvm-gcc should do */
__thread appstack_t appstack;

/* ################################################################### *
 * TANGER FUNCTIONS
 * ################################################################### */

#ifdef EXPLICIT_TX_PARAMETER
# define TX_PARAM (struct stm_tx *)tx,
#else
# define TX_PARAM
#endif

TANGER_LOADSTORE_ATTR
uint8_t tanger_stm_load1(tanger_stm_tx_t *tx, uint8_t *addr)
{
#ifdef STACK_CHECK
  /* TODO add unlikely */
  if (on_stack(addr))
    return *addr;
#endif /* STACK_CHECK */
  return stm_load_u8(TX_PARAM addr);
}

TANGER_LOADSTORE_ATTR
uint8_t tanger_stm_load8(tanger_stm_tx_t *tx, uint8_t *addr)
{
#ifdef STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* STACK_CHECK */
  return stm_load_u8(TX_PARAM addr);
}

TANGER_LOADSTORE_ATTR
uint16_t tanger_stm_load16(tanger_stm_tx_t *tx, uint16_t *addr)
{
#ifdef STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* STACK_CHECK */
  return stm_load_u16(TX_PARAM addr);
}

TANGER_LOADSTORE_ATTR
uint32_t tanger_stm_load32(tanger_stm_tx_t *tx, uint32_t *addr)
{
#ifdef STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* STACK_CHECK */
  return stm_load_u32(TX_PARAM addr);
}

TANGER_LOADSTORE_ATTR
uint64_t tanger_stm_load64(tanger_stm_tx_t *tx, uint64_t *addr)
{
#ifdef STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* STACK_CHECK */
  return stm_load_u64(TX_PARAM addr);
}

TANGER_LOADSTORE_ATTR
uint16_t tanger_stm_load16aligned(tanger_stm_tx_t *tx, uint16_t *addr)
{
#ifdef STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* STACK_CHECK */
  return stm_load_u16(TX_PARAM addr);
}

TANGER_LOADSTORE_ATTR
uint32_t tanger_stm_load32aligned(tanger_stm_tx_t *tx, uint32_t *addr)
{
#ifdef STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* STACK_CHECK */
#if __WORDSIZE == 32
  return (uint32_t)TM_LOAD(TX_PARAM (volatile stm_word_t *)addr);
#else
  return stm_load_u32(TX_PARAM addr);
#endif
}

TANGER_LOADSTORE_ATTR
uint64_t tanger_stm_load64aligned(tanger_stm_tx_t *tx, uint64_t *addr)
{
#ifdef STACK_CHECK
  if (on_stack(addr))
    return *addr;
#endif /* STACK_CHECK */
#if __WORDSIZE == 64
  return (uint64_t)TM_LOAD(TX_PARAM (volatile stm_word_t *)addr);
#else 
  return stm_load_u64(TX_PARAM addr);
#endif 
}

TANGER_LOADSTORE_ATTR
void tanger_stm_loadregion(tanger_stm_tx_t* tx, uint8_t *src, uintptr_t bytes, uint8_t *dest)
{
#ifdef STACK_CHECK
  if (on_stack(src))
    memcpy(dest, src, bytes);
#endif /* STACK_CHECK */
  stm_load_bytes(TX_PARAM src, dest, bytes);
}

TANGER_LOADSTORE_ATTR
void* tanger_stm_loadregionpre(tanger_stm_tx_t* tx, uint8_t *addr, uintptr_t bytes)
{
  fprintf(stderr, "%s: not yet implemented\n", __func__);
  return NULL;
}

TANGER_LOADSTORE_ATTR
void tanger_stm_loadregionpost(tanger_stm_tx_t* tx, uint8_t *addr, uintptr_t bytes)
{
  fprintf(stderr, "%s: not yet implemented\n", __func__);
}

TANGER_LOADSTORE_ATTR
void tanger_stm_store1(tanger_stm_tx_t *tx, uint8_t *addr, uint8_t value)
{
#ifdef STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* STACK_CHECK */
  stm_store_u8(TX_PARAM addr, value);
}

TANGER_LOADSTORE_ATTR
void tanger_stm_store8(tanger_stm_tx_t *tx, uint8_t *addr, uint8_t value)
{
#ifdef STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* STACK_CHECK */
  stm_store_u8(TX_PARAM addr, value);
}

TANGER_LOADSTORE_ATTR
void tanger_stm_store16(tanger_stm_tx_t *tx, uint16_t *addr, uint16_t value)
{
#ifdef STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* STACK_CHECK */
  stm_store_u16(TX_PARAM addr, value);
}

TANGER_LOADSTORE_ATTR
void tanger_stm_store32(tanger_stm_tx_t *tx, uint32_t *addr, uint32_t value)
{
#ifdef STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* STACK_CHECK */
  stm_store_u32(TX_PARAM addr, value);
}

TANGER_LOADSTORE_ATTR
void tanger_stm_store64(tanger_stm_tx_t *tx, uint64_t *addr, uint64_t value)
{
#ifdef STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* STACK_CHECK */
  stm_store_u64(TX_PARAM addr, value);
}

TANGER_LOADSTORE_ATTR
void tanger_stm_store16aligned(tanger_stm_tx_t *tx, uint16_t *addr, uint16_t value)
{
#ifdef STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* STACK_CHECK */
  stm_store_u16(TX_PARAM addr, value);
}

TANGER_LOADSTORE_ATTR
void tanger_stm_store32aligned(tanger_stm_tx_t *tx, uint32_t *addr, uint32_t value)
{
#ifdef STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* STACK_CHECK */
#if __WORDSIZE == 32
  TM_STORE(TX_PARAM (volatile stm_word_t *)addr, (stm_word_t)value);
#else
  stm_store_u32(TX_PARAM addr, value);
#endif
}

TANGER_LOADSTORE_ATTR
void tanger_stm_store64aligned(tanger_stm_tx_t *tx, uint64_t *addr, uint64_t value)
{
#ifdef STACK_CHECK
  if (on_stack(addr)) {
    *addr = value;
    return;
  }
#endif /* STACK_CHECK */
#if __WORD_SIZE == 64
  TM_STORE(TX_PARAM (volatile stm_word_t *)addr, (stm_word_t)value);
#else
  stm_store_u64(TX_PARAM addr, value);
#endif
}

TANGER_LOADSTORE_ATTR
void tanger_stm_storeregion(tanger_stm_tx_t* tx, uint8_t *src, uintptr_t bytes, uint8_t *dest)
{
#ifdef STACK_CHECK
  if (on_stack(dest)) {
    memcpy(dest, src, bytes);
    return;
  }
#endif /* STACK_CHECK */
  stm_store_bytes(TX_PARAM src, dest, bytes);
}

TANGER_LOADSTORE_ATTR
void* tanger_stm_storeregionpre(tanger_stm_tx_t* tx, uint8_t *addr, uintptr_t bytes)
{
  fprintf(stderr, "%s: not yet implemented\n", __func__);
  return NULL;
}

TANGER_LOADSTORE_ATTR
void* tanger_stm_updateregionpre(tanger_stm_tx_t* tx, uint8_t *addr, uintptr_t bytes)
{
  fprintf(stderr, "%s: not yet implemented\n", __func__);
  return NULL;
}

tanger_stm_tx_t *tanger_stm_get_tx()
{
  struct stm_tx *tx = stm_current_tx();
  if (unlikely(tx == NULL)) {
    /* Thread not initialized: must create transaction */
    _ITM_initializeThread();
    tx = stm_current_tx();
  }

  return (tanger_stm_tx_t *)tx;
}


/* TODO manage nesting */
void tanger_stm_save_restore_stack(void* low_addr, void* high_addr) __attribute__((noinline));
void tanger_stm_save_restore_stack(void* low_addr, void* high_addr)
{
  /* Saving stack info and backup the stack in beginTransaction because LLVM
   *  can add code between this function and beginTransaction. */
  appstack.stack_addr = low_addr;
  appstack.stack_size = (size_t)high_addr - (size_t)low_addr;
}

void tanger_stm_init()
{
  _ITM_initializeProcess();
}

void tanger_stm_shutdown()
{
  _ITM_finalizeProcess();
}

void tanger_stm_thread_init()
{
  _ITM_initializeThread();
}

void tanger_stm_thread_shutdown()
{
  _ITM_finalizeThread();
}

/* TODO check if ok */
//void *tanger_stm_malloc(size_t size, tanger_stm_tx_t* tx)
void *tanger_stm_malloc(size_t size)
{
  return _ITM_malloc(size);
}

void tanger_stm_free(void *ptr)
{
  _ITM_free(ptr);
}

void *tanger_stm_calloc(size_t nmemb, size_t size)
{
  void *p = _ITM_malloc(nmemb * size);
  memset(p, 0, nmemb * size);
  return p;
}

void *tanger_stm_realloc(void *ptr, size_t size)
{
  /* TODO to ITM_imize */
  void *p;
#ifdef EXPLICIT_TX_PARAMETER
  struct stm_tx * tx = stm_current_tx();
#endif /* EXPLICIT_TX_PARAMETER */
  if (ptr == NULL) {
    /* Equivalent to malloc */
    return tanger_stm_malloc(size);
  }
  if (size == 0) {
    /* Equivalent to free */
    tanger_stm_free(ptr);
    return NULL;
  }
  /* Allocate new region */
  p = tanger_stm_malloc(size);
  /* Copy old content to new region */
  stm_load_bytes(TX_PARAM ptr, p, malloc_usable_size(ptr));
  /* Free old region */
  tanger_stm_free(ptr);

  return p;
}

/* Cleaning macros */
#undef TM_LOAD
#undef TM_STORE

