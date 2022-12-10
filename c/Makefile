P_FLAGS = -O3 -march=native -Wall
U_FLAGS = -g3 -Og -Wall
D_FLAGS = $(U_FLAGS) -fsanitize=address,undefined

DIMENSIONS ?= 2

COMP = $(CC) hshg.c

.PHONY: build
build: hshg.c hshg.h
	$(COMP) -shared -o libhshg.so $(P_FLAGS)

.PHONY: prod_build
prod_build: hshg.c hshg.h
	$(COMP) -shared -o libhshg.so $(P_FLAGS) -DHSHG_NDEBUG

.PHONY: test
test: hshg.c hshg.h test_1d.c test_2d.c
	@echo "1d Og"
	$(CC) test_1d.c -o test_1d $(D_FLAGS)
	./test_1d
	@echo "1d O3"
	$(CC) test_1d.c -o test_1d $(P_FLAGS)
	./test_1d
	@echo "2d Og"
	$(CC) test_2d.c -o test_2d $(D_FLAGS)
	./test_2d
	@echo "2d O3"
	$(CC) test_2d.c -o test_2d $(P_FLAGS)
	./test_2d
	@echo "3d Og"
	$(CC) test_3d.c -o test_3d $(D_FLAGS)
	./test_3d
	@echo "3d O3"
	$(CC) test_3d.c -o test_3d $(P_FLAGS)
	./test_3d

.PHONY: bench
bench: hshg.c hshg.h bench.c
	$(CC) bench.c -o bench $(P_FLAGS) -DHSHG_D=$(DIMENSIONS) -lm
	./bench

.PHONY: callgrind
callgrind: hshg.c hshg.h bench.c
	$(CC) bench.c -o bench $(U_FLAGS) -DHSHG_D=$(DIMENSIONS) -DBENCH_LITE -lm
	set -m
	./run.sh
	kcachegrind

.PHONY: install
install: libhshg.so
	install libhshg.so /usr/local/lib/ -m 775
	install hshg.h /usr/local/include/ -m 644

.PHONY: uninstall
uninstall:
	$(RM) /usr/local/lib/libhshg.so /usr/local/include/hshg.h libhshg.so
