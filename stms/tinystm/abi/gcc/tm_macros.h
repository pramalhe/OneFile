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

# define TM_START(id,ro)                    __transaction_atomic {
# define TM_LOAD(x)                         *x
# define TM_STORE(x,y)                      *x=y
# define TM_COMMIT                          }
# define TM_MALLOC(size)                    malloc(size)
// TODO is it possible to do TM_FREE(addr ...)  free(addr) ? //__VA_ARGS__
# define TM_FREE(addr)                      free(addr)
# define TM_FREE2(addr, size)               free(addr)

# define TM_INIT
# define TM_EXIT
# define TM_INIT_THREAD
# define TM_EXIT_THREAD

/* Define Annotations */
# define TM_PURE                            __attribute__((transaction_pure))
# define TM_SAFE                            __attribute__((transaction_safe))

#endif /* _TM_MACROS_H_ */

