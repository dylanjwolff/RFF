#!/bin/bash
set -e

./instrument.sh /workdir/RaceBenchData/blackscholes.1/install/blackscholes

AFL_NO_AFFINITY=1 AFL_FAST_CAL=1 AFL_NO_ARITH=1 AFL_PRELOAD=./libsched.so SCHEDULE=./SCHEDULE ALWAYS_RAND=1 POWER_COE=1 POS_ONLY=1 AFL-2.57b/afl-fuzz -m none -i /workdir/RaceBenchData/blackscholes.1/input/ -o o -d -- ./blackscholes.afl 3 @@ /dev/null
