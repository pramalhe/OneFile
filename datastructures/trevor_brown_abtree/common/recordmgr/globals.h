/**
 * C++ record manager implementation (PODC 2015) by Trevor Brown.
 * 
 * Copyright (C) 2015 Trevor Brown
 *
 */

#ifndef RECORDMGR_GLOBALS_H
#define	RECORDMGR_GLOBALS_H

#include "plaf.h"
#include "debugprinting.h"

#ifndef DEBUG
#define DEBUG if(0)
#define DEBUG2 if(0)
#endif

#ifndef MEMORY_STATS
#define MEMORY_STATS if(0)
#define MEMORY_STATS2 if(0)
#endif

// don't touch these options for crash recovery

#define CRASH_RECOVERY_USING_SETJMP
#define SEND_CRASH_RECOVERY_SIGNALS
#define AFTER_NEUTRALIZING_SET_BIT_AND_RETURN_TRUE
#define PERFORM_RESTART_IN_SIGHANDLER
#define SIGHANDLER_IDENTIFY_USING_PTHREAD_GETSPECIFIC

// some useful, data structure agnostic definitions

typedef bool CallbackReturn;
typedef void* CallbackArg;
typedef CallbackReturn (*CallbackType)(CallbackArg);
#ifndef SOFTWARE_BARRIER
#define SOFTWARE_BARRIER asm volatile("": : :"memory")
#endif

#endif	/* GLOBALS_H */
