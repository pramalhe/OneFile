/* Copyright (C) 2007-2009  Torvald Riegel
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
 */
/**
 * Internal STM interface. Contains the interface that an STM has to implement
 * for Tanger. The part of the interface that is visible in the application is
 * in tanger-stm.h.
 */
#ifndef TANGERSTMINTERNAL_H_
#define TANGERSTMINTERNAL_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** A transaction descriptor/handle/... */
typedef void tanger_stm_tx_t;

#ifndef TANGER_LOADSTORE_ATTR
/* FIXME: LLVM seems to have a bug when inlining these functions. Check if fixed with newer version. */
//#define TANGER_LOADSTORE_ATTR __attribute__((nothrow,always_inline))
#define TANGER_LOADSTORE_ATTR __attribute__((nothrow,noinline))
#endif

#if defined(__i386__)
/* XXX This is not supported by LLVM yet. */
#define ITM_REGPARM __attribute__((regparm(2)))
#else
#define ITM_REGPARM
#endif

/* Load and store functions access a certain number of bits.
 * 1b loads/stores are currently assumed to actually go to full 8 bits.
 * Addresses being accessed are not necessarily aligned (e.g., a 16b load
 * might target memory address 1). If it is known at compile time that the
 * access is aligned, then different functions are called.
 */
uint8_t tanger_stm_load1(tanger_stm_tx_t* tx, uint8_t *addr) TANGER_LOADSTORE_ATTR;
uint8_t tanger_stm_load8(tanger_stm_tx_t* tx, uint8_t *addr) TANGER_LOADSTORE_ATTR;
uint16_t tanger_stm_load16(tanger_stm_tx_t* tx, uint16_t *addr) TANGER_LOADSTORE_ATTR;
uint32_t tanger_stm_load32(tanger_stm_tx_t* tx, uint32_t *addr) TANGER_LOADSTORE_ATTR;
uint64_t tanger_stm_load64(tanger_stm_tx_t* tx, uint64_t *addr) TANGER_LOADSTORE_ATTR;
uint16_t tanger_stm_load16aligned(tanger_stm_tx_t* tx, uint16_t *addr) TANGER_LOADSTORE_ATTR;
uint32_t tanger_stm_load32aligned(tanger_stm_tx_t* tx, uint32_t *addr) TANGER_LOADSTORE_ATTR;
uint64_t tanger_stm_load64aligned(tanger_stm_tx_t* tx, uint64_t *addr) TANGER_LOADSTORE_ATTR;

/** Loads a number of bytes from src and copies them to dest
 * src is shared data, dest must point to thread-private data */
void tanger_stm_loadregion(tanger_stm_tx_t* tx, uint8_t *src, uintptr_t bytes, uint8_t *dest);
/** Starts reading a number of bytes from addr.
 * Mostly useful for creating wrappers for library functions. Use with care!
 * The function returns an address that you can read the data from. Depending
 * on the STM algorithm, it might be different from addr or not.
 * You must call tanger_stm_loadregionpost after calling this function.
 * You must not call any other STM function between the pre and post calls.
 * Data read from addr between the two calls is not guaranteed to be a
 * consistent snapshot. */
void* tanger_stm_loadregionpre(tanger_stm_tx_t* tx, uint8_t *addr, uintptr_t bytes);
/** See tanger_stm_loadregionpre */
void tanger_stm_loadregionpost(tanger_stm_tx_t* tx, uint8_t *addr, uintptr_t bytes);
/** This is like tanger_stm_loadregionpre() except that it assumes that addr
 * points to a zero-terminated string and thus reads all bytes up to and
 * including the final zero. It returns the size of the string (including the
 * terminating zero) in bytes. The size is guaranteed to be derived from a
 * consistent snapshot. If you read the strings' contents, you must call
 * tanger_stm_loadregionpost() afterwards.
 */
void* tanger_stm_loadregionstring(tanger_stm_tx_t* tx, char *addr, uintptr_t *bytes);

void tanger_stm_store1(tanger_stm_tx_t* tx, uint8_t *addr, uint8_t value) TANGER_LOADSTORE_ATTR;
void tanger_stm_store8(tanger_stm_tx_t* tx, uint8_t *addr, uint8_t value) TANGER_LOADSTORE_ATTR;
void tanger_stm_store16(tanger_stm_tx_t* tx, uint16_t *addr, uint16_t value) TANGER_LOADSTORE_ATTR;
void tanger_stm_store32(tanger_stm_tx_t* tx, uint32_t *addr, uint32_t value) TANGER_LOADSTORE_ATTR;
void tanger_stm_store64(tanger_stm_tx_t* tx, uint64_t *addr, uint64_t value) TANGER_LOADSTORE_ATTR;
void tanger_stm_store16aligned(tanger_stm_tx_t* tx, uint16_t *addr, uint16_t value) TANGER_LOADSTORE_ATTR;
void tanger_stm_store32aligned(tanger_stm_tx_t* tx, uint32_t *addr, uint32_t value) TANGER_LOADSTORE_ATTR;
void tanger_stm_store64aligned(tanger_stm_tx_t* tx, uint64_t *addr, uint64_t value) TANGER_LOADSTORE_ATTR;
/** Reads a number of bytes from src and writes them to dest
 * dest is shared data, src must point to thread-private data */
void tanger_stm_storeregion(tanger_stm_tx_t* tx, uint8_t *src, uintptr_t bytes, uint8_t *dest);
/** Prepares writing a number of bytes to addr.
 * The function returns an address that you can write the data to. Depending
 * on the STM algorithm, it might be different from addr or not.
 * The memory starting at addr does not necessarily contain a consistent
 * snapshot or the data previously located at this memory region. */
void* tanger_stm_storeregionpre(tanger_stm_tx_t* tx, uint8_t *addr, uintptr_t bytes);
/** Prepares updating a number of bytes starting at addr.
 * The function returns an address that you can write the data to. Depending
 * on the STM algorithm, it might be different from addr or not.
 * The memory starting at the returned address will contain a consistent
 * snapshot of the previous values of the region. */
void* tanger_stm_updateregionpre(tanger_stm_tx_t* tx, uint8_t *addr, uintptr_t bytes);


/**
 * Returns the calling thread's transaction descriptor.
 * ABI note: Remove this once we have efficient TLS.
 */
tanger_stm_tx_t* tanger_stm_get_tx(void);

/**
 * Saves or restores the stack, depending on whether the current txn was
 * started or restarted. The STM will save/restore everything in the range
 * [low_addr, high_addr). The STM's implementation of this function must be
 * marked as no-inline, so it will get a new stack frame that does not
 * overlap the [low_addr, high_addr) region.
 * To avoid corrupting stack space of rollback functions, the STM should skip
 * undoing changes to addresses that are between the current stack pointer
 * during execution of the undo function and the [low_addr, high_addr)
 * area (i.e., all newer stack frames, including the current one).
 */
void tanger_stm_save_restore_stack(void* low_addr, void* high_addr);

/**
 * Replacement function for malloc calls in transactions.
 */
void *tanger_stm_malloc(size_t size);

/**
 * Replacement function for free calls in transactions.
 */
void tanger_stm_free(void *ptr);

/**
 * Replacement function for calloc calls in transactions.
 */
void *tanger_stm_calloc(size_t nmemb, size_t size);

/**
 * Replacement function for realloc calls in transactions.
 */
void *tanger_stm_realloc(void *ptr, size_t size);

/**
 * Returns the transactional version of the function passed as argument.
 * If no transactional version has been registered, it aborts.
 */
void* tanger_stm_indirect_resolve(void *nontxnal_function);

/**
 * Called before transactional versions are registered for nontransactional
 * functions.
 * The parameter returns the exact number of functions that will be registered.
 */
void tanger_stm_indirect_init(uint32_t number_of_call_targets);

/**
 * Registers a transactional versions for a nontransactional function.
 */
void tanger_stm_indirect_register(void* nontxnal, void* txnal);

/* ABI: Additionnal declarations */
void tanger_stm_stack_restorehack();
void tanger_stm_stack_savehack();
//void tanger_stm_threadstack_init();
//void tanger_stm_threadstack_fini();

#ifdef __cplusplus
}
#endif

#endif /*TANGERSTMINTERNAL_H_*/
