ROOT ?= ../..

include $(ROOT)/Makefile.common

BINS = intset-hs intset-ll intset-rb intset-sl

UNAME := $(shell uname)
ifeq ($(UNAME), SunOS)
# Solaris requires rt lib for nanosleep
LDFLAGS += -lrt
endif

.PHONY:	all clean

all:	$(BINS)

intset-hs.o:	intset.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFINES) -DUSE_HASHSET -c -o $@ $<

intset-ll.o:	intset.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFINES) -DUSE_LINKEDLIST -c -o $@ $<

intset-rb.o:	intset.c rbtree.c rbtree.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFINES) -DUSE_RBTREE -c -o $@ $<

intset-sl.o:	intset.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFINES) -DUSE_SKIPLIST -c -o $@ $<

# FIXME in case of ABI $(TMLIB) must be replaced to abi/...
$(BINS):	%:	%.o $(TMLIB)
	$(LD) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
