/*
 * File:
 *   mod_mem.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Module for dynamic memory management.
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

/**
 * @file
 *   Module for dynamic memory management.  This module provides
 *   functions for allocations and freeing memory inside transactions.
 *   A block allocated inside the transaction will be implicitly freed
 *   upon abort, and a block freed inside a transaction will only be
 *   returned to the system upon commit.
 * @author
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * @date
 *   2007-2014
 */

#ifndef _MOD_MEM_H_
# define _MOD_MEM_H_

# include "stm.h"

# ifdef __cplusplus
extern "C" {
# endif

//@{
/**
 * Allocate memory from inside a transaction.  Allocated memory is
 * implicitly freed upon abort.
 *
 * @param size
 *   Number of bytes to allocate.
 * @return
 *   Pointer to the allocated memory block.
 */
void *stm_malloc(size_t size);
void *stm_malloc_tx(struct stm_tx *tx, size_t size);
//@}

//@{
/**
 * Allocate initialized memory from inside a transaction.  Allocated
 * memory is implicitly freed upon abort.
 *
 * @param nm
 *   Size of the array to allocate.
 * @param size
 *   Number of bytes to allocate.
 * @return
 *   Pointer to the allocated memory block.
 */
void *stm_calloc(size_t nm, size_t size);
void *stm_calloc_tx(struct stm_tx *tx, size_t nm, size_t size);
//@}

//@{
/**
 * Free memory from inside a transaction.  Freed memory is only returned
 * to the system upon commit and can optionally be overwritten (more
 * precisely, the locks protecting the memory are acquired) to prevent
 * another transaction from accessing the freed memory and observe
 * inconsistent states.
 *
 * @param addr
 *   Address of the memory block.
 * @param size
 *   Number of bytes to overwrite.
 */
void stm_free(void *addr, size_t size);
void stm_free_tx(struct stm_tx *tx, void *addr, size_t size);
//@}

//@{
/**
 * Free memory from inside a transaction.  Freed memory is only returned
 * to the system upon commit and can optionally be overwritten (more
 * precisely, the locks protecting the memory are acquired) to prevent
 * another transaction from accessing the freed memory and observe
 * inconsistent states.
 *
 * @param addr
 *   Address of the memory block.
 * @param idx
 *   Index of the first byte to overwrite.
 * @param size
 *   Number of bytes to overwrite.
 */
void stm_free2(void *addr, size_t idx, size_t size);
void stm_free2_tx(struct stm_tx *tx, void *addr, size_t idx, size_t size);
//@}

/**
 * Initialize the module.  This function must be called once, from the
 * main thread, after initializing the STM library and before
 * performing any transactional operation.
 *
 * @param gc
 *   True (non-zero) to enable epoch-based garbage collector when
 *   freeing memory in transactions.
 */
void mod_mem_init(int gc);

# ifdef __cplusplus
}
# endif

#endif /* _MOD_MEM_H_ */
