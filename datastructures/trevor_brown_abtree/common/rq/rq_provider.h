/* 
 * File:   rq_provider.h
 * Author: trbot
 *
 * Created on May 16, 2017, 5:14 PM
 */

#ifndef RQ_PROVIDER_H
#define RQ_PROVIDER_H

#if defined RQ_LOCKFREE
    #include "rq_dcssp.h"
#elif defined RQ_RWLOCK
    #include "rq_rwlock.h"
#elif defined RQ_HTM_RWLOCK
    #include "rq_htm_rwlock.h"
#elif defined RQ_UNSAFE
    #include "rq_unsafe.h"
#elif defined RQ_SNAPCOLLECTOR
    #include "rq_snapcollector.h"
#else
    #warning "No range query method specified... using non-linearizable range queries. See rq_provider.h for other options."
    #define RQ_UNSAFE
    #include "rq_unsafe.h"
    //#error NO RQ PROVIDER DEFINED
#endif

#endif /* RQ_PROVIDER_H */

