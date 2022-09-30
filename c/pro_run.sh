#!/bin/bash

cc hshg.c hshg_bench.c -o hshg_bench -O3 -march=native -fprofile-use -lm && ./hshg_bench
