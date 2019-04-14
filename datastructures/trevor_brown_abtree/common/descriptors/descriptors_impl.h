/* 
 * File:   descriptors_impl.h
 * Author: tabrown
 *
 * Created on June 9, 2016, 4:04 PM
 */

#ifndef DESCRIPTORS_IMPL_H
#define	DESCRIPTORS_IMPL_H

#include "descriptors.h"

#if !defined(DESC1_T) || !defined(DESC1_ARRAY) || !defined(MUTABLES1_NEW)
#error "Must define DESC1_T, DESC1_ARRAY, MUTABLES1_NEW before including descriptors_impl.h"
#endif

#define DESC1_INIT_ALL(numProcesses) { \
    for (int i=0;i<(numProcesses);++i) { \
        DESC1_ARRAY[i].c.mutables = MUTABLES1_NEW(0); \
    } \
}

/**
 * pointer to descriptor (listed least to most significant):
 * 1-bit: is descriptor
 * 10-bits: thread id
 * remaining bits: sequence number (for avoiding the ABA problem)
 */

/**
 * mutables_t corresponds to the mutables field of the descriptor.
 * it contains the mutable fields of the descriptor and a sequence number.
 * the width, offset and mask for the sequence number is defined below.
 * this sequence number width, offset and mask are also shared by tagptr_t.
 *
 * in particular, for any tagptr_t x and mutables_t y, the sequence numbers
 * in x and y are equal iff x&MASK1_SEQ == y&MASK1_SEQ (despite the differing
 * types of x and y).
 * 
 * tagptr_t consists of a triple <seq, tid, testbit>.
 * these three fields are defined by the TAGPTR_ macros below.
 */

#ifndef WIDTH1_SEQ
    #define WIDTH1_SEQ 48
#endif
#define OFFSET1_SEQ 11
#define MASK1_SEQ ((uintptr_t)((1LL<<WIDTH1_SEQ)-1)<<OFFSET1_SEQ) /* cast to avoid signed bit shifting */
#define UNPACK1_SEQ(tagptrOrMutables) (((uintptr_t)(tagptrOrMutables))>>OFFSET1_SEQ)

#define TAGPTR1_OFFSET_STALE 0 /* UNUSED */
#define TAGPTR1_OFFSET_TID 1
#define TAGPTR1_MASK_STALE 0x1 /* UNUSED */
#define TAGPTR1_MASK_TID (((1<<OFFSET1_SEQ)-1)&(~(TAGPTR1_MASK_STALE)))
#define TAGPTR1_STALE(tagptr) (((tagptr_t) (tagptr)) & TAGPTR1_MASK_STALE) /* UNUSED */
#define TAGPTR1_UNPACK_TID(tagptr) ((int) ((((tagptr_t) (tagptr))&TAGPTR1_MASK_TID)>>TAGPTR1_OFFSET_TID))
#define TAGPTR1_UNPACK_PTR(tagptr) (&DESC1_ARRAY[TAGPTR1_UNPACK_TID((tagptr))])
#define TAGPTR1_NEW(tid, mutables) ((tagptr_t) (((UNPACK1_SEQ(mutables))<<OFFSET1_SEQ) | ((tid)<<TAGPTR1_OFFSET_TID)))
// assert: there is no thread with tid DUMMY_TID that ever calls TAGPTR1_NEW
#define LAST_TID1 (TAGPTR1_MASK_TID>>TAGPTR1_OFFSET_TID)
#define TAGPTR1_STATIC_DESC(id) ((tagptr_t) TAGPTR1_NEW(LAST_TID1-1-id, 0))
#define TAGPTR1_DUMMY_DESC(id) ((tagptr_t) TAGPTR1_NEW(LAST_TID1, id<<OFFSET1_SEQ))

#define comma1 ,

#define MUTABLES1_UNPACK_FIELD(mutables, mask, offset) \
    ((((mutables_t) (mutables))&(mask))>>(offset))
#define MUTABLES1_WRITE_FIELD(fldMutables, snapMutables, val, mask, offset) { \
    mutables_t __v = (fldMutables); \
    while (UNPACK1_SEQ(__v) == UNPACK1_SEQ((snapMutables)) \
            && MUTABLES1_UNPACK_FIELD(__v, (mask), (offset)) != (val) \
            && !__sync_bool_compare_and_swap(&(fldMutables), __v, \
                    (__v & ~(mask)) | ((val)<<(offset)))) { \
        __v = (fldMutables); \
    } \
}
#define MUTABLES1_WRITE_BIT(fldMutables, snapMutables, mask) { \
    mutables_t __v = (fldMutables); \
    while (UNPACK1_SEQ(__v) == UNPACK1_SEQ((snapMutables)) \
            && !(__v&(mask)) \
            && !__sync_bool_compare_and_swap(&(fldMutables), __v, (__v|(mask)))) { \
        __v = (fldMutables); \
    } \
}

// WARNING: uses a GCC extension "({ })". to get rid of this, use an inline function.
#define DESC1_SNAPSHOT(descDest, tagptr, sz) ({ \
    DESC1_T *__src = TAGPTR1_UNPACK_PTR((tagptr)); \
    memcpy((descDest), __src, (sz)); \
    SOFTWARE_BARRIER; /* prevent compiler from reordering read of __src->mutables before (at least the reading portion of) the memcpy */ \
    (UNPACK1_SEQ(__src->c.mutables) == UNPACK1_SEQ((tagptr))); \
})
#define DESC1_READ_FIELD(successBit, fldMutables, tagptr, mask, offset) ({ \
    mutables_t __mutables = (fldMutables); \
    successBit = (UNPACK1_SEQ(__mutables) == UNPACK1_SEQ(tagptr)); \
    MUTABLES1_UNPACK_FIELD(__mutables, (mask), (offset)); \
})
#define DESC1_NEW(tid) &DESC1_ARRAY[(tid)]; { /* note: only the process invoking this following macro can change the sequence# */ \
    SOFTWARE_BARRIER; \
    uintptr_t __v = DESC1_ARRAY[(tid)].c.mutables; \
/*    while (!__sync_bool_compare_and_swap(&DESC1_ARRAY[(tid)].mutables, __v, MUTABLES1_NEW(__v))) { \
        __v = DESC1_ARRAY[(tid)].mutables; \
    } \
}*/ \
    DESC1_ARRAY[(tid)].c.mutables = MUTABLES1_NEW(__v); \
    /*__sync_synchronize();*/ \
    SOFTWARE_BARRIER; \
}
#define DESC1_INITIALIZED(tid) \
    SOFTWARE_BARRIER; \
    DESC1_ARRAY[(tid)].c.mutables += (1<<OFFSET1_SEQ); /*DESC1_NEW((tid))*/ \
    SOFTWARE_BARRIER;

#endif	/* DESCRIPTORS_IMPL_H */

