/*
 * File:
 *   tm_macros.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Defines macros for transactional operations.
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

#ifndef _TM_MACROS_H_
# define _TM_MACROS_H_
/* TODO check for exit() and perror() */
/* TODO check function pointer in rbtree */
/* 3.0 #define __INTEL_COMPILER_BUILD_DATE 20081204 */
/* 4.0 #define __INTEL_COMPILER_BUILD_DATE 20100806 */
# if __INTEL_COMPILER_BUILD_DATE < 20100806
#  define TM_START(id,ro)                   __tm_atomic {
# else /* __INTEL_COMPILER_BUILD_DATE >= 20100806 */
#  define TM_START(id,ro)                   __transaction [[atomic]] {
# endif /* __INTEL_COMPILER_BUILD_DATE >= 20100806 */
# define TM_LOAD(x)                         *x
# define TM_STORE(x,y)                      *x=y
# define TM_COMMIT                          }
# define TM_MALLOC(size)                    malloc(size)
# define TM_FREE(addr)                      free(addr)
# define TM_FREE2(addr, size)               free(addr)

# define TM_INIT
# define TM_EXIT
# define TM_INIT_THREAD
# define TM_EXIT_THREAD

/* Define Annotations */
# if __INTEL_COMPILER_BUILD_DATE < 20100806
#  define TM_PURE                           __attribute__((tm_pure))
#  define TM_SAFE                           __attribute__((tm_callable))
# else /* __INTEL_COMPILER_BUILD_DATE >= 20100806 */
#  define TM_PURE                           [[transaction_pure]]
#  define TM_SAFE                           [[transaction_safe]]

/* error: non [[transaction_safe]] function "malloc" called inside [[transaction_safe]] routine */
TM_SAFE
void *malloc(size_t);

TM_SAFE
void free(void *);

# endif /* __INTEL_COMPILER_BUILD_DATE >= 20100806 */

#endif /* _TM_MACROS_H_ */

