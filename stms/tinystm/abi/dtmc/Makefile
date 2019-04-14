# Path to tinySTM
ROOT = ../..

.PHONY:	dtmc all clean check

all: 	dtmc

# ROOT must be defined to include Makefile.common
include $(ROOT)/abi/Makefile.common

##############################################################################
## DTMC
##############################################################################
LLVMCC ?= llvm-gcc
LLVMLD ?= llvm-ld
LLC    ?= llc

CPPFLAGS += -DTM_DTMC -I. 

# TODO find a way to avoid compiling not
#LLVMCC_OK = $(shell $(LLVMCC) ; if [ $$? -neq 0 ] ; then echo "no differences" ; fi)
#@$(echo) "TEST"
#if [ $$? -eq 0 ] ; then echo "no differences" ; fi
#$(shell echo $(LLVMCC_OK))

# -DNO_STACK_CHECK => shouldn't be used
# TODO Add possibility to create 32 bits lib => llvm-ld needs -m32?
# TODO find a good way to add dependencies abi.c tanger.c arch.S and also the lib (otherwise it will not be built or updated)
#%.bc:   ../%.c
#	$(LLVMCC) $(CPPFLAGS) $(CFLAGS) $(DEF_ABI) -emit-llvm -c -o $@ $<

dtmc: 	dtmc-bc dtmc-static

libtanger-stm.bc: 	../abi.c tanger.c tanger.h
	$(LLVMCC) $(CPPFLAGS) $(CFLAGS) $(DEF_ABI) -emit-llvm -c -o abi.bc ../abi.c
	$(LLVMLD) -link-as-library -o $@ abi.bc

arch.o: 	arch.S
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<

dtmc-static: 	libtanger-stm.bc arch.o 
	$(LLC) libtanger-stm.bc -o libtanger-stm.s
	$(CC) $(CPPFLAGS) $(CFLAGS) -c libtanger-stm.s -o libtanger-stm.o
	$(AR) cru libitm.a libtanger-stm.o arch.o

dtmc-bc: 	libtanger-stm.bc arch.o
	$(AR) cru libtanger-stm.a arch.o

##############################################################################

TANGERDIR       ?= YOU_DID_NOT_SET_TANGERDIR
LLVMBUILD       ?= $(shell llvm-config --build-mode)
ifeq ($(shell uname),Darwin)
  LIBTYPE       ?= dylib
else
  LIBTYPE       ?= so
endif
#STMLIBDIR       ?= $(TANGERDIR)/$(LLVMBUILD)/lib/
STMLIBDIR       ?= .
STMSUPPORTDIR   ?= $(TANGERDIR)/$(LLVMBUILD)/lib/
TANGERPASS      ?= $(TANGERDIR)/$(LLVMBUILD)/lib/libtanger.$(LIBTYPE)
TMLINK          ?= $(TANGERDIR)/$(LLVMBUILD)/bin/tmlink
TMLINKFLAGS     ?= -stmsupport=$(STMSUPPORTDIR) -stmlib=$(STMLIBDIR)
LLVMCCFLAGS     ?= --gnu-tm -emit-llvm

TESTCC          ?= $(LLVMCC)
TESTCFLAGS      += $(LLVMCCFLAGS) -DTM_DTMC

TESTLD          ?= $(TMLINK)
TESTLDFLAGS     := $(TMLINKFLAGS) $(TESTLDFLAGS) 
# TESTLDFLAGS     += -static 

clean: 	intset-clean
	rm -f *.bc *.o libtanger-stm.bc libtanger-stm.a libitm.a libtanger-stm.s

