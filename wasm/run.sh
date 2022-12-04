#!/bin/bash

clang --target=wasm32 -nostdlib -Wl,--no-entry -Wl,--lto-O3 \
    -Wl,-z,stack-size=65536 -Wl,--import-memory \
    -Wl,-allow-undefined-file=wasm.syms -Wl,--export-dynamic \
    -I../c -o hshg.wasm wasm.c

wasm-opt -O4 -uim -o hshg_opt.wasm hshg.wasm

# wasm2wat hshg_opt.wasm > hshg.txt

node test.js
