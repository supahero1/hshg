#!/bin/bash

cc hshg.c hshg_bench.c -o hshg_bench -O3 -march=native -lm && ./hshg_bench
