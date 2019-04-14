ROOT = ../..

include $(ROOT)/Makefile.common

BINS = types irrevocability

.PHONY:	all clean

all:	$(BINS)

%.o:	%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFINES) -c -o $@ $<

$(BINS):	%:	%.o $(TMLIB)
	$(CC) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
