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

# define TM_START(id,ro)                    __tm_atomic {
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
# define TM_PURE                            __attribute__((tm_pure))
# define TM_SAFE                            __attribute__((tm_callable))

/* FIXME to be removed when DTMC will support annotations */
static double tanger_wrapperpure_erand48(unsigned short int __xsubi[3]) __attribute__ ((weakref("erand48")));

#endif /* _TM_MACROS_H_ */

