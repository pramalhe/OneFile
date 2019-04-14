/* Copyright (C) 2009, 2011, 2012 Free Software Foundation, Inc.
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

/* FIXME Exception support with GCC is not working. */

extern void *__cxa_allocate_exception (size_t) __attribute__((weak));
extern void __cxa_throw (void *, void *, void *) __attribute__((weak));
extern void *__cxa_begin_catch (void *) __attribute__((weak));
extern void *__cxa_end_catch (void) __attribute__((weak));
/* TODO check if TM_ABI -> no problem of dependancy of gcc-tm  */
extern void __cxa_tm_cleanup (void *, void *, unsigned int) __attribute__((weak));


void *_ITM_cxa_allocate_exception (size_t size)
{
  void *r = __cxa_allocate_exception (size);
  /*tx->cxa_unthrown = r;*/
  return r;
}

void _ITM_cxa_throw (void *obj, void *tinfo, void *dest)
{
  /*tx->cxa_unthrown = NULL;*/
  __cxa_throw (obj, tinfo, dest);
}

void *_ITM_cxa_begin_catch (void *exc_ptr)
{
  /*tx->cxa_catch_count++;*/
  return __cxa_begin_catch (exc_ptr);
}

void _ITM_cxa_end_catch (void)
{
  /*tx->cxa_catch_count--;*/
  __cxa_end_catch ();
}

/* On rollback */
/*
 * TODO integrate this to completely makes work exception with GCC-TM
void stm_revert_cpp_exceptions (void)
{   
  if (tx->cxa_unthrown || tx->cxa_catch_count) {
    __cxa_tm_cleanup (tx->cxa_unthrown, tx->eh_in_flight,
                      tx->cxa_catch_count);
    tx->cxa_catch_count = 0;
    tx->cxa_unthrown = NULL;
    tx->eh_in_flight = NULL;
  }
  if (tx->eh_in_flight) {
    _Unwind_DeleteException ((_Unwind_Exception *) tx->eh_in_flight);
    tx->eh_in_flight = NULL;
  }
}

in _ITM_commitTransactionEH
tx->eh_in_flight = exc_ptr;
in _ITM_beginTransaction (no nesting)
tx->eh_in_flight = NULL;
*/

