#!/bin/bash

trap 'exit 0' INT
valgrind --tool=callgrind ./bench
