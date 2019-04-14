/*
 * File:
 *   abi.c
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

#define _GNU_SOURCE
#include <alloca.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#ifdef __SSE__
# include <xmmintrin.h>
#endif /* __SSE__ */

#include "libitm.h"
#include "utils.h"

/* FIXME STACK_CHECK: DTMC and GCC can use ITM_W to write on stack variable */
/* TODO STACK_CHECK: to finish and test */

#include "stm.h"
#include "atomic.h"
#include "mod_cb.h"
#include "mod_mem.h"
#include "mod_log.h"
#include "wrappers.h"
#ifdef TM_DTMC
# include "dtmc/tanger-stm-internal.h"
# include "dtmc/tanger.h"
#endif

/* Indicates to use _ITM_siglongjmp */
#ifdef TM_DTMC
# define CTX_ITM   tanger_stm_restore_stack(); _ITM_siglongjmp
#else
# define CTX_ITM   _ITM_siglongjmp
#endif /* ! TM_DTMC */
extern void _ITM_CALL_CONVENTION _ITM_siglongjmp(int val, sigjmp_buf env) __attribute__ ((noreturn));

#include "stm.c"
#include "mod_cb_mem.c"
#ifdef TM_GCC
# include "gcc/alloc_cpp.c"
#endif
#include "mod_log.c"
#include "wrappers.c"
#ifdef EPOCH_GC
# include "gc.c"
#endif

/* pthread wrapper */
#ifdef PTHREAD_WRAPPER
# include "pthread_wrapper.h"
#endif /* PTHREAD_WRAPPER */

#if defined(TM_GCC) || defined(TM_DTMC)
# define TX_ARG
# define TX_ARGS
# define TX_GET_ABI   stm_tx_t *tx = tls_get_tx()
#elif defined(TM_INTEL)
# define TX_ARG       _ITM_transaction *__td
# define TX_ARGS      _ITM_transaction *__td,
# define TX_GET_ABI   stm_tx_t *tx = (stm_tx_t *)__td
#else
# error "No ABI defined"
#endif

/* ################################################################### *
 * VARIABLES
 * ################################################################### */
/* Status of the ABI */
enum {
  ABI_NOT_INITIALIZED,
  ABI_INITIALIZING,
  ABI_INITIALIZED,
  ABI_FINALIZING,
};

static union {
  struct {
    volatile unsigned long status;
    volatile unsigned long thread_counter;
  };
  uint8_t padding[64]; /* TODO should be cacheline related */
} __attribute__((aligned(64))) global_abi = {{.status = ABI_NOT_INITIALIZED, .thread_counter = 0 }};


typedef struct {
  int thread_id;
#ifdef STACK_CHECK
/* TODO STACK_CHECK could be moved to stm.c and we could calculate a mask to detect if the address is in the stack */
  void *stack_addr_low;
  void *stack_addr_high;
#endif /* STACK_CHECK */
} thread_abi_t;

/* Statistics */
typedef struct stats {
  int thread_id;
  unsigned long nb_commits;
  unsigned long nb_aborts;
  double nb_retries_avg;
  unsigned long nb_retries_min;
  unsigned long nb_retries_max;

  struct stats * next;
} stats_t;

/* Thread statistics managed as a linked list */
/* TODO align + padding */
stats_t * thread_stats = NULL;


/* ################################################################### *
 * COMPATIBILITY FUNCTIONS
 * ################################################################### */
#ifdef STACK_CHECK
static int get_stack_attr(void *low, void *high)
{
  /* GNU Pthread specific */
  pthread_attr_t attr;
  uintptr_t stackaddr;
  size_t stacksize;
  if (pthread_getattr_np(pthread_self(), &attr)) {
    return 1;
  }
  if (pthread_attr_getstack(&attr, (void *)&stackaddr, &stacksize)) {
    return 1;
  }
  *(uintptr_t *)low = stackaddr;
  *(uintptr_t *)high = stackaddr + stacksize;

  return 0;
}
/* Hints for other platforms
#if PLATFORM(DARWIN) 
pthread_get_stackaddr_np(pthread_self()); 
#endif
#elif PLATFORM(WIN_OS) && PLATFORM(X86) && COMPILER(MSVC) 
// offset 0x18 from the FS segment register gives a pointer to 
// the thread information block for the current thread 
NT_TIB* pTib; 
__asm { 
MOV EAX, FS:[18h] 
MOV pTib, EAX 
} 
return (void*)pTib->StackBase; 
#elif PLATFORM(WIN_OS) && PLATFORM(X86_64) && COMPILER(MSVC) 
PNT_TIB64 pTib = reinterpret_cast<PNT_TIB64>(NtCurrentTeb()); 
return (void*)pTib->StackBase; 
#elif PLATFORM(WIN_OS) && PLATFORM(X86) && COMPILER(GCC) 
// offset 0x18 from the FS segment register gives a pointer to 
// the thread information block for the current thread 
NT_TIB* pTib; 
asm ( "movl %%fs:0x18, %0\n" 
: "=r" (pTib) 
); 
return (void*)pTib->StackBase; 
#endif
*/
static INLINE int on_stack(void *a)
{
#ifdef TLS
  thread_abi_t *t = thread_abi;
#else /* ! TLS */
  thread_abi_t *t = pthread_getspecific(thread_abi);
#endif /* ! TLS */
  if ((t->stack_addr_low <= (uintptr_t)a) && ((uintptr_t)a < t->stack_addr_high)) { 
    return 1;
  } 
  return 0;
}
#endif /* STACK_CHECK */


#if defined(__APPLE__)
/* OS X */
# include <malloc/malloc.h>
INLINE size_t block_size(void *ptr)
{
  return malloc_size(ptr);
}
#elif defined(__linux__) || defined(__CYGWIN__)
/* Linux, WIN32 (CYGWIN) */
# include <malloc.h>
INLINE size_t block_size(void *ptr)
{
  return malloc_usable_size(ptr);
}
#else /* ! (defined(__APPLE__) || defined(__linux__) || defined(__CYGWIN__)) */
# error "Target OS does not provide size of allocated blocks"
#endif /* ! (defined(__APPLE__) || defined(__linux__) || defined(__CYGWIN__)) */


/* ################################################################### *
 * 
 * ################################################################### */
static void abi_init(void);

static INLINE stm_tx_t *
abi_init_thread(void)
{
  stm_tx_t *tx = tls_get_tx();
  if (tx == NULL) {
    /* Make sure that the main initilization is done */
    if (ATOMIC_LOAD(&global_abi.status) != ABI_INITIALIZED)
      _ITM_initializeProcess();
    //t->thread_id = (int)ATOMIC_FETCH_INC_FULL(&global_abi.thread_counter);
    tx = stm_init_thread();
#ifdef STACK_CHECK
    get_stack_attr(&t->stack_addr_low, &t->stack_addr_high);
#endif /* STACK_CHECK */
  }
  return tx;
}

static void
abi_exit_thread(struct stm_tx *tx)
{
  if (tx == NULL)
    return;
#if 0
  /* FIXME disable during refactoring */
  if (getenv("ITM_STATISTICS") != NULL) {
    stats_t * ts = malloc(sizeof(stats_t));
#ifdef TLS
    thread_abi_t *t = thread_abi;
#else /* ! TLS */
    thread_abi_t *t = pthread_getspecific(thread_abi);
#endif /* ! TLS */
    ts->thread_id = t->thread_id;
    stm_get_local_stats("nb_commits", &ts->nb_commits);
    stm_get_local_stats("nb_aborts", &ts->nb_aborts);
    stm_get_local_stats("nb_retries_avg", &ts->nb_retries_avg);
    stm_get_local_stats("nb_retries_min", &ts->nb_retries_min);
    stm_get_local_stats("nb_retries_max", &ts->nb_retries_max);
    /* Register thread-statistics to global */
    do {
	ts->next = (stats_t *)ATOMIC_LOAD(&thread_stats);
    } while (ATOMIC_CAS_FULL(&thread_stats, ts->next, ts) == 0);
    /* ts will be freed on _ITM_finalizeProcess. */
#ifdef TLS
    thread_abi = NULL;
#else /* ! TLS */
    pthread_setspecific(thread_abi, NULL);
#endif
    /* Free thread_abi_t structure. */
    free(t);
  }
#endif

  stm_exit_thread();

#ifdef TM_DTMC
  /* Free the saved stack */
  tanger_stm_free_stack();
#endif
}

static INLINE void
abi_init(void)
{
  /* thread safe */
reload:
  if (ATOMIC_LOAD_ACQ(&global_abi.status) == ABI_NOT_INITIALIZED) {
    if (ATOMIC_CAS_FULL(&global_abi.status, ABI_NOT_INITIALIZED, ABI_INITIALIZING) != 0) {
      /* TODO temporary to be sure to use tinySTM */
      printf("TinySTM-ABI v%s.\n", _ITM_libraryVersion());
      atexit((void (*)(void))(_ITM_finalizeProcess));

      /* TinySTM initialization */
      stm_init();
      mod_mem_init(0);
# ifdef TM_GCC
      mod_alloc_cpp();
# endif /* TM_GCC */
      mod_log_init();
      mod_cb_init();
      ATOMIC_STORE(&global_abi.status, ABI_INITIALIZED);
      /* Also initialize thread as specify in the specification */
      abi_init_thread();
      return; 
    } else {
      goto reload;
    }
  } else if (ATOMIC_LOAD_ACQ(&global_abi.status) != ABI_INITIALIZED) {
    /* Wait the end of the initialization */
    goto reload;
  }

  return;
}

static INLINE void
abi_exit(void)
{
  TX_GET;
  char * statistics;

  abi_exit_thread(tx);

  /* Ensure thread safety */
reload:
  if (ATOMIC_LOAD_ACQ(&global_abi.status) == ABI_INITIALIZED) {
    if (ATOMIC_CAS_FULL(&global_abi.status, ABI_INITIALIZED, ABI_FINALIZING) == 0)
      goto reload;
  } else {
    return;
  }

  if ((statistics = getenv("ITM_STATISTICS")) != NULL) {
    FILE * f;
    int i = 0;
    stats_t * ts;
    if (statistics[0] == '-')
      f = stdout;
    else if ((f = fopen("itm.log", "w")) == NULL) {
      fprintf(stderr, "can't open itm.log for writing\n");
      goto finishing;
    }
    fprintf(f, "STATS REPORT\n");
    fprintf(f, "THREAD TOTALS\n");

    while (1) {
      do {
        ts = (stats_t *)ATOMIC_LOAD(&thread_stats);
	if (ts == NULL)
	  goto no_more_stat;
      } while(ATOMIC_CAS_FULL(&thread_stats, ts, ts->next) == 0);
      /* Skip stats if not a transactional thread */
      if (ts->nb_commits == 0)
        continue;
      fprintf(f, "Thread %-4i                : %12s %12s %12s %12s\n", i, "Min", "Mean", "Max", "Total");
      fprintf(f, "  Transactions             : %12lu\n", ts->nb_commits);
      fprintf(f, "  %-25s: %12lu %12.2f %12lu %12lu\n", "Retries", ts->nb_retries_min, ts->nb_retries_avg, ts->nb_retries_max, ts->nb_aborts);
      fprintf(f,"\n");
      /* Free the thread stats structure */
      free(ts);
      i++;
    }
no_more_stat:
    if (f != stdout) {
      fclose(f);
    }
  }
finishing:
  stm_exit();

  ATOMIC_STORE(&global_abi.status, ABI_NOT_INITIALIZED);
}

/* ################################################################### *
 * FUNCTIONS
 * ################################################################### */

_ITM_transaction * _ITM_CALL_CONVENTION _ITM_getTransaction(void)
{
  struct stm_tx *tx = tls_get_tx();
  if (unlikely(tx == NULL)) {
    /* Thread not initialized: must create transaction */
    tx = abi_init_thread();
  }

  return (_ITM_transaction *)tx;
}

_ITM_howExecuting _ITM_CALL_CONVENTION _ITM_inTransaction(TX_ARG)
{
  TX_GET_ABI;
  if (stm_irrevocable_tx(tx))
    return inIrrevocableTransaction;
  if (stm_active_tx(tx))
    return inRetryableTransaction;
  return outsideTransaction;
}

int _ITM_CALL_CONVENTION _ITM_getThreadnum(void)
{
  /* FIXME should be done in stm? */
  return (int)pthread_self();
}

void _ITM_CALL_CONVENTION _ITM_addUserCommitAction(TX_ARGS
                              _ITM_userCommitFunction __commit,
                              _ITM_transactionId resumingTransactionId,
                              void *__arg)
{
  stm_on_commit(__commit, __arg);
}

void _ITM_CALL_CONVENTION _ITM_addUserUndoAction(TX_ARGS
                            const _ITM_userUndoFunction __undo, void * __arg)
{
  stm_on_abort(__undo, __arg);
}

/*
 * Specification: The getTransactionId function returns a sequence number for
 * the current transaction. Within a transaction, nested transactions are 
 * numbered sequentially in the order in which they start, with the outermost
 * transaction getting the lowest number, and non-transactional code the value
 * _ITM_NoTransactionId, which is less than any transaction id for 
 * transactional code.
 */
_ITM_transactionId _ITM_CALL_CONVENTION _ITM_getTransactionId(TX_ARG)
{
  TX_GET_ABI;
  if (tx == NULL)
    return _ITM_noTransactionId;
  /* Note that _ITM_noTransactionId is 1 */
  return (_ITM_transactionId)(tx->nesting + 1);
}

void _ITM_CALL_CONVENTION _ITM_dropReferences(TX_ARGS const void *__start,
                                              size_t __size)
{
  fprintf(stderr, "%s: not yet implemented\n", __func__);
}

void _ITM_CALL_CONVENTION _ITM_userError(const char *errString, int exitCode)
{
  fprintf(stderr, "%s", errString);
  exit(exitCode);
}

const char * _ITM_CALL_CONVENTION _ITM_libraryVersion(void)
{
  return _ITM_VERSION_NO_STR " using TinySTM " STM_VERSION "";
}

int _ITM_CALL_CONVENTION _ITM_versionCompatible(int version)
{
  return version == _ITM_VERSION_NO;
}

int _ITM_CALL_CONVENTION _ITM_initializeThread(void)
{
  abi_init_thread();
  return 0;
}

void _ITM_CALL_CONVENTION _ITM_finalizeThread(void)
{
  TX_GET;
  abi_exit_thread(tx);
}

#ifdef __PIC__
/* Add call when the library is loaded and unloaded */
#define ATTR_CONSTRUCTOR __attribute__ ((constructor))  
#define ATTR_DESTRUCTOR __attribute__ ((destructor))  
#else
#define ATTR_CONSTRUCTOR 
#define ATTR_DESTRUCTOR 
#endif

void ATTR_DESTRUCTOR _ITM_CALL_CONVENTION _ITM_finalizeProcess(void)
{
  abi_exit();
}

int ATTR_CONSTRUCTOR _ITM_CALL_CONVENTION _ITM_initializeProcess(void)
{
  abi_init();
  return 0;
}

void _ITM_CALL_CONVENTION _ITM_error(const _ITM_srcLocation *__src, int errorCode)
{
  fprintf(stderr, "Error: %s (%d)\n", (__src == NULL || __src->psource == NULL ? "?" : __src->psource), errorCode);
  exit(1);
}

/* The _ITM_beginTransaction is defined in assembly (arch.S)  */
uint32_t _ITM_CALL_CONVENTION GTM_begin_transaction(TX_ARGS uint32_t attr, jmp_buf * buf) 
{
  /* FIXME first time return a_saveLiveVariable +> siglongjmp must return a_restoreLiveVariable (and set a_saveLiveVariable)
   *       check a_abortTransaction attr
   * */
  TX_GET_ABI;
  uint32_t ret;
  sigjmp_buf * env;

  /* This variable is in the stack but stm_start copies the content. */
  stm_tx_attr_t _a = (stm_tx_attr_t)0;

#ifdef TM_GCC
  /* GCC does not call initializeProcess TODO: other fix possible? */
  if (unlikely(tx == NULL)) {
    /* Thread not initialized: must create transaction */
    tx = abi_init_thread();
  }
#endif
  assert(tx != NULL);

#ifdef TM_DTMC
  /* FIXME this should be fixed with stdcall conv call */
  /* DTMC prior or equal to Velox R3 did not use regparm(2) with x86-32. */
  /* TODO to be removed when new release of DTMC fix it. */
  /* attr = 3; */
  ret = a_runInstrumentedCode;
#else /* !TM_DTMC */
  /* Manage attribute for the transaction */
  if (unlikely((attr & pr_doesGoIrrevocable) || !(attr & pr_instrumentedCode))) {
    /* TODO Add an attribute to specify irrevocable TX */
    stm_set_irrevocable_tx(tx, 1);
    ret = a_runUninstrumentedCode;
    if ((attr & pr_multiwayCode) == pr_instrumentedCode)
      ret = a_runInstrumentedCode;
  } else {
#ifdef TM_GCC
    if (attr & pr_readOnly)
      _a.read_only = 1;
#endif /* TM_GCC */

    ret = a_runInstrumentedCode | a_saveLiveVariables;
  }
#endif /* !TM_DTMC */

#ifdef TM_DTMC
  /* if (ret & a_runInstrumentedCode) */
  tanger_stm_save_stack();
#endif /* TM_DTMC */

  env = int_stm_start(tx, _a);
  /* Save thread context only when outermost transaction */
  /* TODO check that the memcpy is fast. */
  if (likely(env != NULL))
    memcpy(env, buf, sizeof(jmp_buf)); /* TODO limit size to real size */

  return ret;
}

void _ITM_CALL_CONVENTION _ITM_commitTransaction(
#if defined(TM_GCC)
                             void
#else /* !TM_GCC */
                             TX_ARGS const _ITM_srcLocation *__src
#endif /* !TM_GCC */
                             )
{
  TX_GET_ABI;
  int_stm_commit(tx);
#ifdef TM_DTMC
  tanger_stm_reset_stack();
#endif /* TM_DTMC */
}

bool _ITM_CALL_CONVENTION _ITM_tryCommitTransaction(TX_ARGS
                                   const _ITM_srcLocation *__src)
{
  TX_GET_ABI;
  return (int_stm_commit(tx) != 0);
}

/**
 * Commits all inner transactions nested within the transaction specified by
 * the transaction id parameter.
 */
void _ITM_CALL_CONVENTION _ITM_commitTransactionToId(TX_ARGS
                              const _ITM_transactionId tid,
                              const _ITM_srcLocation *__src)
{
  TX_GET_ABI;
  while ((tx->nesting + 1) > tid)
    int_stm_commit(tx);
}

/* TODO: add noreturn attribute. */
void _ITM_CALL_CONVENTION _ITM_abortTransaction(TX_ARGS
                              _ITM_abortReason __reason,
                              const _ITM_srcLocation *__src)
{
  TX_GET_ABI;
  if( __reason == userAbort) {
    /* __tm_abort was invoked. */
    __reason = STM_ABORT_NO_RETRY;
  } else if(__reason == userRetry) {
    /* __tm_retry was invoked. */
    __reason = STM_ABORT_EXPLICIT;
  }
  stm_rollback(tx, __reason);
}

/* TODO: add noreturn attribute. */
void _ITM_CALL_CONVENTION _ITM_rollbackTransaction(TX_ARGS
                              const _ITM_srcLocation *__src)
{
  /* TODO check exactly the purpose of this function */
  TX_GET_ABI;
  stm_rollback(tx, STM_ABORT_EXPLICIT);
}


void _ITM_CALL_CONVENTION _ITM_registerThrownObject(TX_ARGS
                              const void *__obj, size_t __size)
{
  // TODO A rollback of the tx will not roll back the registered object
  fprintf(stderr, "%s: not yet implemented\n", __func__);
}

void _ITM_CALL_CONVENTION _ITM_changeTransactionMode(TX_ARGS
                              _ITM_transactionState __mode,
                              const _ITM_srcLocation *__loc)
{
  /* FIXME: it seems there is a problem with irrevocable and intel c */
  switch (__mode) {
    case modeSerialIrrevocable:
      stm_set_irrevocable(1);
      /* TODO a_runUninstrumentedCode must be set at rollback! */
      break;
    case modeObstinate:
    case modeOptimistic:
    case modePessimistic:
    default:
	fprintf(stderr, "This mode %d is not implemented yet\n", __mode);
  }
}

#if defined(TM_GCC) || defined(TM_DTMC)
void * _ITM_malloc(size_t size)
{
  stm_tx_t *tx = tls_get_tx();
  if (tx == NULL || !stm_active_tx(tx))
    return malloc(size);
  return stm_malloc_tx(tx, size);
}

void * _ITM_calloc(size_t nm, size_t size)
{
  stm_tx_t *tx = tls_get_tx();
  if (tx == NULL || !stm_active_tx(tx))
    return calloc(nm, size);
  return stm_calloc_tx(tx, nm, size);
}

void _ITM_free(void *ptr)
{
  stm_tx_t *tx = tls_get_tx();
  if (tx == NULL || !stm_active_tx(tx)) {
    free(ptr);
    return;
  }
#ifdef NO_WRITE_ON_FREE
  stm_free_tx(tx, ptr, 0);
#else
  stm_free_tx(tx, ptr, block_size(ptr));
#endif
}
#endif /* defined(TM_GCC) || defined(TM_DTMC) */


#ifdef TM_GCC
# include "gcc/clone.c"
# include "gcc/eh.c"

/* TODO This function is not fully compatible, need to delete exception
 * on abort. */
void _ITM_CALL_CONVENTION _ITM_commitTransactionEH(void *exc_ptr)
{
  TX_GET_ABI;
  int_stm_commit(tx);
}
#endif /* TM_GCC */

#ifdef TM_INTEL
# include "intel/alloc.c"
#endif


/**** LOAD STORE LOG FUNCTIONS ****/

#define TM_LOAD(F, T, WF, WT) \
  T _ITM_CALL_CONVENTION F(TX_ARGS const T *addr) \
  { \
    return (WT)WF((volatile WT *)addr); \
  }

#define TM_LOAD_GENERIC(F, T) \
  T _ITM_CALL_CONVENTION F(TX_ARGS const T *addr) \
  { \
    union { T d; uint8_t s[sizeof(T)]; } c; \
    stm_load_bytes((volatile uint8_t *)addr, c.s, sizeof(T)); \
    return c.d; \
  }

/* TODO if WRITE_BACK/ALL?, write to stack must be saved and written directly
 * TODO must use stm_log is addresses are under the beginTransaction
  if (on_stack(addr)) { stm_log_u64(addr); *addr = val; } 
not enough because if we abort and restore -> stack can be corrupted
*/
#ifdef STACK_CHECK
#define TM_STORE(F, T, WF, WT) \
  void _ITM_CALL_CONVENTION F(TX_ARGS const T *addr, T val) \
  { \
    if (on_stack(addr)) *((T*)addr) = val; \
    else WF((volatile WT *)addr, (WT)val); \
  }
#else /* !STACK_CHECK */
#define TM_STORE(F, T, WF, WT) \
  void _ITM_CALL_CONVENTION F(TX_ARGS const T *addr, T val) \
  { \
    WF((volatile WT *)addr, (WT)val); \
  }
#endif /* !STACK_CHECK */

#define TM_STORE_GENERIC(F, T) \
  void _ITM_CALL_CONVENTION F(TX_ARGS const T *addr, T val) \
  { \
    union { T d; uint8_t s[sizeof(T)]; } c; \
    c.d = val; \
    stm_store_bytes((volatile uint8_t *)addr, c.s, sizeof(T)); \
  }

#define TM_LOG(F, T, WF, WT) \
  void _ITM_CALL_CONVENTION F(TX_ARGS const T *addr) \
  { \
    WF((WT *)addr); \
  }

#define TM_LOG_GENERIC(F, T) \
  void _ITM_CALL_CONVENTION F(TX_ARGS const T *addr) \
  { \
    stm_log_bytes((uint8_t *)addr, sizeof(T));    \
  }

#define TM_STORE_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS void *dst, const void *src, size_t size) \
  { \
    stm_store_bytes((volatile uint8_t *)dst, (uint8_t *)src, size); \
  }

#define TM_LOAD_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS void *dst, const void *src, size_t size) \
  { \
    stm_load_bytes((volatile uint8_t *)src, (uint8_t *)dst, size); \
  }

#define TM_LOG_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS const void *addr, size_t size) \
  { \
    stm_log_bytes((uint8_t *)addr, size); \
  }

#ifdef STACK_CHECK
#define TM_SET_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS void *dst, int val, size_t count) \
  { \
    if (on_stack(dst)) memset(dst, val, count); \
    else stm_set_bytes((volatile uint8_t *)dst, val, count); \
  }
#else /* !STACK_CHECK */
#define TM_SET_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS void *dst, int val, size_t count) \
  { \
    stm_set_bytes((volatile uint8_t *)dst, val, count); \
  }
#endif /* !STACK_CHECK */

#ifdef STACK_CHECK
#define TM_COPY_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS void *dst, const void *src, size_t size) \
  { \
    uint8_t *buf = (uint8_t *)alloca(size); \
    if (on_stack(src)) memcpy(buf, src, size); \
    stm_load_bytes((volatile uint8_t *)src, buf, size); \
    if (on_stack(dst)) memcpy(dst, buf, size); \
    else stm_store_bytes((volatile uint8_t *)dst, buf, size); \
  }
#else /* !STACK_CHECK */
#define TM_COPY_BYTES(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS void *dst, const void *src, size_t size) \
  { \
    uint8_t *buf = (uint8_t *)alloca(size); \
    stm_load_bytes((volatile uint8_t *)src, buf, size); \
    stm_store_bytes((volatile uint8_t *)dst, buf, size); \
  }
#endif /* !STACK_CHECK */

#define TM_COPY_BYTES_RN_WT(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS void *dst, const void *src, size_t size) \
  { \
    uint8_t *buf = (uint8_t *)alloca(size); \
    memcpy(buf, src, size); \
    stm_store_bytes((volatile uint8_t *)dst, buf, size); \
  }

#define TM_COPY_BYTES_RT_WN(F) \
  void _ITM_CALL_CONVENTION F(TX_ARGS void *dst, const void *src, size_t size) \
  { \
    uint8_t *buf = (uint8_t *)alloca(size); \
    stm_load_bytes((volatile uint8_t *)src, buf, size); \
    memcpy(dst, buf, size); \
  }

#define TM_LOAD_ALL(E, T, WF, WT) \
  TM_LOAD(_ITM_R##E, T, WF, WT) \
  TM_LOAD(_ITM_RaR##E, T, WF, WT) \
  TM_LOAD(_ITM_RaW##E, T, WF, WT) \
  TM_LOAD(_ITM_RfW##E, T, WF, WT)

#define TM_LOAD_GENERIC_ALL(E, T) \
  TM_LOAD_GENERIC(_ITM_R##E, T) \
  TM_LOAD_GENERIC(_ITM_RaR##E, T) \
  TM_LOAD_GENERIC(_ITM_RaW##E, T) \
  TM_LOAD_GENERIC(_ITM_RfW##E, T)

#define TM_STORE_ALL(E, T, WF, WT) \
  TM_STORE(_ITM_W##E, T, WF, WT) \
  TM_STORE(_ITM_WaR##E, T, WF, WT) \
  TM_STORE(_ITM_WaW##E, T, WF, WT)

#define TM_STORE_GENERIC_ALL(E, T) \
  TM_STORE_GENERIC(_ITM_W##E, T) \
  TM_STORE_GENERIC(_ITM_WaR##E, T) \
  TM_STORE_GENERIC(_ITM_WaW##E, T)


/* TODO U1 U2 should not use the inline stm_load to increase locality */
TM_LOAD_ALL(U1, uint8_t, int_stm_load_u8, uint8_t)
TM_LOAD_ALL(U2, uint16_t, int_stm_load_u16, uint16_t)
TM_LOAD_ALL(U4, uint32_t, int_stm_load_u32, uint32_t)
TM_LOAD_ALL(U8, uint64_t, int_stm_load_u64, uint64_t)
TM_LOAD_ALL(F, float, stm_load_float, float)
TM_LOAD_ALL(D, double, stm_load_double, double)
#ifdef __SSE__
TM_LOAD_GENERIC_ALL(M64, __m64)
TM_LOAD_GENERIC_ALL(M128, __m128)
#endif /* __SSE__ */
TM_LOAD_GENERIC_ALL(CF, float _Complex)
TM_LOAD_GENERIC_ALL(CD, double _Complex)
TM_LOAD_GENERIC_ALL(CE, long double _Complex)

TM_STORE_ALL(U1, uint8_t, int_stm_store_u8, uint8_t)
TM_STORE_ALL(U2, uint16_t, int_stm_store_u16, uint16_t)
TM_STORE_ALL(U4, uint32_t, int_stm_store_u32, uint32_t)
TM_STORE_ALL(U8, uint64_t, int_stm_store_u64, uint64_t)
TM_STORE_ALL(F, float, stm_store_float, float)
TM_STORE_ALL(D, double, stm_store_double, double)
#ifdef __SSE__
TM_STORE_GENERIC_ALL(M64, __m64)
TM_STORE_GENERIC_ALL(M128, __m128)
#endif /* __SSE__ */
TM_STORE_GENERIC_ALL(CF, float _Complex)
TM_STORE_GENERIC_ALL(CD, double _Complex)
TM_STORE_GENERIC_ALL(CE, long double _Complex)

TM_STORE_BYTES(_ITM_memcpyRnWt)
TM_STORE_BYTES(_ITM_memcpyRnWtaR)
TM_STORE_BYTES(_ITM_memcpyRnWtaW)

TM_LOAD_BYTES(_ITM_memcpyRtWn)
TM_LOAD_BYTES(_ITM_memcpyRtaRWn)
TM_LOAD_BYTES(_ITM_memcpyRtaWWn)

TM_COPY_BYTES(_ITM_memcpyRtWt)
TM_COPY_BYTES(_ITM_memcpyRtWtaR)
TM_COPY_BYTES(_ITM_memcpyRtWtaW)
TM_COPY_BYTES(_ITM_memcpyRtaRWt)
TM_COPY_BYTES(_ITM_memcpyRtaRWtaR)
TM_COPY_BYTES(_ITM_memcpyRtaRWtaW)
TM_COPY_BYTES(_ITM_memcpyRtaWWt)
TM_COPY_BYTES(_ITM_memcpyRtaWWtaR)
TM_COPY_BYTES(_ITM_memcpyRtaWWtaW)

TM_LOG(_ITM_LU1, uint8_t, stm_log_u8, uint8_t)
TM_LOG(_ITM_LU2, uint16_t, stm_log_u16, uint16_t)
TM_LOG(_ITM_LU4, uint32_t, stm_log_u32, uint32_t)
TM_LOG(_ITM_LU8, uint64_t, stm_log_u64, uint64_t)
TM_LOG(_ITM_LF, float, stm_log_float, float)
TM_LOG(_ITM_LD, double, stm_log_double, double)
TM_LOG_GENERIC(_ITM_LE, long double)
#ifdef __SSE__
TM_LOG_GENERIC(_ITM_LM64, __m64)
TM_LOG_GENERIC(_ITM_LM128, __m128)
#endif /* __SSE__ */
TM_LOG_GENERIC(_ITM_LCF, float _Complex)
TM_LOG_GENERIC(_ITM_LCD, double _Complex)
TM_LOG_GENERIC(_ITM_LCE, long double _Complex)

TM_LOG_BYTES(_ITM_LB)

TM_SET_BYTES(_ITM_memsetW)
TM_SET_BYTES(_ITM_memsetWaR)
TM_SET_BYTES(_ITM_memsetWaW)

TM_COPY_BYTES_RN_WT(_ITM_memmoveRnWt)
TM_COPY_BYTES_RN_WT(_ITM_memmoveRnWtaR)
TM_COPY_BYTES_RN_WT(_ITM_memmoveRnWtaW)

TM_COPY_BYTES_RT_WN(_ITM_memmoveRtWn)
TM_COPY_BYTES_RT_WN(_ITM_memmoveRtaRWn)
TM_COPY_BYTES_RT_WN(_ITM_memmoveRtaWWn)

TM_COPY_BYTES(_ITM_memmoveRtWt)
TM_COPY_BYTES(_ITM_memmoveRtWtaR)
TM_COPY_BYTES(_ITM_memmoveRtWtaW)
TM_COPY_BYTES(_ITM_memmoveRtaRWt)
TM_COPY_BYTES(_ITM_memmoveRtaRWtaR)
TM_COPY_BYTES(_ITM_memmoveRtaRWtaW)
TM_COPY_BYTES(_ITM_memmoveRtaWWt)
TM_COPY_BYTES(_ITM_memmoveRtaWWtaR)
TM_COPY_BYTES(_ITM_memmoveRtaWWtaW)

#ifdef TM_DTMC
/* DTMC file uses this macro name for other thing */
# undef TM_LOAD
# undef TM_STORE
# include "dtmc/tanger.c"
#endif /* TM_DTMC */

