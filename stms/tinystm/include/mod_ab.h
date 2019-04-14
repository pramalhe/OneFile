/*
 * File:
 *   mod_ab.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Module for gathering statistics about atomic blocks.
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
 *   Module for gathering statistics about transactions.  This module
 *   maintains aggregate statistics about all threads for every atomic
 *   block in the application (distinguished using the identifier part
 *   of the transaction attributes).
 * @author
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * @date
 *   2007-2014
 */

#ifndef _MOD_AB_H_
# define _MOD_AB_H_

# include "stm.h"

# ifdef __cplusplus
extern "C" {
# endif

/**
 * Statistics associated with an atomic block.
 */
typedef struct stm_ab_stats {
  /**
   * Number of samples collected.
   */
  unsigned long samples;
  /**
   * Arithmetic mean of the samples.
   */
  double mean;
  /**
   * Variance of the samples.
   */
  double variance;
  /**
   * Minimum value among all samples.
   */
  double min;
  /**
   * Maximum value among all samples.
   */
  double max;
  /**
   * 75th percentile (median).
   */
  double percentile_50;
  /**
   * 90th percentile.
   */
  double percentile_90;
  /**
   * 95th percentile.
   */
  double percentile_95;
  /**
   * Sorted ramdom subset of the samples (Vitter's reservoir).
   */
  double *reservoir;
  /**
   * Number of smaples in the reservoir.
   */
  unsigned int reservoir_size;
} stm_ab_stats_t;

/**
 * Get statistics about an atomic block.
 *
 * @param id
 *   Identifier of the atomic block (as specified in transaction
 *   attributes).
 * @param stats
 *   Pointer to the variable to should hold the statistics of the atomic
 *   block.
 * @return
 *   1 upon success, 0 otherwise.
 */
int stm_get_ab_stats(int id, stm_ab_stats_t *stats);

/**
 * Initialize the module.  This function must be called once, from the
 * main thread, after initializing the STM library and before
 * performing any transactional operation.
 *
 * @param freq
 *   Inverse sampling frequency (1 to keep all samples).
 * @param check
 *   Pointer to a function that will be called to check if a sample is
 *   valid and should be kept.  The event will be discarded if and only
 *   if the function returns 0.  If no function is provided, all samples
 *   will be kept.
 */
void mod_ab_init(int freq, int (*check)(void));

# ifdef __cplusplus
}
# endif

#endif /* _MOD_AB_H_ */
