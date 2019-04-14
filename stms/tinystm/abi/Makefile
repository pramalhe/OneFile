
.PHONY:	all gcc dtmc intel clean test check

all:	gcc dtmc intel 

gcc:
	$(MAKE) -C gcc

gcc%:
	$(MAKE) -C gcc $(subst -, , $(subst gcc,,$@))

dtmc:
	$(MAKE) -C dtmc

dtmc%:
	$(MAKE) -C dtmc $(subst -, , $(subst dtmc,,$@))

intel:
	$(MAKE) -C intel

intel%:
	$(MAKE) -C intel $(subst -, , $(subst intel,,$@))

clean:
	$(MAKE) -C test clean
	$(MAKE) -C gcc clean
	$(MAKE) -C dtmc clean
	$(MAKE) -C intel clean

# simple test and check that use explicit calls to gcc libitm library
test:
	$(MAKE) -C test test

check:
	$(MAKE) -C test check

# test-all and check-all need TM compilers (INTEL/DTMC/GCC) in the path 
test-all:
	$(MAKE) -C gcc test
	$(MAKE) -C dtmc test
	$(MAKE) -C intel test

check-all:
	$(MAKE) -C gcc check
	$(MAKE) -C dtmc check
	$(MAKE) -C intel check

gcc/libitm.h: libitm.h.tpl.header libitm.h.tpl.unifdef libitm.h.tpl.cpp libitm.h.tpl.footer
	cat libitm.h.tpl.header > $@
	unifdef -DTM_GCC -UTM_INTEL -UTM_DTMC libitm.h.tpl.unifdef >> $@ || true
	cat libitm.h.tpl.cpp >> $@
	cat libitm.h.tpl.footer >> $@
	sed -i 's/TX_ARGS //g' $@
	sed -i 's/TX_ARG//g' $@

dtmc/libitm.h: libitm.h.tpl.header libitm.h.tpl.unifdef libitm.h.tpl.cpp libitm.h.tpl.footer
	cat libitm.h.tpl.header > $@
	unifdef -UTM_GCC -UTM_INTEL -DTM_DTMC libitm.h.tpl.unifdef >> $@ || true
	cat libitm.h.tpl.cpp >> $@
	cat libitm.h.tpl.footer >> $@
	sed -i 's/TX_ARGS//g' $@
	sed -i 's/TX_ARG//g' $@

intel/libitm.h: libitm.h.tpl.header libitm.h.tpl.unifdef libitm.h.tpl.cpp libitm.h.tpl.footer
	cat libitm.h.tpl.header > $@
	unifdef -UTM_GCC -DTM_INTEL -UTM_DTMC libitm.h.tpl.unifdef >> $@ || true
	cat libitm.h.tpl.cpp >> $@
	cat libitm.h.tpl.footer >> $@
	sed -i 's/TX_ARGS/_ITM_transaction *,/g' $@
	sed -i 's/TX_ARG/_ITM_transaction */g' $@

