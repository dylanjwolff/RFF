#!/bin/bash
set -ex

export TARGET_KEY=SafeStack
export BENCH=period
export SCHED_MAX=10000

export POS_VOL_DIR="e2-pos-$BENCH-$TARGET_KEY"
export RFF_VOL_DIR="e2-rff-$BENCH-$TARGET_KEY"
mkdir -p $POS_VOL_DIR
mkdir -p $RFF_VOL_DIR

docker run -it -v $(pwd)/$POS_VOL_DIR:/opt/out \
      -e FUZZERS=pos-only-schedfuzz \
      -e TARGET_KEY=$TARGET_KEY \
      -e AFL_TIMEOUT=default \
      -e TIME_BUDGET=24h \
      -e NUM_TRIALS=1 \
      -e ALL_PAIRS=1  \
      -e GLOBAL_SCHED_MAX=$SCHED_MAX \
      -e RECORD_EXACT_RFS=1 \
      -e SCHED_COUNTING=1 \
      -e OMIT_Q_SCHEDS=1 \
      fuzz-$BENCH

docker run -it -v $(pwd)/$RFF_VOL_DIR:/opt/out \
      -e FUZZERS=power-coe-always-rand-schedfuzz \
      -e TARGET_KEY=$TARGET_KEY \
      -e AFL_TIMEOUT=default \
      -e TIME_BUDGET=24h \
      -e NUM_TRIALS=1 \
      -e ALL_PAIRS=1  \
      -e GLOBAL_SCHED_MAX=$SCHED_MAX \
      -e RECORD_EXACT_RFS=1 \
      -e SCHED_COUNTING=1 \
      -e OMIT_Q_SCHEDS=1 \
      fuzz-$BENCH


sudo chown -R $USER .
cp $POS_VOL_DIR/exact_rfs.csv scripts/data-analysis/freq-data/pos-exact-rfs.csv
cp $POS_VOL_DIR/paths.csv scripts/data-analysis/freq-data/pos-paths.csv

cp $RFF_VOL_DIR/exact_rfs.csv scripts/data-analysis/freq-data/power-schedfuzz-exact-rfs.csv
cp $RFF_VOL_DIR/paths.csv scripts/data-analysis/freq-data/power-schedfuzz-paths.csv

python3 scripts/data-analysis/bar-freq.py
