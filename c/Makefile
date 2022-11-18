.PHONY: build
build: hshg.c hshg.h
	$(CC) hshg.c -shared -o libhshg.so -O3 -march=native

.PHONY: install
install: libhshg.so
	install libhshg.so /usr/local/lib/ -m 775
	install hshg.h /usr/local/include/ -m 644

.PHONY: uninstall
uninstall:
	$(RM) /usr/local/lib/libhshg.so /usr/local/include/hshg.h libhshg.so
