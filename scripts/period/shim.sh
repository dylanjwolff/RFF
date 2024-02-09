#!/bin/bash
set -e

export TARGET_KEY=$1
echo "target key was:"
echo $TARGET_KEY

export BENCH=period
export FUZZERS=power-coe-always-rand-schedfuzz,pos-only-schedfuzz
export TIME_BUDGET=5m
export AFL_TIMEOUT=30000+
export NUM_TRIALS=10

VOL_DIR=$(echo $TARGET_KEY | sed 's/\//-/g')
mkdir -p $BENCH-$VOL_DIR

docker run -v $(pwd)/$BENCH-$VOL_DIR:/opt/out -e NUM_TRIALS=$NUM_TRIALS -e FUZZERS=$FUZZERS -e TARGET_KEY=$TARGET_KEY -e TIME_BUDGET=$TIME_BUDGET -e AFL_TIMEOUT=$AFL_TIMEOUT fuzz-$BENCH
