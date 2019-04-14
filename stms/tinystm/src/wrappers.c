/*
 * File:
 *   wrappers.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   STM wrapper functions for different data types.
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

#include "utils.h"
#include "stm_internal.h"
#include "wrappers.h"

#define ALLOW_MISALIGNED_ACCESSES

#define TM_LOAD(addr)                stm_load(addr)
#define TM_STORE(addr, val)          stm_store(addr, val)
#define TM_STORE2(addr, val, mask)   stm_store2(addr, val, mask)

typedef union convert_64 {
  uint64_t u64;
  uint32_t u32[2];
  uint16_t u16[4];
  uint8_t u8[8];
  int64_t s64;
  double d;
} convert_64_t;

typedef union convert_32 {
  uint32_t u32;
  uint16_t u16[2];
  uint8_t u8[4];
  int32_t s32;
  float f;
} convert_32_t;

typedef union convert_16 {
  uint16_t u16;
  int16_t s16;
} convert_16_t;

typedef union convert_8 {
  uint8_t u8;
  int8_t s8;
} convert_8_t;

typedef union convert {
  stm_word_t w;
  uint8_t b[sizeof(stm_word_t)];
} convert_t;

static void sanity_checks(void)
{
  COMPILE_TIME_ASSERT(sizeof(convert_64_t) == 8);
  COMPILE_TIME_ASSERT(sizeof(convert_32_t) == 4);
  COMPILE_TIME_ASSERT(sizeof(stm_word_t) == 4 || sizeof(stm_word_t) == 8);
  COMPILE_TIME_ASSERT(sizeof(char) == 1);
  COMPILE_TIME_ASSERT(sizeof(short) == 2);
  COMPILE_TIME_ASSERT(sizeof(int) == 4);
  COMPILE_TIME_ASSERT(sizeof(long) == 4 || sizeof(long) == 8);
  COMPILE_TIME_ASSERT(sizeof(float) == 4);
  COMPILE_TIME_ASSERT(sizeof(double) == 8);
}

/* ################################################################### *
 * INLINE LOADS
 * ################################################################### */

static INLINE
uint8_t int_stm_load_u8(volatile uint8_t *addr)
{
  if (sizeof(stm_word_t) == 4) {
    convert_32_t val;
    val.u32 = (uint32_t)TM_LOAD((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x03));
    return val.u8[(uintptr_t)addr & 0x03];
  } else {
    convert_64_t val;
    val.u64 = (uint64_t)TM_LOAD((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07));
    return val.u8[(uintptr_t)addr & 0x07];
  }
}

static INLINE
uint16_t int_stm_load_u16(volatile uint16_t *addr)
{
  if (unlikely(((uintptr_t)addr & 0x01) != 0)) {
    uint16_t val;
    stm_load_bytes((volatile uint8_t *)addr, (uint8_t *)&val, sizeof(uint16_t));
    return val;
  } else if (sizeof(stm_word_t) == 4) {
    convert_32_t val;
    val.u32 = (uint32_t)TM_LOAD((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x03));
    return val.u16[((uintptr_t)addr & 0x03) >> 1];
  } else {
    convert_64_t val;
    val.u64 = (uint64_t)TM_LOAD((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07));
    return val.u16[((uintptr_t)addr & 0x07) >> 1];
  }
}

static INLINE
uint32_t int_stm_load_u32(volatile uint32_t *addr)
{
  if (unlikely(((uintptr_t)addr & 0x03) != 0)) {
    uint32_t val;
    stm_load_bytes((volatile uint8_t *)addr, (uint8_t *)&val, sizeof(uint32_t));
    return val;
  } else if (sizeof(stm_word_t) == 4) {
    return (uint32_t)TM_LOAD((volatile stm_word_t *)addr);
  } else {
    convert_64_t val;
    val.u64 = (uint64_t)TM_LOAD((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07));
    return val.u32[((uintptr_t)addr & 0x07) >> 2];
  }
}

static INLINE
uint64_t int_stm_load_u64(volatile uint64_t *addr)
{
  if (unlikely(((uintptr_t)addr & 0x07) != 0)) {
    uint64_t val;
    stm_load_bytes((volatile uint8_t *)addr, (uint8_t *)&val, sizeof(uint64_t));
    return val;
  } else if (sizeof(stm_word_t) == 4) {
    convert_64_t val;
    val.u32[0] = (uint32_t)TM_LOAD((volatile stm_word_t *)addr);
    val.u32[1] = (uint32_t)TM_LOAD((volatile stm_word_t *)addr + 1);
    return val.u64;
  } else {
    return (uint64_t)TM_LOAD((volatile stm_word_t *)addr);
  }
}

/* ################################################################### *
 * LOADS
 * ################################################################### */

_CALLCONV uint8_t stm_load_u8(volatile uint8_t *addr)
{
  return int_stm_load_u8(addr);
}

_CALLCONV uint16_t stm_load_u16(volatile uint16_t *addr)
{
  return int_stm_load_u16(addr);
}

_CALLCONV uint32_t stm_load_u32(volatile uint32_t *addr)
{
  return int_stm_load_u32(addr);
}

_CALLCONV uint64_t stm_load_u64(volatile uint64_t *addr)
{
  return int_stm_load_u64(addr);
}

_CALLCONV char stm_load_char(volatile char *addr)
{
  convert_8_t val;
  val.u8 = int_stm_load_u8((volatile uint8_t *)addr);
  return val.s8;
}

_CALLCONV unsigned char stm_load_uchar(volatile unsigned char *addr)
{
  return (unsigned char)int_stm_load_u8((volatile uint8_t *)addr);
}

_CALLCONV short stm_load_short(volatile short *addr)
{
  convert_16_t val;
  val.u16 = int_stm_load_u16((volatile uint16_t *)addr);
  return val.s16;
}

_CALLCONV unsigned short stm_load_ushort(volatile unsigned short *addr)
{
  return (unsigned short)int_stm_load_u16((volatile uint16_t *)addr);
}

_CALLCONV int stm_load_int(volatile int *addr)
{
  convert_32_t val;
  val.u32 = int_stm_load_u32((volatile uint32_t *)addr);
  return val.s32;
}

_CALLCONV unsigned int stm_load_uint(volatile unsigned int *addr)
{
  return (unsigned int)int_stm_load_u32((volatile uint32_t *)addr);
}

_CALLCONV long stm_load_long(volatile long *addr)
{
  if (sizeof(long) == 4) {
    convert_32_t val;
    val.u32 = int_stm_load_u32((volatile uint32_t *)addr);
    return val.s32;
  } else {
    convert_64_t val;
    val.u64 = int_stm_load_u64((volatile uint64_t *)addr);
    return val.s64;
  }
}

_CALLCONV unsigned long stm_load_ulong(volatile unsigned long *addr)
{
  if (sizeof(long) == 4) {
    return (unsigned long)int_stm_load_u32((volatile uint32_t *)addr);
  } else {
    return (unsigned long)int_stm_load_u64((volatile uint64_t *)addr);
  }
}

_CALLCONV float stm_load_float(volatile float *addr)
{
  convert_32_t val;
  val.u32 = int_stm_load_u32((volatile uint32_t *)addr);
  return val.f;
}

_CALLCONV double stm_load_double(volatile double *addr)
{
  convert_64_t val;
  val.u64 = int_stm_load_u64((volatile uint64_t *)addr);
  return val.d;
}

_CALLCONV void *stm_load_ptr(volatile void **addr)
{
  union { stm_word_t w; void *v; } convert;
  convert.w = TM_LOAD((stm_word_t *)addr);
  return convert.v;
}

_CALLCONV void stm_load_bytes(volatile uint8_t *addr, uint8_t *buf, size_t size)
{
  convert_t val;
  unsigned int i;
  stm_word_t *a;

  if (size == 0)
    return;
  i = (uintptr_t)addr & (sizeof(stm_word_t) - 1);
  if (i != 0) {
    /* First bytes */
    a = (stm_word_t *)((uintptr_t)addr & ~(uintptr_t)(sizeof(stm_word_t) - 1));
    val.w = TM_LOAD(a++);
    for (; i < sizeof(stm_word_t) && size > 0; i++, size--)
      *buf++ = val.b[i];
  } else
    a = (stm_word_t *)addr;
  /* Full words */
  while (size >= sizeof(stm_word_t)) {
#ifdef ALLOW_MISALIGNED_ACCESSES
    *((stm_word_t *)buf) = TM_LOAD(a++);
    buf += sizeof(stm_word_t);
#else /* ! ALLOW_MISALIGNED_ACCESSES */
    val.w = TM_LOAD(a++);
    for (i = 0; i < sizeof(stm_word_t); i++)
      *buf++ = val.b[i];
#endif /* ! ALLOW_MISALIGNED_ACCESSES */
    size -= sizeof(stm_word_t);
  }
  if (size > 0) {
    /* Last bytes */
    val.w = TM_LOAD(a);
    i = 0;
    for (i = 0; size > 0; i++, size--)
      *buf++ = val.b[i];
  }
}

/* ################################################################### *
 * INLINE STORES
 * ################################################################### */

static INLINE
void int_stm_store_u8(volatile uint8_t *addr, uint8_t value)
{
  if (sizeof(stm_word_t) == 4) {
    convert_32_t val, mask;
    val.u8[(uintptr_t)addr & 0x03] = value;
    mask.u32 = 0;
    mask.u8[(uintptr_t)addr & 0x03] = ~(uint8_t)0;
    TM_STORE2((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x03), (stm_word_t)val.u32, (stm_word_t)mask.u32);
  } else {
    convert_64_t val, mask;
    val.u8[(uintptr_t)addr & 0x07] = value;
    mask.u64 = 0;
    mask.u8[(uintptr_t)addr & 0x07] = ~(uint8_t)0;
    TM_STORE2((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07), (stm_word_t)val.u64, (stm_word_t)mask.u64);
  }
}

static INLINE
void int_stm_store_u16(volatile uint16_t *addr, uint16_t value)
{
  if (unlikely(((uintptr_t)addr & 0x01) != 0)) {
    stm_store_bytes((volatile uint8_t *)addr, (uint8_t *)&value, sizeof(uint16_t));
  } else if (sizeof(stm_word_t) == 4) {
    convert_32_t val, mask;
    val.u16[((uintptr_t)addr & 0x03) >> 1] = value;
    mask.u32 = 0;
    mask.u16[((uintptr_t)addr & 0x03) >> 1] = ~(uint16_t)0;
    TM_STORE2((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x03), (stm_word_t)val.u32, (stm_word_t)mask.u32);
  } else {
    convert_64_t val, mask;
    val.u16[((uintptr_t)addr & 0x07) >> 1] = value;
    mask.u64 = 0;
    mask.u16[((uintptr_t)addr & 0x07) >> 1] = ~(uint16_t)0;
    TM_STORE2((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07), (stm_word_t)val.u64, (stm_word_t)mask.u64);
  }
}

static INLINE
void int_stm_store_u32(volatile uint32_t *addr, uint32_t value)
{
  if (unlikely(((uintptr_t)addr & 0x03) != 0)) {
    stm_store_bytes((volatile uint8_t *)addr, (uint8_t *)&value, sizeof(uint32_t));
  } else if (sizeof(stm_word_t) == 4) {
    TM_STORE((volatile stm_word_t *)addr, (stm_word_t)value);
  } else {
    convert_64_t val, mask;
    val.u32[((uintptr_t)addr & 0x07) >> 2] = value;
    mask.u64 = 0;
    mask.u32[((uintptr_t)addr & 0x07) >> 2] = ~(uint32_t)0;
    TM_STORE2((volatile stm_word_t *)((uintptr_t)addr & ~(uintptr_t)0x07), (stm_word_t)val.u64, (stm_word_t)mask.u64);
  }
}

static INLINE
void int_stm_store_u64(volatile uint64_t *addr, uint64_t value)
{
  if (unlikely(((uintptr_t)addr & 0x07) != 0)) {
    stm_store_bytes((volatile uint8_t *)addr, (uint8_t *)&value, sizeof(uint64_t));
  } else if (sizeof(stm_word_t) == 4) {
    convert_64_t val;
    val.u64 = value;
    TM_STORE((volatile stm_word_t *)addr, (stm_word_t)val.u32[0]);
    TM_STORE((volatile stm_word_t *)addr + 1, (stm_word_t)val.u32[1]);
  } else {
    return TM_STORE((volatile stm_word_t *)addr, (stm_word_t)value);
  }
}

/* ################################################################### *
 * STORES
 * ################################################################### */

_CALLCONV void stm_store_u8(volatile uint8_t *addr, uint8_t value)
{
  int_stm_store_u8(addr, value);
}

_CALLCONV void stm_store_u16(volatile uint16_t *addr, uint16_t value)
{
  int_stm_store_u16(addr, value);
}

_CALLCONV void stm_store_u32(volatile uint32_t *addr, uint32_t value)
{
  int_stm_store_u32(addr, value);
}

_CALLCONV void stm_store_u64(volatile uint64_t *addr, uint64_t value)
{
  int_stm_store_u64(addr, value);
}

_CALLCONV void stm_store_char(volatile char *addr, char value)
{
  convert_8_t val;
  val.s8 = value;
  int_stm_store_u8((volatile uint8_t *)addr, val.u8);
}

_CALLCONV void stm_store_uchar(volatile unsigned char *addr, unsigned char value)
{
  int_stm_store_u8((volatile uint8_t *)addr, (uint8_t)value);
}

_CALLCONV void stm_store_short(volatile short *addr, short value)
{
  convert_16_t val;
  val.s16 = value;
  int_stm_store_u16((volatile uint16_t *)addr, val.u16);
}

_CALLCONV void stm_store_ushort(volatile unsigned short *addr, unsigned short value)
{
  int_stm_store_u16((volatile uint16_t *)addr, (uint16_t)value);
}

_CALLCONV void stm_store_int(volatile int *addr, int value)
{
  convert_32_t val;
  val.s32 = value;
  int_stm_store_u32((volatile uint32_t *)addr, val.u32);
}

_CALLCONV void stm_store_uint(volatile unsigned int *addr, unsigned int value)
{
  int_stm_store_u32((volatile uint32_t *)addr, (uint32_t)value);
}

_CALLCONV void stm_store_long(volatile long *addr, long value)
{
  if (sizeof(long) == 4) {
    convert_32_t val;
    val.s32 = value;
    int_stm_store_u32((volatile uint32_t *)addr, val.u32);
  } else {
    convert_64_t val;
    val.s64 = value;
    int_stm_store_u64((volatile uint64_t *)addr, val.u64);
  }
}

_CALLCONV void stm_store_ulong(volatile unsigned long *addr, unsigned long value)
{
  if (sizeof(long) == 4) {
    int_stm_store_u32((volatile uint32_t *)addr, (uint32_t)value);
  } else {
    int_stm_store_u64((volatile uint64_t *)addr, (uint64_t)value);
  }
}

_CALLCONV void stm_store_float(volatile float *addr, float value)
{
  convert_32_t val;
  val.f = value;
  int_stm_store_u32((volatile uint32_t *)addr, val.u32);
}

_CALLCONV void stm_store_double(volatile double *addr, double value)
{
  convert_64_t val;
  val.d = value;
  int_stm_store_u64((volatile uint64_t *)addr, val.u64);
}

_CALLCONV void stm_store_ptr(volatile void **addr, void *value)
{
  union { stm_word_t w; void *v; } convert;
  convert.v = value;
  TM_STORE((stm_word_t *)addr, convert.w);
}

_CALLCONV void stm_store_bytes(volatile uint8_t *addr, uint8_t *buf, size_t size)
{
  convert_t val, mask;
  unsigned int i;
  stm_word_t *a;

  if (size == 0)
    return;
  i = (uintptr_t)addr & (sizeof(stm_word_t) - 1);
  if (i != 0) {
    /* First bytes */
    a = (stm_word_t *)((uintptr_t)addr & ~(uintptr_t)(sizeof(stm_word_t) - 1));
    val.w = mask.w = 0;
    for (; i < sizeof(stm_word_t) && size > 0; i++, size--) {
      mask.b[i] = 0xFF;
      val.b[i] = *buf++;
    }
    TM_STORE2(a++, val.w, mask.w);
  } else
    a = (stm_word_t *)addr;
  /* Full words */
  while (size >= sizeof(stm_word_t)) {
#ifdef ALLOW_MISALIGNED_ACCESSES
    TM_STORE(a++, *((stm_word_t *)buf));
    buf += sizeof(stm_word_t);
#else /* ! ALLOW_MISALIGNED_ACCESSES */
    for (i = 0; i < sizeof(stm_word_t); i++)
      val.b[i] = *buf++;
    TM_STORE(a++, val.w);
#endif /* ! ALLOW_MISALIGNED_ACCESSES */
    size -= sizeof(stm_word_t);
  }
  if (size > 0) {
    /* Last bytes */
    val.w = mask.w = 0;
    for (i = 0; size > 0; i++, size--) {
      mask.b[i] = 0xFF;
      val.b[i] = *buf++;
    }
    TM_STORE2(a, val.w, mask.w);
  }
}

_CALLCONV void stm_set_bytes(volatile uint8_t *addr, uint8_t byte, size_t count)
{
  convert_t val, mask;
  unsigned int i;
  stm_word_t *a;

  if (count == 0)
    return;

  for (i = 0; i < sizeof(stm_word_t); i++)
    val.b[i] = byte;

  i = (uintptr_t)addr & (sizeof(stm_word_t) - 1);
  if (i != 0) {
    /* First bytes */
    a = (stm_word_t *)((uintptr_t)addr & ~(uintptr_t)(sizeof(stm_word_t) - 1));
    mask.w = 0;
    for (; i < sizeof(stm_word_t) && count > 0; i++, count--)
      mask.b[i] = 0xFF;
    TM_STORE2(a++, val.w, mask.w);
  } else
    a = (stm_word_t *)addr;
  /* Full words */
  while (count >= sizeof(stm_word_t)) {
    TM_STORE(a++, val.w);
    count -= sizeof(stm_word_t);
  }
  if (count > 0) {
    /* Last bytes */
    mask.w = 0;
    for (i = 0; count > 0; i++, count--)
      mask.b[i] = 0xFF;
    TM_STORE2(a, val.w, mask.w);
  }
}

#undef TM_LOAD
#undef TM_STORE
#undef TM_STORE2

