# Path to tinySTM
ROOT = ../..

.PHONY:	intel all clean check test

all:	intel 

# ROOT must be defined to include Makefile.common
include $(ROOT)/abi/Makefile.common


##############################################################################
## INTEL
##############################################################################
CPPFLAGS += -DTM_INTEL -I.

# NOTES
#   Intel STM Compiler uses explicit transaction descriptor
#   Two ways to use:
#   - Compile STM application using libitm.a
#   - Compile STM application using Intel STM and change LD_LIBRARY_PATH to use tiny's libitmdyn.so
# FIXME 
#   .so version need minor modification to stm_tx_t structure (add vtable at 0x10)
# intel/libitmdyn.so
intel: 	libitm.a

%.do: 	../%.c 
	$(CC) -fPIC $(CPPFLAGS) $(CFLAGS) $(DEF_ABI) -c -o $@ $<

%.o: 	../%.c 
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEF_ABI) -c -o $@ $<

# TODO the dependency of arch_x86.S is not set
arch.o: 	arch.S
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

libitm.a: 	abi.o arch.o alloc.o 
	$(AR) cru $@ $^

# TODO check the dynamic version (not really needed)
arch.do: 	arch.S
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

abi_dyn.o:	abi.c
	$(CC) -fPIC $(CPPFLAGS) $(CFLAGS) $(DEF_ABI) -c -o $@ $<

libitmdyn.so: 	abi_dyn.o arch.do
	$(CC) -fPIC $(CPPFLAGS) $(CFLAGS) -shared -Wl,--version-script,../lib.map -o $@ $^ 
##############################################################################

TESTCC          ?= icc
TESTCFLAGS      += -Qtm_enabled -DTM_INTEL

TESTLD          ?= icc
TESTLDFLAGS     += -Qtm_enabled -litm -L. 

clean: 	intset-clean
	rm -f *.o *.do libitm.a libitmdyn.so

