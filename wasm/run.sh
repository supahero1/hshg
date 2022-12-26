#!/bin/bash

clang --target=wasm32 -nostdlib -O3 -Wl,--no-entry -Wl,--lto-O3 \
    -Wl,-z,stack-size=64496 -Wl,--import-memory \
    -Wl,-allow-undefined-file=wasm.syms -Wl,--export-dynamic \
    -I../c -o hshg.wasm wasm.c

node test.js
