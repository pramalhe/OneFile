/*
 * File:
 *   stm_wbctl.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   STM internal functions for write-back CTL.
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

#ifndef _STM_WBCTL_H_
#define _STM_WBCTL_H_

static INLINE int
stm_wbctl_validate(stm_tx_t *tx)
{
  r_entry_t *r;
  int i;
  stm_word_t l;

  PRINT_DEBUG("==> stm_wbctl_validate(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  /* Validate reads */
  r = tx->r_set.entries;
  for (i = tx->r_set.nb_entries; i > 0; i--, r++) {
    /* Read lock */
    l = ATOMIC_LOAD(r->lock);
    /* Unlocked and still the same version? */
    if (LOCK_GET_OWNED(l)) {
      /* Do we own the lock? */
      w_entry_t *w = (w_entry_t *)LOCK_GET_ADDR(l);
      /* Simply check if address falls inside our write set (avoids non-faulting load) */
      if (!(tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries))
      {
        /* Locked by another transaction: cannot validate */
#ifdef CONFLICT_TRACKING
        if (_tinystm.conflict_cb != NULL) {
# ifdef UNIT_TX
          if (l != LOCK_UNIT) {
# endif /* UNIT_TX */
            /* Call conflict callback */
            stm_tx_t *other = ((w_entry_t *)LOCK_GET_ADDR(l))->tx;
            _tinystm.conflict_cb(tx, other);
# ifdef UNIT_TX
          }
# endif /* UNIT_TX */
        }
#endif /* CONFLICT_TRACKING */
        return 0;
      }
      /* We own the lock: OK */
      if (w->version != r->version) {
        /* Other version: cannot validate */
        return 0;
      }
    } else {
      if (LOCK_GET_TIMESTAMP(l) != r->version) {
        /* Other version: cannot validate */
        return 0;
      }
      /* Same version: OK */
    }
  }
  return 1;
}

/*
 * Extend snapshot range.
 */
static INLINE int
stm_wbctl_extend(stm_tx_t *tx)
{
  stm_word_t now;

  PRINT_DEBUG("==> stm_wbctl_extend(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

#ifdef UNIT_TX
  /* Extension is disabled */
  if (tx->attr.no_extend)
    return 0;
#endif /* UNIT_TX */

  /* Get current time */
  now = GET_CLOCK;
  /* No need to check clock overflow here. The clock can exceed up to MAX_THREADS and it will be reset when the quiescence is reached. */

  /* Try to validate read set */
  if (stm_wbctl_validate(tx)) {
    /* It works: we can extend until now */
    tx->end = now;
    return 1;
  }
  return 0;
}

static INLINE void
stm_wbctl_rollback(stm_tx_t *tx)
{
  w_entry_t *w;

  PRINT_DEBUG("==> stm_wbctl_rollback(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  assert(IS_ACTIVE(tx->status));

  if (tx->w_set.nb_acquired > 0) {
    w = tx->w_set.entries + tx->w_set.nb_entries;
    do {
      w--;
      if (!w->no_drop) {
        if (--tx->w_set.nb_acquired == 0) {
          /* Make sure that all lock releases become visible to other threads */
          ATOMIC_STORE_REL(w->lock, LOCK_SET_TIMESTAMP(w->version));
        } else {
          ATOMIC_STORE(w->lock, LOCK_SET_TIMESTAMP(w->version));
        }
      }
    } while (tx->w_set.nb_acquired > 0);
  }
}

static INLINE stm_word_t
stm_wbctl_read(stm_tx_t *tx, volatile stm_word_t *addr)
{
  volatile stm_word_t *lock;
  stm_word_t l, l2, value, version;
  r_entry_t *r;
  w_entry_t *written = NULL;

  PRINT_DEBUG2("==> stm_wbctl_read(t=%p[%lu-%lu],a=%p)\n", tx, (unsigned long)tx->start, (unsigned long)tx->end, addr);

  assert(IS_ACTIVE(tx->status));

  /* Did we previously write the same address? */
  written = stm_has_written(tx, addr);
  if (written != NULL) {
    /* Yes: get value from write set if possible */
    if (written->mask == ~(stm_word_t)0) {
      value = written->value;
      /* No need to add to read set */
      return value;
    }
  }

  /* Get reference to lock */
  lock = GET_LOCK(addr);

  /* Note: we could check for duplicate reads and get value from read set */

  /* Read lock, value, lock */
 restart:
  l = ATOMIC_LOAD_ACQ(lock);
 restart_no_load:
  if (LOCK_GET_WRITE(l)) {
    /* Locked */
    /* Do we own the lock? */
    /* Spin while locked (should not last long) */
    goto restart;
  } else {
    /* Not locked */
    value = ATOMIC_LOAD_ACQ(addr);
    l2 = ATOMIC_LOAD_ACQ(lock);
    if (l != l2) {
      l = l2;
      goto restart_no_load;
    }
#ifdef IRREVOCABLE_ENABLED
    /* In irrevocable mode, no need check timestamp nor add entry to read set */
    if (tx->irrevocable)
      goto return_value;
#endif /* IRREVOCABLE_ENABLED */
    /* Check timestamp */
    version = LOCK_GET_TIMESTAMP(l);
    /* Valid version? */
    if (version > tx->end) {
      /* No: try to extend first (except for read-only transactions: no read set) */
      if (tx->attr.read_only || !stm_wbctl_extend(tx)) {
        /* Not much we can do: abort */
        stm_rollback(tx, STM_ABORT_VAL_READ);
        return 0;
      }
      /* Verify that version has not been overwritten (read value has not
       * yet been added to read set and may have not been checked during
       * extend) */
      l = ATOMIC_LOAD_ACQ(lock);
      if (l != l2) {
        l = l2;
        goto restart_no_load;
      }
      /* Worked: we now have a good version (version <= tx->end) */
    }
  }
  /* We have a good version: add to read set (update transactions) and return value */

  /* Did we previously write the same address? */
  if (written != NULL) {
    value = (value & ~written->mask) | (written->value & written->mask);
    /* Must still add to read set */
  }
#ifdef READ_LOCKED_DATA
 add_to_read_set:
#endif /* READ_LOCKED_DATA */
  if (!tx->attr.read_only) {
#ifdef NO_DUPLICATES_IN_RW_SETS
    if (stm_has_read(tx, lock) != NULL)
      goto return_value;
#endif /* NO_DUPLICATES_IN_RW_SETS */
    /* Add address and version to read set */
    if (tx->r_set.nb_entries == tx->r_set.size)
      stm_allocate_rs_entries(tx, 1);
    r = &tx->r_set.entries[tx->r_set.nb_entries++];
    r->version = version;
    r->lock = lock;
  }
 return_value:
  return value;
}

static INLINE w_entry_t *
stm_wbctl_write(stm_tx_t *tx, volatile stm_word_t *addr, stm_word_t value, stm_word_t mask)
{
  volatile stm_word_t *lock;
  stm_word_t l, version;
  w_entry_t *w;

  PRINT_DEBUG2("==> stm_wbctl_write(t=%p[%lu-%lu],a=%p,d=%p-%lu,m=0x%lx)\n",
               tx, (unsigned long)tx->start, (unsigned long)tx->end, addr, (void *)value, (unsigned long)value, (unsigned long)mask);

  /* Get reference to lock */
  lock = GET_LOCK(addr);

  /* Try to acquire lock */
 restart:
  l = ATOMIC_LOAD_ACQ(lock);
 restart_no_load:
  if (LOCK_GET_OWNED(l)) {
    /* Locked */
    /* Spin while locked (should not last long) */
    goto restart;
  }
  /* Not locked */
  w = stm_has_written(tx, addr);
  if (w != NULL) {
    w->value = (w->value & ~mask) | (value & mask);
    w->mask |= mask;
    return w;
  }
  /* Handle write after reads (before CAS) */
  version = LOCK_GET_TIMESTAMP(l);
#ifdef IRREVOCABLE_ENABLED
  /* In irrevocable mode, no need to revalidate */
  if (tx->irrevocable)
    goto acquire_no_check;
#endif /* IRREVOCABLE_ENABLED */
 acquire:
  if (version > tx->end) {
    /* We might have read an older version previously */
#ifdef UNIT_TX
    if (tx->attr.no_extend) {
      stm_rollback(tx, STM_ABORT_VAL_WRITE);
      return NULL;
    }
#endif /* UNIT_TX */
    if (stm_has_read(tx, lock) != NULL) {
      /* Read version must be older (otherwise, tx->end >= version) */
      /* Not much we can do: abort */
      stm_rollback(tx, STM_ABORT_VAL_WRITE);
      return NULL;
    }
  }
  /* Acquire lock (ETL) */
#ifdef IRREVOCABLE_ENABLED
 acquire_no_check:
#endif /* IRREVOCABLE_ENABLED */
  /* We own the lock here (ETL) */
do_write:
  /* Add address to write set */
  if (tx->w_set.nb_entries == tx->w_set.size)
    stm_allocate_ws_entries(tx, 1);
  w = &tx->w_set.entries[tx->w_set.nb_entries++];
  w->addr = addr;
  w->mask = mask;
  w->lock = lock;
  if (mask == 0) {
    /* Do not write anything */
#ifndef NDEBUG
    w->value = 0;
#endif /* ! NDEBUG */
  } else {
    /* Remember new value */
    w->value = value;
  }
# ifndef NDEBUG
  w->version = version;
# endif /* !NDEBUG */
  w->no_drop = 1;
# ifdef USE_BLOOM_FILTER
  tx->w_set.bloom |= FILTER_BITS(addr) ;
# endif /* USE_BLOOM_FILTER */

  return w;
}

static INLINE stm_word_t
stm_wbctl_RaR(stm_tx_t *tx, volatile stm_word_t *addr)
{
  /* Possible optimization: avoid adding to read set. */
  return stm_wbctl_read(tx, addr);
}

static INLINE stm_word_t
stm_wbctl_RaW(stm_tx_t *tx, volatile stm_word_t *addr)
{
  /* Cannot be much better than regular due to mask == 0 case. */
  return stm_wbctl_read(tx, addr);
}

static INLINE stm_word_t
stm_wbctl_RfW(stm_tx_t *tx, volatile stm_word_t *addr)
{
  /* We need to return the value here, so write with mask=0 is not enough. */
  return stm_wbctl_read(tx, addr);
}

static INLINE void
stm_wbctl_WaR(stm_tx_t *tx, volatile stm_word_t *addr, stm_word_t value, stm_word_t mask)
{
  /* Probably no optimization can be done here. */
  stm_wbctl_write(tx, addr, value, mask);
}

static INLINE void
stm_wbctl_WaW(stm_tx_t *tx, volatile stm_word_t *addr, stm_word_t value, stm_word_t mask)
{
  w_entry_t *w;
  /* Get the write set entry. */
  w = stm_has_written(tx, addr);
  assert(w != NULL);
  /* Update directly into the write set. */
  w->value = (w->value & ~mask) | (value & mask);
  w->mask |= mask;
}

static INLINE int
stm_wbctl_commit(stm_tx_t *tx)
{
  w_entry_t *w;
  stm_word_t t;
  int i;
  stm_word_t l, value;

  PRINT_DEBUG("==> stm_wbctl_commit(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  /* Acquire locks (in reverse order) */
  w = tx->w_set.entries + tx->w_set.nb_entries;
  do {
    w--;
    /* Try to acquire lock */
 restart:
    l = ATOMIC_LOAD(w->lock);
    if (LOCK_GET_OWNED(l)) {
      /* Do we already own the lock? */
      if (tx->w_set.entries <= (w_entry_t *)LOCK_GET_ADDR(l) && (w_entry_t *)LOCK_GET_ADDR(l) < tx->w_set.entries + tx->w_set.nb_entries) {
        /* Yes: ignore */
        continue;
      }
      /* Conflict: CM kicks in */
# if CM == CM_DELAY
      tx->c_lock = w->lock;
# endif /* CM == CM_DELAY */

#ifdef IRREVOCABLE_ENABLED
      if (tx->irrevocable) {
# ifdef IRREVOCABLE_IMPROVED
        if (ATOMIC_LOAD(&_tinystm.irrevocable) == 1)
          ATOMIC_STORE(&_tinystm.irrevocable, 2);
# endif /* IRREVOCABLE_IMPROVED */
        /* Spin while locked */
        goto restart;
      }
#endif /* IRREVOCABLE_ENABLED */

      /* Abort self */
      stm_rollback(tx, STM_ABORT_WW_CONFLICT);
      return 0;
    }
    if (ATOMIC_CAS_FULL(w->lock, l, LOCK_SET_ADDR_WRITE((stm_word_t)w)) == 0)
      goto restart;
    /* We own the lock here */
    w->no_drop = 0;
    /* Store version for validation of read set */
    w->version = LOCK_GET_TIMESTAMP(l);
    tx->w_set.nb_acquired++;
  } while (w > tx->w_set.entries);

#ifdef IRREVOCABLE_ENABLED
  /* Verify if there is an irrevocable transaction once all locks have been acquired */
# ifdef IRREVOCABLE_IMPROVED
  /* FIXME: it is bogus. the status should be changed to idle otherwise stm_quiesce will not progress */
  if (unlikely(!tx->irrevocable)) {
    do {
      t = ATOMIC_LOAD(&_tinystm.irrevocable);
      /* If the irrevocable transaction have encountered an acquired lock, abort */
      if (t == 2) {
        stm_rollback(tx, STM_ABORT_IRREVOCABLE);
        return 0;
      }
    } while (t);
  }
# else /* ! IRREVOCABLE_IMPROVED */
  if (!tx->irrevocable && ATOMIC_LOAD(&_tinystm.irrevocable)) {
    stm_rollback(tx, STM_ABORT_IRREVOCABLE);
    return 0;
  }
# endif /* ! IRREVOCABLE_IMPROVED */
#endif /* IRREVOCABLE_ENABLED */

  /* Get commit timestamp (may exceed VERSION_MAX by up to MAX_THREADS) */
  t = FETCH_INC_CLOCK + 1;

#ifdef IRREVOCABLE_ENABLED
  if (unlikely(tx->irrevocable))
    goto release_locks;
#endif /* IRREVOCABLE_ENABLED */

  /* Try to validate (only if a concurrent transaction has committed since tx->start) */
  if (unlikely(tx->start != t - 1 && !stm_wbctl_validate(tx))) {
    /* Cannot commit */
    stm_rollback(tx, STM_ABORT_VALIDATE);
    return 0;
  }

#ifdef IRREVOCABLE_ENABLED
  release_locks:
#endif /* IRREVOCABLE_ENABLED */

  /* Install new versions, drop locks and set new timestamp */
  w = tx->w_set.entries;
  for (i = tx->w_set.nb_entries; i > 0; i--, w++) {
    if (w->mask == ~(stm_word_t)0) {
      ATOMIC_STORE(w->addr, w->value);
    } else if (w->mask != 0) {
      value = (ATOMIC_LOAD(w->addr) & ~w->mask) | (w->value & w->mask);
      ATOMIC_STORE(w->addr, value);
    }
    /* Only drop lock for last covered address in write set (cannot be "no drop") */
    if (!w->no_drop)
      ATOMIC_STORE_REL(w->lock, LOCK_SET_TIMESTAMP(t));
  }

 end:
  return 1;
}

#endif /* _STM_WBCTL_H_ */
