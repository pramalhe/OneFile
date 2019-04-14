# Path to TinySTM
ROOT = ../..

.PHONY:	gcc all clean check test

all:	gcc

# ROOT must be defined to include Makefile.common
include $(ROOT)/abi/Makefile.common


##############################################################################
## GCC
##############################################################################
CPPFLAGS += -DTM_GCC -I.
INCLUDES += $(wildcard ../../src/*.h)

# NOTES
#   lib.map enables to export only some functions
gcc: 	libitm.so libitm.a 

arch.o: arch.S
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEF_ABI) -c -o $@ $<

%.do: 	../%.c $(INCLUDES)
	$(CC) -fPIC $(CPPFLAGS) $(CFLAGS) $(DEF_ABI) -c -o $@ $<

%.o: 	../%.c $(INCLUDES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEF_ABI) -c -o $@ $<

%.s: 	../%.c 
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEF_ABI) -fverbose-asm -S -o $@ $<

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

TESTCC       ?= $(CC)
TESTCFLAGS   += $(CPPFLAGS) $(CFLAGS) -DTM_GCC -fgnu-tm
TESTLD       ?= $(LD)
TESTLDFLAGS  += -Wl,-rpath=$(shell pwd)
TESTLDFLAGS  += -L$(ROOT)/abi/gcc 
TESTLDFLAGS  += -static -litm

clean: 	intset-clean
	rm -f *.o *.do libitm.a libitm.so

