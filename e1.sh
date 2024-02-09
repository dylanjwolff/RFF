#!/bin/bash

export TIME_BUDGET=5m
export AFL_TIMEOUT=30000+
export NUM_TRIALS=20



export BENCH=period
export FUZZERS=power-coe-always-rand-schedfuzz,pos-only-schedfuzz

cat all_targets.txt | parallel -I @@ -u ./scripts/period/e1_shim.sh @@

export BENCH=period-pct
export FUZZERS=pct-no-rff-always-rand-schedfuzz

cat all_targets.txt | parallel -I @@ -u ./scripts/period/e1_shim.sh @@

export BENCH=period-qlearning
export FUZZERS=qlearning-schedfuzz

cat all_targets.txt | parallel -I @@ -u ./scripts/period/e1_shim.sh @@

sudo chown -R $USER .

python scripts/period/parse-agg.py period-*/result.json

cp full-data.csv scripts/data-analysis

python scripts/data-analysis/analyze.py
