#!/bin/bash
set -e

export TARGET_NAME=$1
echo "target name was:"
echo $TARGET_NAME

export BENCH=sctbench
export FUZZERS=schedfuzz
export TIME_BUDGET=5m
export AFL_TIMEOUT=30000+

mkdir -p $BENCH-$TARGET_NAME

docker run -v $(pwd)/$BENCH-$TARGET_NAME:/opt/out -e FUZZERS=$FUZZERS -e TARGET_NAME=$TARGET_NAME -e TIME_BUDGET=$TIME_BUDGET -e AFL_TIMEOUT=$AFL_TIMEOUT fuzz-$BENCH
