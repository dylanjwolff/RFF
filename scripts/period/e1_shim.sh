#!/bin/bash
set -e

export TARGET_KEY=$1
VOL_DIR=$(echo $TARGET_KEY | sed 's/\//-/g')-$BENCH
mkdir -p $BENCH-$VOL_DIR

echo $VOL_DIR
docker run -v $(pwd)/$BENCH-$VOL_DIR:/opt/out -e NUM_TRIALS=$NUM_TRIALS -e FUZZERS=$FUZZERS -e TARGET_KEY=$TARGET_KEY -e TIME_BUDGET=$TIME_BUDGET -e AFL_TIMEOUT=$AFL_TIMEOUT fuzz-$BENCH
