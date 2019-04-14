/* Copyright (C) 2009, 2010, 2011 Free Software Foundation, Inc.
   Contributed by Richard Henderson <rth@redhat.com>.

   This file is part of the GNU Transactional Memory Library (libitm).

   Libitm is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   Libitm is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.

   This file was modified to allow compatibility with the GNU Transactional
   Memory Library (libitm). */

/* No include needed since the file is included */

struct clone_entry
{
  void *orig, *clone;
};

struct clone_table
{
  struct clone_entry *table;
  size_t size;
  struct clone_table *next;
};

static struct clone_table *all_tables;

static void *
find_clone (void *ptr)
{
  struct clone_table *table;

  for (table = all_tables; table ; table = table->next)
    {
      struct clone_entry *t = table->table;
      size_t lo = 0, hi = table->size, i;

      /* Quick test for whether PTR is present in this table.  */
      if (ptr < t[0].orig || ptr > t[hi - 1].orig)
	continue;

      /* Otherwise binary search.  */
      while (lo < hi)
	{
	  i = (lo + hi) / 2;
	  if (ptr < t[i].orig)
	    hi = i;
	  else if (ptr > t[i].orig)
	    lo = i + 1;
	  else
	    {
	      return t[i].clone;
	    }
	}

      /* Given the quick test above, if we don't find the entry in
	 this table then it doesn't exist.  */
      break;
    }
  return NULL;
}


void * _ITM_CALL_CONVENTION
_ITM_getTMCloneOrIrrevocable (void *ptr)
{
  // if the function (ptr) have a TM version, give the pointer to the TM function 
  // otherwise, set transaction to irrevocable mode
  void *ret = find_clone (ptr);
  if (ret)
    return ret;

  /* TODO Check we are in an active transaction */
  //  if (stm_current_tx() != NULL && stm_is_active(tx))
    /* GCC always use implicit transaction descriptor */
    stm_set_irrevocable(1);

  return ptr;
}

void * _ITM_CALL_CONVENTION
_ITM_getTMCloneSafe (void *ptr)
{
  void *ret = find_clone(ptr);
  if (ret == NULL) {
    fprintf(stderr, "libitm: cannot find clone for %p\n", ptr);
    abort();
  }
  return ret;
}

static int
clone_entry_compare (const void *a, const void *b)
{
  const struct clone_entry *aa = (const struct clone_entry *)a;
  const struct clone_entry *bb = (const struct clone_entry *)b;

  if (aa->orig < bb->orig)
    return -1;
  else if (aa->orig > bb->orig)
    return 1;
  else
    return 0;
}

void
_ITM_registerTMCloneTable (void *xent, size_t size)
{
  struct clone_entry *ent = (struct clone_entry *)(xent);
  struct clone_table *old, *table;

  table = (struct clone_table *) malloc (sizeof (struct clone_table));
  table->table = ent;
  table->size = size;

  qsort (ent, size, sizeof (struct clone_entry), clone_entry_compare);

  old = all_tables;
  do
    {
      table->next = old;
      /* TODO Change to use AtomicOps wrapper */
      old = __sync_val_compare_and_swap (&all_tables, old, table);
    }
  while (old != table);
}

void
_ITM_deregisterTMCloneTable (void *xent)
{
  struct clone_entry *ent = (struct clone_entry *)(xent);
  struct clone_table **pprev = &all_tables;
  struct clone_table *tab;

  /* FIXME: we must make sure that no transaction is active at this point. */

  for (pprev = &all_tables;
       tab = *pprev, tab->table != ent;
       pprev = &tab->next)
    continue;
  *pprev = tab->next;

  free (tab);
}

