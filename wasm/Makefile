.PHONY: test
test:
	node test_2d.js

.PHONY: build
build:
	for i in 1 2 3; \
	do \
		name="hshg_$${i}d.wasm"; \
		 \
		clang --target=wasm32 -nostdlib -O3 -DHSHG_D=$$i -Wl,--no-entry \
		-Wl,--lto-O3 -Wl,-z,stack-size=64496 -Wl,--import-memory \
		-Wl,-allow-undefined-file=wasm.syms -Wl,--export-dynamic \
		-I../c -o $$name wasm.c; \
	done
