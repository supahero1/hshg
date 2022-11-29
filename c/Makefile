.PHONY: build
build: hshg.c hshg.h
	$(CC) hshg.c -shared -o libhshg.so -O3 -march=native -Wall

.PHONY: prod_build
prod_build: hshg.c hshg.h
	$(CC) hshg.c -shared -o libhshg.so -O3 -march=native -Wall -DHSHG_NDEBUG

.PHONY: bench
bench: hshg.c hshg.h hshg_bench.c
	$(CC) hshg.c hshg_bench.c -o hshg_bench -O3 -march=native -Wall -flto -lm && ./hshg_bench

.PHONY: callgrind
callgrind: hshg.c hshg.h hshg_bench.c
	$(CC) hshg.c hshg_bench.c -o hshg_bench -g3 -Og -march=native -Wall -DBENCH_LITE -lm && \
	set -m; ./run.sh; \
	kcachegrind

.PHONY: install
install: libhshg.so
	install libhshg.so /usr/local/lib/ -m 775
	install hshg.h /usr/local/include/ -m 644

.PHONY: uninstall
uninstall:
	$(RM) /usr/local/lib/libhshg.so /usr/local/include/hshg.h libhshg.so
