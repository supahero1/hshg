#!/bin/bash

rm -f hshg_bench-hshg.gcda hshg_bench-hshg_bench.gcda
printf "Running an unoptimized version:\nPS: Don't worry if nothing is displayed.\n\n"
sh ./arc_run.sh &  PIDHSHG=$!
sleep 10
kill -s SIGINT $(pidof hshg_bench)
wait $PIDHSHG
printf "\n\nRunning an optimized version:\n\n"
sh ./pro_run.sh &  PIGHSHGOPT=$!
wait $PIGHSHGOPT
