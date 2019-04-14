/*
 * File:
 *   tanger.h
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

#ifndef _TANGER_H_
# define _TANGER_H_

typedef struct {
  void *stack_addr;
  size_t stack_size;
  void *data;
  size_t data_size;
} appstack_t;

extern __thread appstack_t appstack;

static inline void tanger_stm_save_stack()
{
  // Is the data big enough?
  if (appstack.stack_size > appstack.data_size) {
    // TODO round to 4096+
    appstack.data_size = appstack.stack_size;
    appstack.data = realloc(appstack.data, appstack.data_size);
  }
  __builtin_memcpy(appstack.data, appstack.stack_addr, appstack.stack_size);
}

static inline void tanger_stm_restore_stack()
{
  __builtin_memcpy(appstack.stack_addr, appstack.data, appstack.stack_size);
}

static inline void tanger_stm_reset_stack()
{
  appstack.stack_addr = NULL;
}

static inline void tanger_stm_free_stack()
{
  if (appstack.data) {
    free(appstack.data);
    appstack.data = NULL;
  }
}

#endif /* _TANGER_H_ */

