/*
 * File:
 *   mod_log.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Module for logging memory accesses.
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mod_log.h"

#include "stm.h"
#include "utils.h"

#ifndef LW_SET_SIZE
# define LW_SET_SIZE                    1024
#endif /* ! LW_SET_SIZE */

/* ################################################################### *
 * TYPES
 * ################################################################### */

enum {
  TYPE_WORD,
  TYPE_U8,
  TYPE_U16,
  TYPE_U32,
  TYPE_U64,
  TYPE_CHAR,
  TYPE_UCHAR,
  TYPE_SHORT,
  TYPE_USHORT,
  TYPE_INT,
  TYPE_UINT,
  TYPE_LONG,
  TYPE_ULONG,
  TYPE_FLOAT,
  TYPE_DOUBLE,
  TYPE_PTR,
  TYPE_BYTES
};

typedef struct mod_log_w_entry {        /* Write set entry */
  int type;                             /* Data type */
  union {                               /* Address written and old value */
    struct { stm_word_t *a; stm_word_t v; } w;
    struct { uint8_t *a; char v; } u8;
    struct { uint16_t *a; char v; } u16;
    struct { uint32_t *a; char v; } u32;
    struct { uint64_t *a; char v; } u64;
    struct { char *a; char v; } c;
    struct { unsigned char *a; unsigned char v; } uc;
    struct { short *a; short v; } s;
    struct { unsigned short *a; unsigned short v; } us;
    struct { int *a; int v; } i;
    struct { unsigned int *a; unsigned int v; } ui;
    struct { long *a; long v; } l;
    struct { unsigned long *a; unsigned long v; } ul;
    struct { float *a; float v; } f;
    struct { double *a; double v; } d;
    struct { void **a; void *v; } p;
    struct { uint8_t *a; uint8_t *v; size_t s; } b;
  } data;
} mod_log_w_entry_t;

typedef struct mod_log_w_set {          /* Write set */
  mod_log_w_entry_t *entries;           /* Array of entries */
  int nb_entries;                       /* Number of entries */
  int size;                             /* Size of array */
  int allocated;                        /* Memory blocks allocated */
} mod_log_w_set_t;

static int mod_log_key;
static int mod_log_initialized = 0;

/* ################################################################### *
 * STATIC
 * ################################################################### */

/*
 * Called by the CURRENT thread to obtain log entry.
 */
static inline mod_log_w_entry_t *get_entry(void)
{
  mod_log_w_set_t *ws;

  if (!mod_log_initialized) {
    fprintf(stderr, "Module mod_log not initialized\n");
    exit(1);
  }

  /* Store in undo log */
  ws = (mod_log_w_set_t *)stm_get_specific(mod_log_key);
  assert(ws != NULL);

  if (ws->nb_entries == ws->size) {
    /* Extend read set */
    ws->size = (ws->size < LW_SET_SIZE ? LW_SET_SIZE : ws->size * 2);
    ws->entries = (mod_log_w_entry_t *)xrealloc(ws->entries, ws->size * sizeof(mod_log_w_entry_t));
  }

  return &ws->entries[ws->nb_entries++];
}

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

void stm_log(stm_word_t *addr)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_WORD;
  w->data.w.a = addr;
  w->data.w.v = *addr;
}

void stm_log_u8(uint8_t *addr)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_U8;
  w->data.u8.a = addr;
  w->data.u8.v = *addr;
}

void stm_log_u16(uint16_t *addr)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_U16;
  w->data.u16.a = addr;
  w->data.u16.v = *addr;
}

void stm_log_u32(uint32_t *addr)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_U32;
  w->data.u32.a = addr;
  w->data.u32.v = *addr;
}

void stm_log_u64(uint64_t *addr)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_U64;
  w->data.u64.a = addr;
  w->data.u64.v = *addr;
}

void stm_log_char(char *addr)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_CHAR;
  w->data.c.a = addr;
  w->data.c.v = *addr;
}

void stm_log_uchar(unsigned char *addr)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_UCHAR;
  w->data.uc.a = addr;
  w->data.uc.v = *addr;
}

void stm_log_short(short *addr)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_SHORT;
  w->data.s.a = addr;
  w->data.s.v = *addr;
}

void stm_log_ushort(unsigned short *addr)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_USHORT;
  w->data.us.a = addr;
  w->data.us.v = *addr;
}

void stm_log_int(int *addr)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_INT;
  w->data.i.a = addr;
  w->data.i.v = *addr;
}

void stm_log_uint(unsigned int *addr)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_UINT;
  w->data.ui.a = addr;
  w->data.ui.v = *addr;
}

void stm_log_long(long *addr)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_LONG;
  w->data.l.a = addr;
  w->data.l.v = *addr;
}

void stm_log_ulong(unsigned long *addr)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_ULONG;
  w->data.ul.a = addr;
  w->data.ul.v = *addr;
}

void stm_log_float(float *addr)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_FLOAT;
  w->data.f.a = addr;
  w->data.f.v = *addr;
}

void stm_log_double(double *addr)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_DOUBLE;
  w->data.d.a = addr;
  w->data.d.v = *addr;
}

void stm_log_ptr(void **addr)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_PTR;
  w->data.p.a = addr;
  w->data.p.v = *addr;
}

void stm_log_bytes(uint8_t *addr, size_t size)
{
  mod_log_w_entry_t *w = get_entry();

  w->type = TYPE_BYTES;
  w->data.b.a = addr;
  w->data.b.v = (uint8_t *)xmalloc(size);
  w->data.b.s = size;
  memcpy(w->data.b.v, addr, size);

  /* Remember we have allocated memory */
  ((mod_log_w_set_t *)stm_get_specific(mod_log_key))->allocated++;
}

/*
 * Called upon thread creation.
 */
static void mod_log_on_thread_init(void *arg)
{
  mod_log_w_set_t *ws;

  ws = (mod_log_w_set_t *)xmalloc(sizeof(mod_log_w_set_t));
  ws->entries = NULL;
  ws->nb_entries = ws->size = ws->allocated = 0;

  stm_set_specific(mod_log_key, ws);
}

/*
 * Called upon thread deletion.
 */
static void mod_log_on_thread_exit(void *arg)
{
  mod_log_w_set_t *ws;

  ws = (mod_log_w_set_t *)stm_get_specific(mod_log_key);
  assert(ws != NULL);

  xfree(ws->entries);
  xfree(ws);
}

/*
 * Called upon transaction commit.
 */
static void mod_log_on_commit(void *arg)
{
  mod_log_w_set_t *ws;
  mod_log_w_entry_t *w;

  ws = (mod_log_w_set_t *)stm_get_specific(mod_log_key);
  assert(ws != NULL);

  /* Free memory */
  if (ws->allocated > 0) {
    w = ws->entries;
    do {
      assert(w < &ws->entries[ws->nb_entries]);
      if (w->type == TYPE_BYTES) {
        xfree(w->data.b.v);
        ws->allocated--;
      }
      w++;
    } while (ws->allocated > 0);
  }
  /* Erase undo log */
  ws->nb_entries = 0;
}

/*
 * Called upon transaction abort.
 */
static void mod_log_on_abort(void *arg)
{
  mod_log_w_set_t *ws;
  mod_log_w_entry_t *w;

  ws = (mod_log_w_set_t *)stm_get_specific(mod_log_key);
  assert(ws != NULL);

  if (ws->nb_entries > 0) {
    /* Apply undo log in reverse order */
    w = &ws->entries[ws->nb_entries - 1];
    do {
      switch (w->type) {
       case TYPE_WORD:
         *w->data.w.a = w->data.w.v;
         break;
       case TYPE_U8:
         *w->data.u8.a = w->data.u8.v;
         break;
       case TYPE_U16:
         *w->data.u16.a = w->data.u16.v;
         break;
       case TYPE_U32:
         *w->data.u32.a = w->data.u32.v;
         break;
       case TYPE_U64:
         *w->data.u64.a = w->data.u64.v;
         break;
       case TYPE_CHAR:
         *w->data.c.a = w->data.c.v;
         break;
       case TYPE_UCHAR:
         *w->data.uc.a = w->data.uc.v;
         break;
       case TYPE_SHORT:
         *w->data.s.a = w->data.s.v;
         break;
       case TYPE_USHORT:
         *w->data.us.a = w->data.us.v;
         break;
       case TYPE_INT:
         *w->data.i.a = w->data.i.v;
         break;
       case TYPE_UINT:
         *w->data.ui.a = w->data.ui.v;
         break;
       case TYPE_LONG:
         *w->data.l.a = w->data.l.v;
         break;
       case TYPE_ULONG:
         *w->data.ul.a = w->data.ul.v;
         break;
       case TYPE_FLOAT:
         *w->data.f.a = w->data.f.v;
         break;
       case TYPE_DOUBLE:
         *w->data.d.a = w->data.d.v;
         break;
       case TYPE_PTR:
         *w->data.p.a = w->data.p.v;
         break;
       case TYPE_BYTES:
         memcpy(w->data.b.a, w->data.b.v, w->data.b.s);
         xfree(w->data.b.v);
         ws->allocated--;
         break;
       default:
         fprintf(stderr, "Unexpected entry in undo log\n");
         abort();
         exit(1);
      }
    } while (--w >= ws->entries);
    /* Erase undo log */
    ws->nb_entries = 0;
  }
  assert(ws->allocated == 0);
}

/*
 * Initialize module.
 */
void mod_log_init(void)
{
  if (mod_log_initialized)
    return;

  if (!stm_register(mod_log_on_thread_init, mod_log_on_thread_exit, NULL, NULL, mod_log_on_commit, mod_log_on_abort, NULL)) {
    fprintf(stderr, "Cannot register callbacks\n");
    exit(1);
  }
  mod_log_key = stm_create_specific();
  if (mod_log_key < 0) {
    fprintf(stderr, "Cannot create specific key\n");
    exit(1);
  }
  mod_log_initialized = 1;
}
