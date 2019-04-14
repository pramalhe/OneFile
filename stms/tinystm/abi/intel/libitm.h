/*
 * File:
 *   libitm.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   ABI for tinySTM.
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

#ifndef _LIBITM_H_
#define _LIBITM_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <stdbool.h>
#ifdef __SSE__
# include <xmmintrin.h>
#endif

/* ################################################################### *
 * DEFINES
 * ################################################################### */
#define _ITM_VERSION_NO_STR "1.0.4"
#define _ITM_VERSION_NO 104

#if defined(__i386__)
# define _ITM_CALL_CONVENTION __attribute__((regparm(2)))
#else
# define _ITM_CALL_CONVENTION
#endif

#define _ITM_noTransactionId 1		/* Id for non-transactional code. */


#define _ITM_TRANSACTION_PURE __attribute__((tm_pure))

/* ################################################################### *
 * TYPES
 * ################################################################### */

typedef void *_ITM_transaction;

typedef void (*_ITM_userUndoFunction)(void *);
typedef void (*_ITM_userCommitFunction)(void *);

typedef uint32_t _ITM_transactionId;

typedef enum
{
  outsideTransaction = 0,
  inRetryableTransaction,
  inIrrevocableTransaction
} _ITM_howExecuting;

struct _ITM_srcLocationS
{
  int32_t reserved_1;
  int32_t flags;
  int32_t reserved_2;
  int32_t reserved_3;
  const char *psource;
};

typedef struct _ITM_srcLocationS _ITM_srcLocation;

typedef enum {
  pr_instrumentedCode = 0x0001,
  pr_uninstrumentedCode = 0x0002,
  pr_multiwayCode = pr_instrumentedCode | pr_uninstrumentedCode,
  pr_hasNoXMMUpdate = 0x0004,
  pr_hasNoAbort = 0x0008,
  pr_hasNoRetry = 0x0010,
  pr_hasNoIrrevocable = 0x0020,
  pr_doesGoIrrevocable = 0x0040,
  pr_hasNoSimpleReads = 0x0080,
  pr_aWBarriersOmitted = 0x0100,
  pr_RaRBarriersOmitted = 0x0200,
  pr_undoLogCode = 0x0400,
  pr_preferUninstrumented = 0x0800,
  pr_exceptionBlock = 0x1000,
  pr_hasElse = 0x2000,
  pr_readOnly = 0x4000 /* GNU gcc specific */
} _ITM_codeProperties;

typedef enum {
  a_runInstrumentedCode = 0x01,
  a_runUninstrumentedCode = 0x02,
  a_saveLiveVariables = 0x04,
  a_restoreLiveVariables = 0x08,
  a_abortTransaction = 0x10,
} _ITM_actions;

typedef enum {
  modeSerialIrrevocable,
  modeObstinate,
  modeOptimistic,
  modePessimistic,
} _ITM_transactionState;

typedef enum {
  unknown = 0,
  userAbort = 1,
  userRetry = 2,
  TMConflict= 4,
  exceptionBlockAbort = 8
} _ITM_abortReason;


/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

extern _ITM_TRANSACTION_PURE
_ITM_transaction * _ITM_CALL_CONVENTION _ITM_getTransaction(void);

extern _ITM_TRANSACTION_PURE
_ITM_howExecuting _ITM_CALL_CONVENTION _ITM_inTransaction(_ITM_transaction *);

extern _ITM_TRANSACTION_PURE
int _ITM_CALL_CONVENTION _ITM_getThreadnum(void);

extern _ITM_TRANSACTION_PURE
void _ITM_CALL_CONVENTION _ITM_addUserCommitAction(_ITM_transaction *, 
                             _ITM_userCommitFunction __commit,
                             _ITM_transactionId resumingTransactionId,
                             void *__arg);

extern _ITM_TRANSACTION_PURE
void _ITM_CALL_CONVENTION _ITM_addUserUndoAction(_ITM_transaction *, 
                             const _ITM_userUndoFunction __undo, void * __arg);

extern _ITM_TRANSACTION_PURE
_ITM_transactionId _ITM_CALL_CONVENTION _ITM_getTransactionId(_ITM_transaction *);

extern _ITM_TRANSACTION_PURE
void _ITM_CALL_CONVENTION _ITM_dropReferences(_ITM_transaction *, 
                             const void *__start, size_t __size);

extern _ITM_TRANSACTION_PURE
void _ITM_CALL_CONVENTION _ITM_userError(const char *errString, int exitCode);

extern const char * _ITM_CALL_CONVENTION _ITM_libraryVersion(void);

extern int _ITM_CALL_CONVENTION _ITM_versionCompatible(int version);


extern int _ITM_CALL_CONVENTION _ITM_initializeThread(void);

extern void _ITM_CALL_CONVENTION _ITM_finalizeThread(void);

extern void _ITM_CALL_CONVENTION _ITM_finalizeProcess(void);

extern int _ITM_CALL_CONVENTION _ITM_initializeProcess(void);

extern void _ITM_CALL_CONVENTION _ITM_error(const _ITM_srcLocation *__src,
                             int errorCode);

extern uint32_t _ITM_CALL_CONVENTION _ITM_beginTransaction(_ITM_transaction *, 
                             uint32_t __properties,
                             const _ITM_srcLocation *__src)
                             __attribute__((returns_twice));

extern void _ITM_CALL_CONVENTION _ITM_commitTransaction(_ITM_transaction *, 
                             const _ITM_srcLocation *__src);


extern bool _ITM_CALL_CONVENTION _ITM_tryCommitTransaction(_ITM_transaction *, 
                             const _ITM_srcLocation *__src);

extern void _ITM_CALL_CONVENTION _ITM_commitTransactionToId(_ITM_transaction *, 
                             const _ITM_transactionId tid,
                             const _ITM_srcLocation *__src);

extern void _ITM_CALL_CONVENTION _ITM_abortTransaction(_ITM_transaction *, 
                             _ITM_abortReason __reason,
                             const _ITM_srcLocation *__src);

extern void _ITM_CALL_CONVENTION _ITM_rollbackTransaction(_ITM_transaction *, 
                             const _ITM_srcLocation *__src);

extern void _ITM_CALL_CONVENTION _ITM_registerThrownObject(_ITM_transaction *, 
                             const void *__obj,
                             size_t __size);

extern void _ITM_CALL_CONVENTION _ITM_changeTransactionMode(_ITM_transaction *, 
                             _ITM_transactionState __mode,
                             const _ITM_srcLocation *__loc);



/*** Loads ***/

extern uint8_t _ITM_CALL_CONVENTION _ITM_RU1(_ITM_transaction *, const uint8_t *);
extern uint8_t _ITM_CALL_CONVENTION _ITM_RaRU1(_ITM_transaction *, const uint8_t *);
extern uint8_t _ITM_CALL_CONVENTION _ITM_RaWU1(_ITM_transaction *, const uint8_t *);
extern uint8_t _ITM_CALL_CONVENTION _ITM_RfWU1(_ITM_transaction *, const uint8_t *);

extern uint16_t _ITM_CALL_CONVENTION _ITM_RU2(_ITM_transaction *, const uint16_t *);
extern uint16_t _ITM_CALL_CONVENTION _ITM_RaRU2(_ITM_transaction *, const uint16_t *);
extern uint16_t _ITM_CALL_CONVENTION _ITM_RaWU2(_ITM_transaction *, const uint16_t *);
extern uint16_t _ITM_CALL_CONVENTION _ITM_RfWU2(_ITM_transaction *, const uint16_t *);

extern uint32_t _ITM_CALL_CONVENTION _ITM_RU4(_ITM_transaction *, const uint32_t *);
extern uint32_t _ITM_CALL_CONVENTION _ITM_RaRU4(_ITM_transaction *, const uint32_t *);
extern uint32_t _ITM_CALL_CONVENTION _ITM_RaWU4(_ITM_transaction *, const uint32_t *);
extern uint32_t _ITM_CALL_CONVENTION _ITM_RfWU4(_ITM_transaction *, const uint32_t *);

extern uint64_t _ITM_CALL_CONVENTION _ITM_RU8(_ITM_transaction *, const uint64_t *);
extern uint64_t _ITM_CALL_CONVENTION _ITM_RaRU8(_ITM_transaction *, const uint64_t *);
extern uint64_t _ITM_CALL_CONVENTION _ITM_RaWU8(_ITM_transaction *, const uint64_t *);
extern uint64_t _ITM_CALL_CONVENTION _ITM_RfWU8(_ITM_transaction *, const uint64_t *);

extern float _ITM_CALL_CONVENTION _ITM_RF(_ITM_transaction *, const float *);
extern float _ITM_CALL_CONVENTION _ITM_RaRF(_ITM_transaction *, const float *);
extern float _ITM_CALL_CONVENTION _ITM_RaWF(_ITM_transaction *, const float *);
extern float _ITM_CALL_CONVENTION _ITM_RfWF(_ITM_transaction *, const float *);

extern double _ITM_CALL_CONVENTION _ITM_RD(_ITM_transaction *, const double *);
extern double _ITM_CALL_CONVENTION _ITM_RaRD(_ITM_transaction *, const double *);
extern double _ITM_CALL_CONVENTION _ITM_RaWD(_ITM_transaction *, const double *);
extern double _ITM_CALL_CONVENTION _ITM_RfWD(_ITM_transaction *, const double *);

#ifdef __SSE__
extern __m64 _ITM_CALL_CONVENTION _ITM_RM64(_ITM_transaction *, const __m64 *);
extern __m64 _ITM_CALL_CONVENTION _ITM_RaRM64(_ITM_transaction *, const __m64 *);
extern __m64 _ITM_CALL_CONVENTION _ITM_RaWM64(_ITM_transaction *, const __m64 *);
extern __m64 _ITM_CALL_CONVENTION _ITM_RfWM64(_ITM_transaction *, const __m64 *);

extern __m128 _ITM_CALL_CONVENTION _ITM_RM128(_ITM_transaction *, const __m128 *);
extern __m128 _ITM_CALL_CONVENTION _ITM_RaRM128(_ITM_transaction *, const __m128 *);
extern __m128 _ITM_CALL_CONVENTION _ITM_RaWM128(_ITM_transaction *, const __m128 *);
extern __m128 _ITM_CALL_CONVENTION _ITM_RfWM128(_ITM_transaction *, const __m128 *);
#endif /* __SSE__ */

extern float _Complex _ITM_CALL_CONVENTION _ITM_RCF(_ITM_transaction *, const float _Complex *);
extern float _Complex _ITM_CALL_CONVENTION _ITM_RaRCF(_ITM_transaction *, const float _Complex *);
extern float _Complex _ITM_CALL_CONVENTION _ITM_RaWCF(_ITM_transaction *, const float _Complex *);
extern float _Complex _ITM_CALL_CONVENTION _ITM_RfWCF(_ITM_transaction *, const float _Complex *);

extern double _Complex _ITM_CALL_CONVENTION _ITM_RCD(_ITM_transaction *, const double _Complex *);
extern double _Complex _ITM_CALL_CONVENTION _ITM_RaRCD(_ITM_transaction *, const double _Complex *);
extern double _Complex _ITM_CALL_CONVENTION _ITM_RaWCD(_ITM_transaction *, const double _Complex *);
extern double _Complex _ITM_CALL_CONVENTION _ITM_RfWCD(_ITM_transaction *, const double _Complex *);

extern long double _Complex _ITM_CALL_CONVENTION _ITM_RCE(_ITM_transaction *, const long double _Complex *);
extern long double _Complex _ITM_CALL_CONVENTION _ITM_RaRCE(_ITM_transaction *, const long double _Complex *);
extern long double _Complex _ITM_CALL_CONVENTION _ITM_RaWCE(_ITM_transaction *, const long double _Complex *);
extern long double _Complex _ITM_CALL_CONVENTION _ITM_RfWCE(_ITM_transaction *, const long double _Complex *);


/*** Stores ***/

extern void _ITM_CALL_CONVENTION _ITM_WU1(_ITM_transaction *, const uint8_t *, uint8_t);
extern void _ITM_CALL_CONVENTION _ITM_WaRU1(_ITM_transaction *, const uint8_t *, uint8_t);
extern void _ITM_CALL_CONVENTION _ITM_WaWU1(_ITM_transaction *, const uint8_t *, uint8_t);

extern void _ITM_CALL_CONVENTION _ITM_WU2(_ITM_transaction *, const uint16_t *, uint16_t);
extern void _ITM_CALL_CONVENTION _ITM_WaRU2(_ITM_transaction *, const uint16_t *, uint16_t);
extern void _ITM_CALL_CONVENTION _ITM_WaWU2(_ITM_transaction *, const uint16_t *, uint16_t);

extern void _ITM_CALL_CONVENTION _ITM_WU4(_ITM_transaction *, const uint32_t *, uint32_t);
extern void _ITM_CALL_CONVENTION _ITM_WaRU4(_ITM_transaction *, const uint32_t *, uint32_t);
extern void _ITM_CALL_CONVENTION _ITM_WaWU4(_ITM_transaction *, const uint32_t *, uint32_t);

extern void _ITM_CALL_CONVENTION _ITM_WU8(_ITM_transaction *, const uint64_t *, uint64_t);
extern void _ITM_CALL_CONVENTION _ITM_WaRU8(_ITM_transaction *, const uint64_t *, uint64_t);
extern void _ITM_CALL_CONVENTION _ITM_WaWU8(_ITM_transaction *, const uint64_t *, uint64_t);

extern void _ITM_CALL_CONVENTION _ITM_WF(_ITM_transaction *, const float *, float);
extern void _ITM_CALL_CONVENTION _ITM_WaRF(_ITM_transaction *, const float *, float);
extern void _ITM_CALL_CONVENTION _ITM_WaWF(_ITM_transaction *, const float *, float);

extern void _ITM_CALL_CONVENTION _ITM_WD(_ITM_transaction *, const double *, double);
extern void _ITM_CALL_CONVENTION _ITM_WaRD(_ITM_transaction *, const double *, double);
extern void _ITM_CALL_CONVENTION _ITM_WaWD(_ITM_transaction *, const double *, double);

#ifdef __SSE__
extern void _ITM_CALL_CONVENTION _ITM_WM64(_ITM_transaction *, const __m64 *, __m64);
extern void _ITM_CALL_CONVENTION _ITM_WaRM64(_ITM_transaction *, const __m64 *, __m64);
extern void _ITM_CALL_CONVENTION _ITM_WaWM64(_ITM_transaction *, const __m64 *, __m64);

extern void _ITM_CALL_CONVENTION _ITM_WM128(_ITM_transaction *, const __m128 *, __m128);
extern void _ITM_CALL_CONVENTION _ITM_WaRM128(_ITM_transaction *, const __m128 *, __m128);
extern void _ITM_CALL_CONVENTION _ITM_WaWM128(_ITM_transaction *, const __m128 *, __m128);
#endif /* __SSE__ */

extern void _ITM_CALL_CONVENTION _ITM_WCF(_ITM_transaction *, const float _Complex *, float _Complex);
extern void _ITM_CALL_CONVENTION _ITM_WaRCF(_ITM_transaction *, const float _Complex *, float _Complex);
extern void _ITM_CALL_CONVENTION _ITM_WaWCF(_ITM_transaction *, const float _Complex *, float _Complex);

extern void _ITM_CALL_CONVENTION _ITM_WCD(_ITM_transaction *, const double _Complex *, double _Complex);
extern void _ITM_CALL_CONVENTION _ITM_WaRCD(_ITM_transaction *, const double _Complex *, double _Complex);
extern void _ITM_CALL_CONVENTION _ITM_WaWCD(_ITM_transaction *, const double _Complex *, double _Complex);

extern void _ITM_CALL_CONVENTION _ITM_WCE(_ITM_transaction *, const long double _Complex *, long double _Complex);
extern void _ITM_CALL_CONVENTION _ITM_WaRCE(_ITM_transaction *, const long double _Complex *, long double _Complex);
extern void _ITM_CALL_CONVENTION _ITM_WaWCE(_ITM_transaction *, const long double _Complex *, long double _Complex);


/*** Logging functions ***/

extern void _ITM_CALL_CONVENTION _ITM_LU1(_ITM_transaction *, const uint8_t *);
extern void _ITM_CALL_CONVENTION _ITM_LU2(_ITM_transaction *, const uint16_t *);
extern void _ITM_CALL_CONVENTION _ITM_LU4(_ITM_transaction *, const uint32_t *);
extern void _ITM_CALL_CONVENTION _ITM_LU8(_ITM_transaction *, const uint64_t *);
extern void _ITM_CALL_CONVENTION _ITM_LF(_ITM_transaction *, const float *);
extern void _ITM_CALL_CONVENTION _ITM_LD(_ITM_transaction *, const double *);
extern void _ITM_CALL_CONVENTION _ITM_LE(_ITM_transaction *, const long double *);
extern void _ITM_CALL_CONVENTION _ITM_LM64(_ITM_transaction *, const __m64 *);
extern void _ITM_CALL_CONVENTION _ITM_LM128(_ITM_transaction *, const __m128 *);
extern void _ITM_CALL_CONVENTION _ITM_LCF(_ITM_transaction *, const float _Complex *);
extern void _ITM_CALL_CONVENTION _ITM_LCD(_ITM_transaction *, const double _Complex *);
extern void _ITM_CALL_CONVENTION _ITM_LCE(_ITM_transaction *, const long double _Complex *);
extern void _ITM_CALL_CONVENTION _ITM_LB(_ITM_transaction *, const void *, size_t);


/*** memcpy functions ***/

extern void _ITM_CALL_CONVENTION _ITM_memcpyRnWt(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRnWtaR(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRnWtaW(_ITM_transaction *, void *, const void *, size_t);

extern void _ITM_CALL_CONVENTION _ITM_memcpyRtWn(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtaRWn(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtaWWn(_ITM_transaction *, void *, const void *, size_t);

extern void _ITM_CALL_CONVENTION _ITM_memcpyRtWt(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtWtaR(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtWtaW(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtaRWt(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtaRWtaR(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtaRWtaW(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtaWWt(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtaWWtaR(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memcpyRtaWWtaW(_ITM_transaction *, void *, const void *, size_t);


/*** memset functions ***/

extern void _ITM_CALL_CONVENTION _ITM_memsetW(_ITM_transaction *, void *, int, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memsetWaR(_ITM_transaction *, void *, int, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memsetWaW(_ITM_transaction *, void *, int, size_t);


/*** memmove functions ***/

extern void _ITM_CALL_CONVENTION _ITM_memmoveRnWt(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRnWtaR(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRnWtaW(_ITM_transaction *, void *, const void *, size_t);

extern void _ITM_CALL_CONVENTION _ITM_memmoveRtWn(_ITM_transaction *, void *, const void *, size_t); 
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtaRWn(_ITM_transaction *, void *, const void *, size_t); 
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtaWWn(_ITM_transaction *, void *, const void *, size_t); 

extern void _ITM_CALL_CONVENTION _ITM_memmoveRtWt(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtWtaR(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtWtaW(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtaRWt(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtaRWtaR(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtaRWtaW(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtaWWt(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtaWWtaR(_ITM_transaction *, void *, const void *, size_t);
extern void _ITM_CALL_CONVENTION _ITM_memmoveRtaWWtaW(_ITM_transaction *, void *, const void *, size_t);


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* _LIBITM_H_ */

