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
/* Compile with explicit calls to ITM library */
# include <bits/wordsize.h>
# include "libitm.h"

/* Define TM_MACROS */
# ifdef EXPLICIT_TX_PARAMETER
#  define TXARG                         __td
#  define TXARGS                        __td,
#  define TM_START(id, ro)              { _ITM_transaction* __td = _ITM_getTransaction(); \
                                          _ITM_beginTransaction(TXARGS ro == RO ? pr_readOnly | pr_instrumentedCode : pr_instrumentedCode, NULL);
# else
#  define TXARG
#  define TXARGS
#  define TM_START(id, ro)              { _ITM_beginTransaction(ro == RO ? pr_readOnly | pr_instrumentedCode : pr_instrumentedCode, NULL);
# endif
// TODO check if __LP64__ is better?
# if __WORDSIZE == 64
#  define TM_LOAD(addr)                 _ITM_RU8(TXARGS (uint64_t *)addr)
#  define TM_STORE(addr, value)         _ITM_WU8(TXARGS (uint64_t *)addr, (uint64_t)value)
# else /* __WORDSIZE == 32 */
#  define TM_LOAD(addr)                 _ITM_RU4(TXARGS (uint32_t *)addr)
#  define TM_STORE(addr, value)         _ITM_WU4(TXARGS (uint32_t *)addr, (uint32_t)value)
# endif /* __WORDSIZE == 32 */
# define TM_COMMIT                      _ITM_commitTransaction(TXARGS); }
/* TODO Wrong for Intel */
# define TM_MALLOC(size)                _ITM_malloc(TXARGS size)
# define TM_FREE(addr)                  _ITM_free(TXARGS addr)
# define TM_FREE2(addr, size)           _ITM_free(TXARGS addr)

# define TM_INIT                        _ITM_initializeProcess()
# define TM_EXIT                        _ITM_finalizeProcess()
# define TM_INIT_THREAD                 _ITM_initializeThread()
# define TM_EXIT_THREAD                 _ITM_finalizeThread()

/* Annotations used in this benchmark */
# define TM_SAFE
# define TM_PURE

#endif /* _TM_MACROS_H_ */

