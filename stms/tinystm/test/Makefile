.PHONY:	all

TESTS = bank intset regression

.PHONY:	all $(TESTS)

all:	$(TESTS)

check: 	all
	@echo Testing load/store \(regression/types\) 
	@./regression/types 1>/dev/null 2>&1
	@echo Testing irrevocability \(regression/irrevocability\)
	@./regression/irrevocability 1>/dev/null 2>&1
	@echo Testing Linked List \(intset/intset-ll\)
	@./intset/intset-ll -d 2000 1>/dev/null 2>&1
	@echo Testing Linked List with concurrency \(intset/intset-ll -n 4\)
	@./intset/intset-ll -d 2000 -n 4 1>/dev/null 2>&1
	@echo Testing Red Black Tree \(intset/intset-rb\)
	@./intset/intset-rb -d 2000 1>/dev/null 2>&1
	@echo Testing Red Black Tree with concurrency \(intset/intset-rb -n 4\)
	@./intset/intset-rb -d 2000 -n 4 1>/dev/null 2>&1
	@echo Testing Skip List \(intset/intset-sl\)
	@./intset/intset-sl -d 2000 1>/dev/null 2>&1
	@echo Testing Skip List with concurrency \(intset/intset-sl -n 4\)
	@./intset/intset-sl -d 2000 -n 4 1>/dev/null 2>&1
	@echo Testing Hash Set \(intset/intset-hs\)
	@./intset/intset-hs -d 2000 1>/dev/null 2>&1
	@echo Testing Hash Set with concurrency \(intset/intset-hs -n 4\)
	@./intset/intset-hs -d 2000 -n 4 1>/dev/null 2>&1
	@echo All tests passed

$(TESTS):
	$(MAKE) -C $@ $(TARGET)
