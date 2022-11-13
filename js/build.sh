#!/bin/bash

# If you changed AGENTS_NUM in hshg_wasm.c and the simulation
# flatlines due to no stack space, increase INITIAL_MEMORY below.
emcc ../c/hshg.c hshg_wasm.c -o hshg.js -O3 -I../c -lm -s INITIAL_MEMORY=65536
