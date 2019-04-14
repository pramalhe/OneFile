/* 
 * File:   descriptors_impl2.h
 * Author: tabrown
 *
 * Created on June 30, 2016, 11:53 AM
 */

#ifndef DESCRIPTORS_IMPL2_H
#define	DESCRIPTORS_IMPL2_H

#include "descriptors.h"

#define DESC_INIT_ALL(descArray, macro_mutablesNew, numProcesses) { \
    for (int i=0;i<(numProcesses);++i) { \
        (descArray)[i].mutables = macro_mutablesNew(0); \
    } \
}

/**
 * mutables_t corresponds to the mutables field of the descriptor.
 * it contains the mutable fields of the descriptor and a sequence number.
 * the width, offset and mask for the sequence number is defined below.
 * this sequence number width, offset and mask are also shared by tagptr_t.
 *
 * in particular, for any tagptr_t x and mutables_t y, the sequence numbers
 * in x and y are equal iff x&MASK_SEQ == y&MASK_SEQ (despite the differing
 * types of x and y).
 * 
 * tagptr_t consists of a triple <seq, tid, testbit>.
 * these three fields are defined by the TAGPTR_ macros below.
 */

#ifndef WIDTH_SEQ
    #define WIDTH_SEQ 48
#endif
#define OFFSET_SEQ 14
#define MASK_SEQ ((uintptr_t)((1LL<<WIDTH_SEQ)-1)<<OFFSET_SEQ) /* cast to avoid signed bit shifting */
#define UNPACK_SEQ(tagptrOrMutables) (((uintptr_t)(tagptrOrMutables))>>OFFSET_SEQ)

#define TAGPTR_OFFSET_USER 0
#define TAGPTR_OFFSET_TID 3
#define TAGPTR_MASK_USER ((1<<TAGPTR_OFFSET_TID)-1) /* assumes TID is next field after USER */
#define TAGPTR_MASK_TID (((1<<OFFSET_SEQ)-1)&(~(TAGPTR_MASK_USER)))
#define TAGPTR_UNPACK_TID(tagptr) ((int) ((((tagptr_t) (tagptr))&TAGPTR_MASK_TID)>>TAGPTR_OFFSET_TID))
#define TAGPTR_UNPACK_PTR(descArray, tagptr) (&(descArray)[TAGPTR_UNPACK_TID((tagptr))])
#define TAGPTR_NEW(tid, mutables, userBits) ((tagptr_t) (((UNPACK_SEQ(mutables))<<OFFSET_SEQ) | ((tid)<<TAGPTR_OFFSET_TID) | (tagptr_t) (userBits)<<TAGPTR_OFFSET_USER))
// assert: there is no thread with tid DUMMY_TID that ever calls TAGPTR_NEW
#define LAST_TID (TAGPTR_MASK_TID>>TAGPTR_OFFSET_TID)
#define TAGPTR_STATIC_DESC(id) ((tagptr_t) TAGPTR_NEW(LAST_TID-1-id, 0))
#define TAGPTR_DUMMY_DESC(id) ((tagptr_t) TAGPTR_NEW(LAST_TID, id<<OFFSET_SEQ))

#define comma ,

#define MUTABLES_UNPACK_FIELD(mutables, mask, offset) \
    ((((mutables_t) (mutables))&(mask))>>(offset))
// TODO: make more efficient version "MUTABLES_CAS_BIT"
// TODO: change sequence # unpacking to masking for quick comparison
// note: if there is only one subfield besides seq#, then the third if-block is redundant, and you should just return false if the cas fails, since the only way the cas fails and the field being cas'd contains still old is if the sequence number has changed.
#define MUTABLES_BOOL_CAS_FIELD(successBit, fldMutables, snapMutables, oldval, val, mask, offset) { \
    mutables_t __v = (fldMutables); \
    while (1) { \
        if (UNPACK_SEQ(__v) != UNPACK_SEQ((snapMutables))) { \
            (successBit) = false; \
            break; \
        } \
        if ((successBit) = __sync_bool_compare_and_swap(&(fldMutables), \
                (__v & ~(mask)) | ((oldval)<<(offset)), \
                (__v & ~(mask)) | ((val)<<(offset)))) { \
            break; \
        } \
        __v = (fldMutables); \
        if (MUTABLES_UNPACK_FIELD(__v, (mask), (offset)) != (oldval)) { \
            (successBit) = false; \
            break; \
        } \
    } \
}

#define MUTABLES_VAL_CAS_FIELD(failedBit, retval, fldMutables, snapMutables, oldval, val, mask, offset) { \
    mutables_t __v = (fldMutables); \
    while (1) { \
        if (UNPACK_SEQ(__v) != UNPACK_SEQ((snapMutables))) { \
            (failedBit) = true; /* version number has changed, CAS cannot occur */ \
            break; \
        } \
        mutables_t __oldval = (__v & ~(mask)) | ((oldval)<<(offset)); \
        (retval) = __sync_val_compare_and_swap(&(fldMutables), \
                __oldval, \
                (__v & ~(mask)) | ((val)<<(offset))); \
        if ((retval) == __oldval) { /* CAS SUCCESS */ \
            (retval) = MUTABLES_UNPACK_FIELD((retval), (mask), (offset)); /* return contents of subfield */ \
            (failedBit) = false; \
            break; \
        } else { /* CAS FAILURE: should we retry? */ \
            __v = (retval); /* save the value that caused our CAS to fail, in case we need to retry */ \
            (retval) = MUTABLES_UNPACK_FIELD((retval), (mask), (offset)); /* return contents of subfield */ \
            if ((retval) != (oldval)) { /* check if we failed because the subfield's contents do not match oldval */ \
                (failedBit) = false; \
                break; \
            } \
            /* subfield's contents DO match oldval, so we need to try again */ \
        } \
    } \
}

// TODO: change sequence # unpacking to masking for quick comparison
// note: MUTABLES_FAA_FIELD would be very similar to MUTABLES_BOOL_CAS_FIELD; i think one would simply delete the last if block and change the new val from (val)<<offset to (val&mask)+1.
#define MUTABLES_WRITE_FIELD(fldMutables, snapMutables, val, mask, offset) { \
    mutables_t __v = (fldMutables); \
    while (UNPACK_SEQ(__v) == UNPACK_SEQ((snapMutables)) \
            && MUTABLES_UNPACK_FIELD(__v, (mask), (offset)) != (val) \
            && !__sync_bool_compare_and_swap(&(fldMutables), __v, \
                    (__v & ~(mask)) | ((val)<<(offset)))) { \
        __v = (fldMutables); \
    } \
}
#define MUTABLES_WRITE_BIT(fldMutables, snapMutables, mask) { \
    mutables_t __v = (fldMutables); \
    while (UNPACK_SEQ(__v) == UNPACK_SEQ((snapMutables)) \
            && !(__v&(mask)) \
            && !__sync_bool_compare_and_swap(&(fldMutables), __v, (__v|(mask)))) { \
        __v = (fldMutables); \
    } \
}

// WARNING: uses a GCC extension "({ })". to get rid of this, use an inline function.
#define DESC_SNAPSHOT(descType, descArray, descDest, tagptr, sz) ({ \
    descType *__src = TAGPTR_UNPACK_PTR((descArray), (tagptr)); \
    memcpy((descDest), __src, (sz)); \
    SOFTWARE_BARRIER; /* prevent compiler from reordering read of __src->mutables before (at least the reading portion of) the memcpy */ \
    (UNPACK_SEQ(__src->mutables) == UNPACK_SEQ((tagptr))); \
})
#define DESC_READ_FIELD(successBit, fldMutables, tagptr, mask, offset) ({ \
    mutables_t __mutables = (fldMutables); \
    successBit = (__mutables & MASK_SEQ) == ((tagptr) & MASK_SEQ); \
    MUTABLES_UNPACK_FIELD(__mutables, (mask), (offset)); \
})
#define DESC_NEW(descArray, macro_mutablesNew, tid) &(descArray)[(tid)]; { /* note: only the process invoking this following macro can change the sequence# */ \
    SOFTWARE_BARRIER; \
    mutables_t __v = (descArray)[(tid)].mutables; \
    (descArray)[(tid)].mutables = macro_mutablesNew(__v); \
    SOFTWARE_BARRIER; \
    /*__sync_synchronize();*/ \
}
#define DESC_INITIALIZED(descArray, tid) \
    SOFTWARE_BARRIER; \
    (descArray)[(tid)].mutables += (1<<OFFSET_SEQ); \
    SOFTWARE_BARRIER;

#endif	/* DESCRIPTORS_IMPL2_H */

