/*
 * File:
 *   utils.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Utilities functions.
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

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdio.h>
#include <stdlib.h>

#define COMPILE_TIME_ASSERT(pred)       switch (0) { case 0: case pred: ; }

#ifdef DEBUG2
# ifndef DEBUG
#  define DEBUG
# endif /* ! DEBUG */
#endif /* DEBUG2 */

#ifdef DEBUG
/* Note: stdio is thread-safe */
# define IO_FLUSH                       fflush(NULL)
# define PRINT_DEBUG(...)               printf(__VA_ARGS__); fflush(NULL)
#else /* ! DEBUG */
# define IO_FLUSH
# define PRINT_DEBUG(...)
#endif /* ! DEBUG */

#ifdef DEBUG2
# define PRINT_DEBUG2(...)              PRINT_DEBUG(__VA_ARGS__)
#else /* ! DEBUG2 */
# define PRINT_DEBUG2(...)
#endif /* ! DEBUG2 */

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#ifndef CACHELINE_SIZE
/* It ensures efficient usage of cache and avoids false sharing.
 * It could be defined in an architecture specific file. */
# define CACHELINE_SIZE                 64
#endif

#if defined(__GNUC__) || defined(__INTEL_COMPILER)
# define likely(x)                      __builtin_expect(!!(x), 1)
# define unlikely(x)                    __builtin_expect(!!(x), 0)
# define INLINE                         inline __attribute__((always_inline))
# define NOINLINE                       __attribute__((noinline))
# if defined(__INTEL_COMPILER)
#  define ALIGNED                       /* Unknown */
# else /* ! __INTEL_COMPILER */
#  define ALIGNED                       __attribute__((aligned(CACHELINE_SIZE)))
# endif /* ! __INTEL_COMPILER */
#else /* ! (defined(__GNUC__) || defined(__INTEL_COMPILER)) */
# define likely(x)                      (x)
# define unlikely(x)                    (x)
# define INLINE                         inline
# define NOINLINE                       /* None in the C standard */
# define ALIGNED                        /* None in the C standard */
#endif /* ! (defined(__GNUC__) || defined(__INTEL_COMPILER)) */

/*
 * malloc/free wrappers.
 */
static INLINE void*
xmalloc(size_t size)
{
  void *memptr = malloc(size);
  if (unlikely(memptr == NULL)) {
    perror("malloc");
    exit(1);
  }
  return memptr;
}

static INLINE void*
xcalloc(size_t count, size_t size)
{
  void *memptr = calloc(count, size);
  if (unlikely(memptr == NULL)) {
    perror("calloc");
    exit(1);
  }
  return memptr;
}

static INLINE void*
xrealloc(void *addr, size_t size)
{
  addr = realloc(addr, size);
  if (unlikely(addr == NULL)) {
    perror("realloc");
    exit(1);
  }
  return addr;
}

static INLINE void
xfree(void *mem)
{
  free(mem);
}

static INLINE void*
xmalloc_aligned(size_t size)
{
  void *memptr;
  /* TODO is posix_memalign is not available, provide malloc fallback. */
  /* Make sure that the allocation is aligned with cacheline size. */
#if defined(__CYGWIN__) || defined (__sun__)
  memptr = memalign(CACHELINE_SIZE, size);
#elif defined(__APPLE__)
  memptr = valloc(size);
#else
  if (unlikely(posix_memalign(&memptr, CACHELINE_SIZE, size)))
    memptr = NULL;
#endif
  if (unlikely(memptr == NULL)) {
    fprintf(stderr, "Error allocating aligned memory\n");
    exit(1);
  }
  return memptr;
}

#endif /* !_UTILS_H_ */

