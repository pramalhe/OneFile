#include <stdlib.h>
#include "mod_mem.h"

/* TODO make inline calls */

void *malloc_txn(size_t size) __asm__("malloc._$TXN");
void *malloc_txn(size_t size)
{
  return stm_malloc(size);
}

void *malloc_wraptxn(size_t size) __asm__("malloc._$WrapTXN");
void *malloc_wraptxn(size_t size)
{
  __asm__ __volatile__("jmp 1f\nmov $0xf0f0f0f0,%eax\n1:");
  return malloc_txn(size);
}

void *calloc_txn(size_t nmemb, size_t size) __asm__("calloc._$TXN");
void *calloc_txn(size_t nmemb, size_t size)
{
  return stm_calloc(nmemb, size);
}

void *calloc_wraptxn(size_t nmemb, size_t size) __asm__("calloc._$WrapTXN");
void *calloc_wraptxn(size_t nmemb, size_t size)
{
  __asm__ __volatile__("jmp 1f\nmov $0xf0f0f0f0,%eax\n1:");
  return calloc_txn(nmemb, size);
}

void free_txn(void *addr) __asm__("free._$TXN");
void free_txn(void *addr)
{
  stm_free(addr, sizeof(void *));
}

void free_wraptxn(void *addr) __asm__("free._$WrapTXN");
void free_wraptxn(void *addr)
{
  __asm__ __volatile__("jmp 1f\nmov $0xf0f0f0f0,%eax\n1:");
  free_txn(addr);
}

#if 0
/* TODO */
_mm_free._$TXN
_mm_free._$WrapTXN
_mm_malloc._$TXN
_mm_malloc._$WrapTXN

void* _mm_malloc (int size, int align)
void _mm_free (void *p)

_ZdaPv._$TXN
_ZdaPv._$WrapTXN
_ZdlPv._$TXN
_ZdlPv._$WrapTXN
_Znam._$TXN
_Znam._$WrapTXN
_Znwm._$TXN
_Znwm._$WrapTXN
#endif 

