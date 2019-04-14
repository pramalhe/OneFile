include Makefile.common

########################################################################
# TinySTM can be configured in many ways.  The main compilation options
# are described below.  To read more easily through the code, you can
# generate a source file stripped from most of the conditional
# preprocessor directives using:
#
#   make src/stm.o.c
#
# For more details on the LSA algorithm and the design of TinySTM, refer
# to:
#
# [DISC-06] Torvald Riegel, Pascal Felber, and Christof Fetzer.  A Lazy
#   Snapshot Algorithm with Eager Validation.  20th International
#   Symposium on Distributed Computing (DISC), 2006.
#
# [PPoPP-08] Pascal Felber, Christof Fetzer, and Torvald Riegel.
#   Dynamic Performance Tuning of Word-Based Software Transactional
#   Memory.  Proceedings of the 13th ACM SIGPLAN Symposium on Principles
#   and Practice of Parallel Programming (PPoPP), 2008.
########################################################################

########################################################################
# Three different designs can be chosen from, which differ in when locks
# are acquired (encounter-time vs. commit-time), and when main memory is
# updated (write-through vs. write-back).
#
# WRITE_BACK_ETL: write-back with encounter-time locking acquires lock
#   when encountering write operations and buffers updates (they are
#   committed to main memory at commit time).
#
# WRITE_BACK_CTL: write-back with commit-time locking delays acquisition
#   of lock until commit time and buffers updates.
#
# WRITE_THROUGH: write-through (encounter-time locking) directly updates
#   memory and keeps an undo log for possible rollback.
#
# Refer to [PPoPP-08] for more details.
########################################################################

DEFINES += -DDESIGN=WRITE_BACK_ETL
# DEFINES += -DDESIGN=WRITE_BACK_CTL
# DEFINES += -DDESIGN=WRITE_THROUGH
# DEFINES += -DDESIGN=MODULAR

########################################################################
# Several contention management strategies are available:
#
# CM_SUICIDE: immediately abort the transaction that detects the
#   conflict.
#
# CM_DELAY: like CM_SUICIDE but wait until the contended lock that
#   caused the abort (if any) has been released before restarting the
#   transaction.  The intuition is that the transaction will likely try
#   again to acquire the same lock and might fail once more if it has
#   not been released.  In addition, this increases the chances that the
#   transaction can succeed with no interruption upon retry, which
#   improves execution time on the processor.
#
# CM_BACKOFF: like CM_SUICIDE but wait for a random delay before
#   restarting the transaction.  The delay duration is chosen uniformly
#   at random from a range whose size increases exponentially with every
#   restart.
#
# CM_MODULAR: supports several built-in contention managers.  At the
#   time, the following ones are supported:
#   - SUICIDE: kill current transaction (i.e., the transaction that
#     discovers the conflict).
#   - AGGRESSIVE: kill other transaction.
#   - DELAY: same as SUICIDE but wait for conflict resolution before
#     restart.
#   - TIMESTAMP: kill youngest transaction.
#   One can also register custom contention managers.
########################################################################

# Pick one contention manager (CM)
DEFINES += -DCM=CM_SUICIDE
# DEFINES += -DCM=CM_DELAY
# DEFINES += -DCM=CM_BACKOFF
# DEFINES += -DCM=CM_MODULAR

########################################################################
# Enable irrevocable mode (required for using the library with a
# compiler).
########################################################################

DEFINES += -DIRREVOCABLE_ENABLED
# DEFINES += -UIRREVOCABLE_ENABLED

########################################################################
# Maintain detailed internal statistics.  Statistics are stored in
# thread locals and do not add much overhead, so do not expect much gain
# from disabling them.
########################################################################

# DEFINES += -DTM_STATISTICS
DEFINES += -UTM_STATISTICS
# DEFINES += -DTM_STATISTICS2
DEFINES += -UTM_STATISTICS2

########################################################################
# Prevent duplicate entries in read/write sets when accessing the same
# address multiple times.  Enabling this option may reduce performance
# so leave it disabled unless transactions repeatedly read or write the
# same address.
########################################################################

# DEFINES += -DNO_DUPLICATES_IN_RW_SETS
DEFINES += -UNO_DUPLICATES_IN_RW_SETS

########################################################################
# Yield the processor when waiting for a contended lock to be released.
# This only applies to the DELAY and CM_MODULAR contention managers.
########################################################################

# DEFINES += -DWAIT_YIELD
DEFINES += -UWAIT_YIELD

########################################################################
# Use a (degenerate) bloom filter for quickly checking in the write set
# whether an address has previously been written.  This approach is
# directly inspired by TL2.  It only applies to the WRITE_BACK_CTL
# design.
########################################################################

# DEFINES += -DUSE_BLOOM_FILTER
DEFINES += -UUSE_BLOOM_FILTER

########################################################################
# Use an epoch-based memory allocator and garbage collector to ensure
# that accesses to the dynamic memory allocated by a transaction from
# another transaction are valid.  There is a slight overhead from
# enabling this feature.
########################################################################

# DEFINES += -DEPOCH_GC
DEFINES += -UEPOCH_GC

########################################################################
# Keep track of conflicts between transactions and notifies the
# application (using a callback), passing the identity of the two
# conflicting transaction and the associated threads.  This feature
# requires EPOCH_GC.
########################################################################

# DEFINES += -DCONFLICT_TRACKING
DEFINES += -UCONFLICT_TRACKING

########################################################################
# Allow transactions to read the previous version of locked memory
# locations, as in the original LSA algorithm (see [DISC-06]).  This is
# achieved by peeking into the write set of the transaction that owns
# the lock.  There is a small overhead with non-contended workloads but
# it may significantly reduce the abort rate, especially with
# transactions that read much data.  This feature only works with the
# WRITE_BACK_ETL design and MODULAR contention manager.
########################################################################

# DEFINES += -DREAD_LOCKED_DATA
DEFINES += -UREAD_LOCKED_DATA

########################################################################
# Tweak the hash function that maps addresses to locks so that
# consecutive addresses do not map to consecutive locks.  This can avoid
# cache line invalidations for application that perform sequential
# memory accesses.  The last byte of the lock index is swapped with the
# previous byte.
########################################################################

# DEFINES += -DLOCK_IDX_SWAP
DEFINES += -ULOCK_IDX_SWAP

########################################################################
# Output many (DEBUG) or even mode (DEBUG2) debugging messages.
########################################################################

# DEFINES += -DDEBUG
DEFINES += -UDEBUG
# DEFINES += -DDEBUG2
DEFINES += -UDEBUG2

########################################################################
# Catch SIGBUS and SIGSEGV signals
########################################################################

# DEFINES += -DSIGNAL_HANDLER
DEFINES += -USIGNAL_HANDLER

# TODO Enable the construction of 32bit lib on 64bit environment 

########################################################################
# Use COMPILER thread-local storage (TLS) support by default
# TLS_COMPILER: use __thread keyword
# TLS_POSIX: use posix (pthread) functions
# TLS_DARWIN: use posix inline functions
# TLS_GLIBC: use the space reserved for TM in the GLIBC
########################################################################

DEFINES += -DTLS_COMPILER
# DEFINES += -DTLS_POSIX
# DEFINES += -DTLS_DARWIN
# DEFINES += -DTLS_GLIBC

########################################################################
# Enable unit transaction
########################################################################

# DEFINES += -DUNIT_TX
DEFINES += -UUNIT_TX

########################################################################
# Various default values can also be overridden:
#
# RW_SET_SIZE (default=4096): initial size of the read and write
#   sets.  These sets will grow dynamically when they become full.
#
# LOCK_ARRAY_LOG_SIZE (default=20): number of bits used for indexes in
#   the lock array.  The size of the array will be 2 to the power of
#   LOCK_ARRAY_LOG_SIZE.
#
# LOCK_SHIFT_EXTRA (default=2): additional shifts to apply to the
#   address when determining its index in the lock array.  This controls
#   how many consecutive memory words will be covered by the same lock
#   (2 to the power of LOCK_SHIFT_EXTRA).  Higher values will increase
#   false sharing but reduce the number of CASes necessary to acquire
#   locks and may avoid cache line invalidations on some workloads.  As
#   shown in [PPoPP-08], a value of 2 seems to offer best performance on
#   many benchmarks.
#
# MIN_BACKOFF (default=0x04UL) and MAX_BACKOFF (default=0x80000000UL):
#   minimum and maximum values of the exponential backoff delay.  This
#   parameter is only used with the CM_BACKOFF contention manager.
#
# VR_THRESHOLD_DEFAULT (default=3): number of aborts due to failed
#   validation before switching to visible reads.  A value of 0
#   indicates no limit.  This parameter is only used with the
#   CM_MODULAR contention manager.  It can also be set using an
#   environment variable of the same name.
########################################################################

# DEFINES += -DRW_SET_SIZE=4096
# DEFINES += -DLOCK_ARRAY_LOG_SIZE=20
# DEFINES += -DLOCK_SHIFT_EXTRA=2
# DEFINES += -DMIN_BACKOFF=0x04UL
# DEFINES += -DMAX_BACKOFF=0x80000000UL
# DEFINES += -DVR_THRESHOLD_DEFAULT=3

########################################################################
# Do not modify anything below this point!
########################################################################

# Replace textual values by constants for unifdef...
D := $(DEFINES)
D := $(D:WRITE_BACK_ETL=0)
D := $(D:WRITE_BACK_CTL=1)
D := $(D:WRITE_THROUGH=2)
D := $(D:MODULAR=3)
D += -DWRITE_BACK_ETL=0 -DWRITE_BACK_CTL=1 -DWRITE_THROUGH=2 -DMODULAR=3
D := $(D:CM_SUICIDE=0)
D := $(D:CM_DELAY=1)
D := $(D:CM_BACKOFF=2)
D := $(D:CM_MODULAR=3)
D += -DCM_SUICIDE=0 -DCM_DELAY=1 -DCM_BACKOFF=2 -DCM_MODULAR=3

ifneq (,$(findstring -DEPOCH_GC,$(DEFINES)))
  GC := $(SRCDIR)/gc.o
else
  GC :=
endif

CPPFLAGS += -I$(SRCDIR)
CPPFLAGS += $(DEFINES)

MODULES := $(patsubst %.c,%.o,$(wildcard $(SRCDIR)/mod_*.c))

.PHONY:	all doc test abi clean check

all:	$(TMLIB)

%.o:	%.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -DCOMPILE_FLAGS="$(CPPFLAGS) $(CFLAGS)" -c -o $@ $<

# Additional dependencies
$(SRCDIR)/stm.o:	$(INCDIR)/stm.h
$(SRCDIR)/stm.o:	$(SRCDIR)/stm_internal.h $(SRCDIR)/stm_wt.h $(SRCDIR)/stm_wbetl.h $(SRCDIR)/stm_wbctl.h $(SRCDIR)/tls.h $(SRCDIR)/utils.h $(SRCDIR)/atomic.h

%.s:	%.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) -DCOMPILE_FLAGS="$(CPPFLAGS) $(CFLAGS)" -fverbose-asm -S -o $@ $<

%.o.c:	%.c Makefile
	$(UNIFDEF) $(D) $< > $@ || true

$(TMLIB):	$(SRCDIR)/$(TM).o $(SRCDIR)/wrappers.o $(GC) $(MODULES)
	$(AR) crus $@ $^

test:	$(TMLIB)
	$(MAKE) -C test

abi:
	$(MAKE) -C abi

abi-%: 	
	$(MAKE) -C abi $(subst abi-,,$@)

doc:
	$(DOXYGEN)

check: 	$(TMLIB)
	$(MAKE) -C test check

# TODO add an install rule
#install: 	$(TMLIB)

clean:
	rm -f $(TMLIB) $(SRCDIR)/*.o
	$(MAKE) -C abi clean
	TARGET=clean $(MAKE) -C test

