P_FLAGS = -O3 -march=native -Wall
D_FLAGS = -g3 -Og -march=native -Wall

COMP = $(CC) hshg.c

.PHONY: build
build: hshg.c hshg.h
	$(COMP) -shared -o libhshg.so $(P_FLAGS)

.PHONY: prod_build
prod_build: hshg.c hshg.h
	$(COMP) -shared -o libhshg.so $(P_FLAGS) -DHSHG_NDEBUG

.PHONY: test
test: hshg.c hshg.h bench.c
	$(CC) test.c -o test $(D_FLAGS) && ./test && \
	$(CC) test.c -o test $(P_FLAGS) && ./test

.PHONY: bench
bench: hshg.c hshg.h bench.c
	$(CC) bench.c -o bench $(P_FLAGS) -lm && ./bench

.PHONY: callgrind
callgrind: hshg.c hshg.h bench.c
	$(CC) bench.c -o bench $(D_FLAGS) -DBENCH_LITE -lm && \
	set -m; \
	./run.sh
	kcachegrind

.PHONY: install
install: libhshg.so
	install libhshg.so /usr/local/lib/ -m 775
	install hshg.h /usr/local/include/ -m 644

.PHONY: uninstall
uninstall:
	$(RM) /usr/local/lib/libhshg.so /usr/local/include/hshg.h libhshg.so
