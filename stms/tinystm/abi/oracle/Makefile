# Path to TinySTM
ROOT = ../..

.PHONY:	oracletm all clean check test

all:	oracletm

# ROOT must be defined to include Makefile.common
include $(ROOT)/abi/Makefile.common


##############################################################################
## Oracle TM 
##############################################################################
CPPFLAGS += -DTM_ORACLE -I.

oracletm: 	libitm.a 

arch.o: arch.S
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEF_ABI) -c -o $@ $<

%.o: 	%.c 
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEF_ABI) -c -o $@ $<

%.s: 	%.c 
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEF_ABI) -fverbose-asm -S -o $@ $<

libitm.a: otm.o arch.o 
	$(AR) cru $@ $^

clean:
	rm -f *.o *.do libitm.a

