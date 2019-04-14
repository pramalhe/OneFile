/*
 * File:
 *   stm_wt.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   STM internal functions.
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

#ifndef _STM_WT_H_
#define _STM_WT_H_

static INLINE int
stm_wt_validate(stm_tx_t *tx)
{
  r_entry_t *r;
  int i;
  stm_word_t l;

  PRINT_DEBUG("==> stm_wt_validate(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

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
stm_wt_extend(stm_tx_t *tx)
{
  stm_word_t now;

  PRINT_DEBUG("==> stm_wt_extend(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

#ifdef UNIT_TX
  /* Extension is disabled */
  if (tx->attr.no_extend)
    return 0;
#endif /* UNIT_TX */

  /* Get current time */
  now = GET_CLOCK;
  /* No need to check clock overflow here. The clock can exceed up to MAX_THREADS and it will be reset when the quiescence is reached. */

  /* Try to validate read set */
  if (stm_wt_validate(tx)) {
    /* It works: we can extend until now */
    tx->end = now;
    return 1;
  }
  return 0;
}

static INLINE void
stm_wt_rollback(stm_tx_t *tx)
{
  int i;
  w_entry_t *w;
  stm_word_t t;

  PRINT_DEBUG("==> stm_wt_rollback(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  assert(IS_ACTIVE(tx->status));

  t = 0;
  /* Undo writes and drop locks */
  w = tx->w_set.entries;
  for (i = tx->w_set.nb_entries; i > 0; i--, w++) {
    stm_word_t j;
    /* Restore previous value */
    if (w->mask != 0)
      ATOMIC_STORE(w->addr, w->value);
    if (w->next != NULL)
      continue;
    /* Incarnation numbers allow readers to detect dirty reads */
    j = LOCK_GET_INCARNATION(w->version) + 1;
    if (j > INCARNATION_MAX) {
      /* Simple approach: write new version (might trigger unnecessary aborts) */
      if (t == 0) {
        /* Get new version (may exceed VERSION_MAX by up to MAX_THREADS) */
        t = FETCH_INC_CLOCK + 1;
      }
      ATOMIC_STORE_REL(w->lock, LOCK_SET_TIMESTAMP(t));
    } else {
      /* Use new incarnation number */
      ATOMIC_STORE_REL(w->lock, LOCK_UPD_INCARNATION(w->version, j));
    }
  }
  /* Make sure that all lock releases become visible */
  ATOMIC_MB_WRITE;
}

static INLINE void
stm_wt_add_to_rs(stm_tx_t *tx, stm_word_t version, volatile stm_word_t *lock)
{
  r_entry_t *r;

  /* No need to add to read set for read-only transaction */
  if (tx->attr.read_only)
    return;

#ifdef NO_DUPLICATES_IN_RW_SETS
  if (stm_has_read(tx, lock) != NULL)
    return value;
#endif /* NO_DUPLICATES_IN_RW_SETS */

  /* Add address and version to read set */
  if (tx->r_set.nb_entries == tx->r_set.size)
    stm_allocate_rs_entries(tx, 1);
  r = &tx->r_set.entries[tx->r_set.nb_entries++];
  r->version = version;
  r->lock = lock;
}

static INLINE stm_word_t
stm_wt_read(stm_tx_t *tx, volatile stm_word_t *addr)
{
  volatile stm_word_t *lock;
  stm_word_t l, l2, value, version;
  w_entry_t *w;

  PRINT_DEBUG2("==> stm_wt_read(t=%p[%lu-%lu],a=%p)\n", tx, (unsigned long)tx->start, (unsigned long)tx->end, addr);

  assert(IS_ACTIVE(tx->status));

  /* Get reference to lock */
  lock = GET_LOCK(addr);

  /* Note: we could check for duplicate reads and get value from read set */

  /* Read lock, value, lock */
 restart:
  l = ATOMIC_LOAD_ACQ(lock);
 restart_no_load:
  if (likely(!LOCK_GET_WRITE(l))) {
    /* Not locked */
    value = ATOMIC_LOAD_ACQ(addr);
    l2 = ATOMIC_LOAD_ACQ(lock);
    if (l != l2) {
      l = l2;
      goto restart_no_load;
    }

#ifdef IRREVOCABLE_ENABLED
    /* In irrevocable mode, no need check timestamp nor add entry to read set */
    if (unlikely(tx->irrevocable))
      goto no_check;
#endif /* IRREVOCABLE_ENABLED */

    /* Check timestamp */
    version = LOCK_GET_TIMESTAMP(l);

    /* Add to read set (update transactions only) */
    stm_wt_add_to_rs(tx, version, lock);

    /* Valid version? */
    if (unlikely(version > tx->end)) {
      /* No: try to extend first (except for read-only transactions: no read set) */
      if (tx->attr.read_only || !stm_wt_extend(tx)) {
        /* Not much we can do: abort */
        stm_rollback(tx, STM_ABORT_VAL_READ);
        return 0;
      }
      /* Worked: we now have a good version (version <= tx->end) */
    }

#ifdef IRREVOCABLE_ENABLED
 no_check:
#endif /* IRREVOCABLE_ENABLED */
    /* We have a good version: return value */
    return value;
  } else {
    /* Locked */
    /* Do we own the lock? */
    w = (w_entry_t *)LOCK_GET_ADDR(l);

    /* Simply check if address falls inside our write set (avoids non-faulting load) */
    if (likely(tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries)) {
      /* Yes: we have a version locked by us that was valid at write time */
      value = ATOMIC_LOAD(addr);
      /* No need to add to read set (will remain valid) */
      return value;
    }

# ifdef UNIT_TX
    if (l == LOCK_UNIT) {
      /* Data modified by a unit store: should not last long => retry */
      goto restart;
    }
# endif /* UNIT_TX */

    /* Conflict: CM kicks in (we could also check for duplicate reads and get value from read set) */
# if defined(IRREVOCABLE_ENABLED) && defined(IRREVOCABLE_IMPROVED)
    if (tx->irrevocable && ATOMIC_LOAD(&_tinystm.irrevocable) == 1)
      ATOMIC_STORE(&_tinystm.irrevocable, 2);
# endif /* defined(IRREVOCABLE_ENABLED) && defined(IRREVOCABLE_IMPROVED) */
# if defined(IRREVOCABLE_ENABLED)
    if (tx->irrevocable) {
      /* Spin while locked */
      goto restart;
    }
# endif /* defined(IRREVOCABLE_ENABLED) */
# if CM == CM_DELAY
    tx->c_lock = lock;
# endif /* CM == CM_DELAY */

    /* Abort */
# ifdef CONFLICT_TRACKING
    if (_tinystm.conflict_cb != NULL) {
#  ifdef UNIT_TX
      if (l != LOCK_UNIT) {
#  endif /* UNIT_TX */
        /* Call conflict callback */
        stm_tx_t *other = ((w_entry_t *)LOCK_GET_ADDR(l))->tx;
        _tinystm.conflict_cb(tx, other);
#  ifdef UNIT_TX
      }
#  endif /* UNIT_TX */
    }
# endif /* CONFLICT_TRACKING */

    stm_rollback(tx, STM_ABORT_RW_CONFLICT);
    return 0;
  }
}

static INLINE w_entry_t *
stm_wt_write(stm_tx_t *tx, volatile stm_word_t *addr, stm_word_t value, stm_word_t mask)
{
  volatile stm_word_t *lock;
  stm_word_t l, version;
  w_entry_t *w;
  w_entry_t *prev = NULL;

  PRINT_DEBUG2("==> stm_wt_write(t=%p[%lu-%lu],a=%p,d=%p-%lu,m=0x%lx)\n",
               tx, (unsigned long)tx->start, (unsigned long)tx->end, addr, (void *)value, (unsigned long)value, (unsigned long)mask);

  /* Get reference to lock */
  lock = GET_LOCK(addr);

  /* Try to acquire lock */
 restart:
  l = ATOMIC_LOAD_ACQ(lock);
 restart_no_load:
  if (LOCK_GET_OWNED(l)) {
    /* Locked */

#ifdef UNIT_TX
    if (l == LOCK_UNIT) {
      /* Data modified by a unit store: should not last long => retry */
      goto restart;
    }
#endif /* UNIT_TX */

    /* Do we own the lock? */
    w = (w_entry_t *)LOCK_GET_ADDR(l);
    /* Simply check if address falls inside our write set (avoids non-faulting load) */
    if (likely(tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries)) {
      if (mask == 0) {
        /* No need to insert new entry or modify existing one */
        return w;
      }
      prev = w;
      /* Did we previously write the same address? */
      while (1) {
        if (addr == prev->addr) {
          if (w->mask == 0) {
            /* Remember old value */
            w->value = ATOMIC_LOAD(addr);
            w->mask = mask;
          }
          /* Yes: only write to memory */
          if (mask != ~(stm_word_t)0)
            value = (ATOMIC_LOAD(addr) & ~mask) | (value & mask);
          ATOMIC_STORE(addr, value);
          return w;
        }
        if (prev->next == NULL) {
          /* Remember last entry in linked list (for adding new entry) */
          break;
        }
        prev = prev->next;
      }
      /* Must add to write set */
      if (tx->w_set.nb_entries == tx->w_set.size)
        stm_rollback(tx, STM_ABORT_EXTEND_WS);
      w = &tx->w_set.entries[tx->w_set.nb_entries];
      /* Get version from previous write set entry (all entries in linked list have same version) */
      w->version = prev->version;
      goto do_write;
    }
    /* Conflict: CM kicks in */
# if defined(IRREVOCABLE_ENABLED) && defined(IRREVOCABLE_IMPROVED)
    if (tx->irrevocable && ATOMIC_LOAD(&_tinystm.irrevocable) == 1)
      ATOMIC_STORE(&_tinystm.irrevocable, 2);
# endif /* defined(IRREVOCABLE_ENABLED) && defined(IRREVOCABLE_IMPROVED) */
# if defined(IRREVOCABLE_ENABLED)
    if (tx->irrevocable) {
      /* Spin while locked */
      goto restart;
    }
# endif /* defined(IRREVOCABLE_ENABLED) */
# if CM == CM_DELAY
    tx->c_lock = lock;
# endif /* CM == CM_DELAY */

    /* Abort */
# ifdef CONFLICT_TRACKING
    if (_tinystm.conflict_cb != NULL) {
#  ifdef UNIT_TX
      if (l != LOCK_UNIT) {
#  endif /* UNIT_TX */
        /* Call conflict callback */
        stm_tx_t *other = ((w_entry_t *)LOCK_GET_ADDR(l))->tx;
        _tinystm.conflict_cb(tx, other);
#  ifdef UNIT_TX
      }
#  endif /* UNIT_TX */
    }
# endif /* CONFLICT_TRACKING */

    stm_rollback(tx, STM_ABORT_WW_CONFLICT);
    return NULL;
  }
  /* Not locked */
  /* Handle write after reads (before CAS) */
  version = LOCK_GET_TIMESTAMP(l);
#ifdef IRREVOCABLE_ENABLED
  /* In irrevocable mode, no need to revalidate */
  if (tx->irrevocable)
    goto acquire_no_check;
#endif /* IRREVOCABLE_ENABLED */
 acquire:
  if (unlikely(version > tx->end)) {
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
  if (tx->w_set.nb_entries == tx->w_set.size)
    stm_rollback(tx, STM_ABORT_EXTEND_WS);
  w = &tx->w_set.entries[tx->w_set.nb_entries];
  if (ATOMIC_CAS_FULL(lock, l, LOCK_SET_ADDR_WRITE((stm_word_t)w)) == 0)
    goto restart;
  /* We store the old value of the lock (timestamp and incarnation) */
  w->version = l;
  /* We own the lock here (ETL) */
do_write:
  /* Add address to write set */
  w->addr = addr;
  w->mask = mask;
  w->lock = lock;
  if (mask == 0) {
    /* Do not write anything */
#ifndef NDEBUG
    w->value = 0;
#endif /* ! NDEBUG */
  } else {
    /* Remember old value */
    w->value = ATOMIC_LOAD(addr);
  }
  if (mask != 0) {
    if (mask != ~(stm_word_t)0)
      value = (w->value & ~mask) | (value & mask);
    ATOMIC_STORE(addr, value);
  }
  w->next = NULL;
  if (prev != NULL) {
    /* Link new entry in list */
    prev->next = w;
  }
  tx->w_set.nb_entries++;

  return w;
}

static INLINE stm_word_t
stm_wt_RaR(stm_tx_t *tx, volatile stm_word_t *addr)
{
  /* TODO same as fast read but no need to add into the RS */
  return stm_wt_read(tx, addr);
}

static INLINE stm_word_t
stm_wt_RaW(stm_tx_t *tx, volatile stm_word_t *addr)
{
#ifndef NDEBUG
  stm_word_t l;
  w_entry_t *w;
  l = ATOMIC_LOAD_ACQ(GET_LOCK(addr));
  /* Does the lock owned? */
  assert(LOCK_GET_WRITE(l));
  /* Do we own the lock? */
  w = (w_entry_t *)LOCK_GET_ADDR(l);
  assert(tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries);
#endif /* ! NDEBUG */

  /* Read directly from memory. */
  return *addr;
}

static INLINE stm_word_t
stm_wt_RfW(stm_tx_t *tx, volatile stm_word_t *addr)
{
  /* Acquire lock as write. */
  stm_wt_write(tx, addr, 0, 0);
  /* Now the lock is owned, read directly from memory is safe. */
  return *addr;
}

static INLINE void
stm_wt_WaR(stm_tx_t *tx, volatile stm_word_t *addr, stm_word_t value, stm_word_t mask)
{
  /* Probably no optimization can be done here. */
  stm_wt_write(tx, addr, value, mask);
}

static INLINE void
stm_wt_WaW(stm_tx_t *tx, volatile stm_word_t *addr, stm_word_t value, stm_word_t mask)
{
#ifndef NDEBUG
  stm_word_t l;
  w_entry_t *w;
  l = ATOMIC_LOAD_ACQ(GET_LOCK(addr));
  /* Does the lock owned? */
  assert(LOCK_GET_WRITE(l));
  /* Do we own the lock? */
  w = (w_entry_t *)LOCK_GET_ADDR(l);
  assert(tx->w_set.entries <= w && w < tx->w_set.entries + tx->w_set.nb_entries);
  /* in WaW, mask can never be 0 */
  assert(mask != 0);
#endif /* ! NDEBUG */
  if (mask != ~(stm_word_t)0) {
    value = (ATOMIC_LOAD(addr) & ~mask) | (value & mask);
  }
  ATOMIC_STORE(addr, value);
}

static INLINE int
stm_wt_commit(stm_tx_t *tx)
{
  w_entry_t *w;
  stm_word_t t;
  int i;

  PRINT_DEBUG("==> stm_wt_commit(%p[%lu-%lu])\n", tx, (unsigned long)tx->start, (unsigned long)tx->end);

  /* Update transaction */
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
  if (unlikely(tx->start != t - 1 && !stm_wt_validate(tx))) {
    /* Cannot commit */
    stm_rollback(tx, STM_ABORT_VALIDATE);
    return 0;
  }

#ifdef IRREVOCABLE_ENABLED
  release_locks:
#endif /* IRREVOCABLE_ENABLED */

  /* Make sure that the updates become visible before releasing locks */
  ATOMIC_MB_WRITE;
  /* Drop locks and set new timestamp */
  w = tx->w_set.entries;
  for (i = tx->w_set.nb_entries; i > 0; i--, w++) {
    if (w->next == NULL) {
      /* No need for CAS (can only be modified by owner transaction) */
      ATOMIC_STORE(w->lock, LOCK_SET_TIMESTAMP(t));
    }
  }
  /* Make sure that all lock releases become visible */
  /* TODO: is ATOMIC_MB_WRITE required? */
  ATOMIC_MB_WRITE;
end:
  return 1;
}

#endif /* _STM_WT_H_ */

