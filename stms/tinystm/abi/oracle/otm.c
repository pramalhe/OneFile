#include <stdint.h>
#include "stm.h"
#include "wrappers.h"
#include "mod_mem.h"

#define CTX_ITM   _ITM_siglongjmp
#define _ITM_CALL_CONVENTION __attribute__((regparm(2)))
extern void _ITM_CALL_CONVENTION _ITM_siglongjmp(int val, sigjmp_buf env) __attribute__ ((noreturn));

#include "stm.c"
#include "mod_cb_mem.c"
#include "wrappers.c"

/* TODO __FUNCTION__ is not available with Oracle Studio if -Xc () and -Xs (K&R mode) but should not be a problem. */
/* __FUNCTION__ and __PRETTY_FUNCTION__ are predefined identifiers that contain the name of the lexically-enclosing function. They are functionally equivalent to the c99 predefined identifier, __func__. On Solaris platforms, __FUNCTION__ and __PRETTY_FUNCTION__ are not available in -Xs and -Xc modes. */

stm_tx_t *STM_GetMyTransId(void)
{
  stm_tx_t *tx = stm_current_tx();
  PRINT_DEBUG("==> %s()\n", __FUNCTION__);
  if (tx == NULL) {
    stm_init();
    mod_mem_init(0);
    tx = stm_init_thread();
    /* TODO save stack high and low addr */
  }
  PRINT_DEBUG("==> %s() -> 0x%p\n", __FUNCTION__, tx);
  return tx;
}

__attribute__((regparm(2)))
int _STM_BeginTransaction(stm_tx_t *tx, jmp_buf *buf)
{
  sigjmp_buf * env;
  PRINT_DEBUG("==> %s(0x%p)\n", __FUNCTION__, tx);
  /* TODO see how the ctx is saved and rollback. */
  env = int_stm_start(tx, (stm_tx_attr_t)0);
  if (likely(env != NULL))
    memcpy(env, buf, sizeof(jmp_buf)); /* TODO limit size to real size */
  return 1;
}

int STM_ValidateTransaction(stm_tx_t *tx)
{
  /*int ret;*/
  PRINT_DEBUG("==> %s(0x%p)\n", __FUNCTION__, tx);
  /*ret = stm_validate(tx);*/
  return 1 /*ret*/;
}

int STM_CommitTransaction(stm_tx_t *tx)
{
  int ret;
  PRINT_DEBUG("==> %s(tx=0x%p)\n", __FUNCTION__, tx);
  ret = stm_commit_tx(tx);
  /* Returned value are : Abort no retry=-1 / Abort retry=0 / Committed no retry=1 */
  return ret;
}

/* API uses many steps: acquisition, then transactional read
 * to apply this to tinySTM it requires to split stm_load function in many pieces */

typedef void * RdHandle;
typedef void * WrHandle;

RdHandle *STM_AcquireReadPermission(stm_tx_t *tx, stm_word_t *addr, int valid)
{
  PRINT_DEBUG("==> %s(tx=0x%p,addr=0x%p,valid=%d)\n", tx, addr, valid);
  return NULL;
}

WrHandle *STM_AcquireWritePermission(stm_tx_t *tx, stm_word_t *addr, int valid)
{
  PRINT_DEBUG("==> %s(tx=0x%p,addr=0x%p,valid=%d)\n", tx, addr, valid);
  return NULL;
}

WrHandle* STM_AcquireReadWritePermission(stm_tx_t *tx, stm_word_t *addr, int valid)
{
  PRINT_DEBUG("==> %s(tx=0x%p,addr=0x%p,valid=%d)\n", __FUNCTION__, tx, addr, valid);
  return NULL;
}


/* Transactional loads */

uint8_t STM_TranRead8(stm_tx_t *tx, RdHandle *theRdHandle, uint8_t *addr, int valid)
{
  PRINT_DEBUG("==> %s(tx=0x%p,handle=0x%p,addr=0x%p,valid=%d)\n", __FUNCTION__, tx, theRdHandle, addr, valid);
  return stm_load_u8(/*tx,*/ addr);
}

uint16_t STM_TranRead16(stm_tx_t *tx, RdHandle *theRdHandle, uint16_t *addr, int valid)
{
  PRINT_DEBUG("==> %s(tx=0x%p,handle=0x%p,addr=0x%p,valid=%d)\n", __FUNCTION__, tx, theRdHandle, addr, valid);
  return stm_load_u16(/*tx,*/ addr);
}

uint32_t STM_TranRead32(stm_tx_t *tx, RdHandle *theRdHandle, uint32_t *addr, int valid)
{
  PRINT_DEBUG("==> %s(tx=0x%p,handle=0x%p,addr=0x%p,valid=%d)\n", __FUNCTION__, tx, theRdHandle, addr, valid);
  /* TODO can it be more efficient with #ifdef _LP64 and stm_load(). */
  return stm_load_u32(/*tx,*/ addr);
}

double STM_TranReadFloat32(stm_tx_t *tx, RdHandle *theRdHandle, float *addr, int valid)
{
  PRINT_DEBUG("==> %s(tx=0x%p,handle=0x%p,addr=0x%p,valid=%d)\n", __FUNCTION__, tx, theRdHandle, addr, valid);
  return stm_load_float(/*tx,*/ addr);
}

uint64_t STM_TranRead64(stm_tx_t *tx, RdHandle *theRdHandle, uint64_t *addr, int valid)
{
  PRINT_DEBUG("==> %s(tx=0x%p,handle=0x%p,addr=0x%p,valid=%d)\n", __FUNCTION__, tx, theRdHandle, addr, valid);
  return stm_load_u64(/*tx,*/ addr);
}

double STM_TranReadFloat64(stm_tx_t *tx, RdHandle *theRdHandle, double *addr, int valid)
{
  PRINT_DEBUG("==> %s(tx=0x%p,handle=0x%p,addr=0x%p,valid=%d)\n", __FUNCTION__, tx, theRdHandle, addr, valid);
  return stm_load_double(/*tx,*/ addr);
}


/* Transactional stores */

int STM_TranWrite8(stm_tx_t *tx, WrHandle* theWrHandle, uint8_t *addr,  uint8_t val, int valid)
{
  PRINT_DEBUG("==> %s(tx=0x%p,handle=0x%p,addr=0x%p,val=%u,valid=%d)\n", __FUNCTION__, tx, theWrHandle, addr, val, valid);
  stm_store_u8(/*tx,*/ addr, val);
  return 1;
}
int STM_TranWrite16(stm_tx_t *tx, WrHandle* theWrHandle, uint16_t *addr, uint16_t val, int valid)
{
  PRINT_DEBUG("==> %s(tx=0x%p,handle=0x%p,addr=0x%p,val=%u,valid=%d)\n", __FUNCTION__, tx, theWrHandle, addr, val, valid);
  stm_store_u16(/*tx,*/ addr, val);
  return 1;
}

int STM_TranWrite32(stm_tx_t *tx, WrHandle *theWrHandle, uint32_t *addr, uint32_t val, int valid)
{
  PRINT_DEBUG("==> %s(tx=0x%p,handle=0x%p,addr=0x%p,val=%u,valid=%d)\n", __FUNCTION__, tx, theWrHandle, addr, val, valid);
  stm_store_u32(/*tx,*/ addr, val);
  return 1;
}

int STM_TranWrite64(stm_tx_t *tx, WrHandle *theWrHandle, uint64_t *addr, uint64_t val, int valid)
{
  PRINT_DEBUG("==> %s(tx=0x%p,handle=0x%p,addr=0x%p,val=%lu,valid=%d)\n", __FUNCTION__, tx, theWrHandle, addr, val, valid);
  stm_store_u64(/*tx,*/ addr, val);
  return 1;
}

int STM_TranWriteFloat32(stm_tx_t *tx, WrHandle *theWrHandle, float *addr, float val, int valid)
{
  PRINT_DEBUG("==> %s(tx=0x%p,handle=0x%p,addr=0x%p,val=%f,valid=%d)\n", __FUNCTION__, tx, theWrHandle, addr, val, valid);
  stm_store_float(/*tx,*/ addr, val);
  return 1;
}

int STM_TranWriteFloat64(stm_tx_t *tx, WrHandle *theWrHandle, double *addr, double val, int valid)
{
  PRINT_DEBUG("==> %s(tx=0x%p,handle=0x%p,addr=0x%p,val=%f,valid=%d)\n", __FUNCTION__, tx, theWrHandle, addr, val, valid);
  stm_store_double(/*tx,*/ addr, val);
  return 1;
}


/* Transactional memory management */

void *STM_TranMalloc(stm_tx_t *tx, size_t sz)
{
  PRINT_DEBUG("==> %s(tx=0x%p,size=%d)\n", __FUNCTION__, tx, sz);
  return stm_malloc_tx(tx, sz);
}

void *STM_TranCalloc(stm_tx_t *tx, size_t elem, size_t sz)
{
  PRINT_DEBUG("==> %s(tx=0x%p,elem=%d,size=%d)\n", __FUNCTION__, tx, elem, sz);
  return stm_calloc_tx(tx, elem, sz);
}

void STM_TranMFree(stm_tx_t *tx, void *addr)
{
  PRINT_DEBUG("==> %s(tx=0x%p,addr=%p)\n", __FUNCTION__, tx, addr);
  /* TODO: guess the size... use as in itm block_size(ptr)? is it available in Solaris? */
  stm_free_tx(tx, addr, sizeof(stm_word_t));
}

void *STM_TranMemAlign(stm_tx_t *tx, size_t alignment, size_t sz)
{
  assert(0);
  return NULL;
}

void *STM_TranValloc(stm_tx_t *tx, size_t sz)
{
  assert(0);
  return NULL;
}

/* TODO check if sz is size_t? same for alignment? */
void STM_TranMemCpy(stm_tx_t *tx, void* src, void* dst, size_t sz, uint32_t alignment)
{
  /* TODO what to do with alignment? */
  uint8_t *buf = (uint8_t *)alloca(sz);
  stm_load_bytes(/*tx,*/ (volatile uint8_t *)src, buf, sz);
  stm_store_bytes(/*tx,*/ (volatile uint8_t *)dst, buf, sz);
}

int STM_CurrentlyUsingDecoratedPath(stm_tx_t* tx)
{
  if (tx == NULL)
    return 0;
  /* TODO check that the nesting is != 0 */
  return 1; 
}


