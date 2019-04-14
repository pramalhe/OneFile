# Path to TinySTM
ROOT = ../..

.PHONY:	gcc all clean test check

all:	gcc

# ROOT must be defined to include Makefile.common
include $(ROOT)/abi/Makefile.common

##############################################################################
## GCC
##############################################################################
# TODO Currently it is a copy of GCC-TM Makefile but I didn't find a best way
# to do that until now
CPPFLAGS += -DTM_GCC -I../gcc

# NOTES
#   lib.map enables to export only some functions
gcc: 	libitm.so libitm.a 

arch.o: ../gcc/arch.S
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEF_ABI) -c -o $@ $<

%.do: 	../%.c 
	$(CC) -fPIC $(CPPFLAGS) $(CFLAGS) $(DEF_ABI) -c -o $@ $<

%.o: 	../%.c 
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEF_ABI) -c -o $@ $<

libitm.a: abi.o arch.o 
	$(AR) cru $@ $^

libitm.so: 	abi.do arch.o 
	$(CC) -fPIC $(CPPFLAGS) $(CFLAGS) -shared -Wl,--version-script,../lib.map -o $@ $^
# TODO Check if strip is really needed
#	strip $@
#	cp libitm.so libitm.so.1
#	ln -s libitm.so libitm.so.1
#TODO for FAT filesystem, ln doesn't work
##############################################################################

TESTCC       ?= gcc
TESTCFLAGS   += -Wall -O2 -march=native -DTM_ABI -I../gcc/ -DTLS 
TESTLD       ?= gcc
TESTLDFLAGS  += -Wl,-rpath=$(shell pwd)
TESTLDFLAGS  += -L$(shell pwd) 
# TESTLDFLAGS  += libitm.a
# FIXME: see why big perf degradation with shared library
TESTLDFLAGS  += -litm

clean: 	intset-clean
	rm -f *.o *.do libitm.a libitm.so

