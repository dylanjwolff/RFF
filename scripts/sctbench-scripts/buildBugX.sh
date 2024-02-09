#!/bin/sh

cd /opt/sched-fuzz/sctbench/benchmarks/radbench/Benchmarks/bug2
make
ln -s /opt/sched-fuzz/sctbench/benchmarks/radbench/Benchmarks/bug2/src/js/src/Linux_All_DBG.OBJ/libjs.so /usr/lib/libjs.so

cd /opt/sched-fuzz/sctbench/benchmarks/radbench/Benchmarks/bug3
sed -i '24 d' /opt/sched-fuzz/sctbench/benchmarks/radbench/Benchmarks/bug3/scripts/build-testharness.sh
sed -i '24 i gcc -g -DXP_UNIX -DJS_THREADSAFE -I $JS_SRC_SRC -I $JS_SRC_SRC/*.OBJ/ $SRC_DIR/test.c -L $JS_SRC_SRC/*.OBJ/ -ljs -o $BIN_DIR/test-js -lpthread -lm' /opt/sched-fuzz/sctbench/benchmarks/radbench/Benchmarks/bug3/scripts/build-testharness.sh
make

cd /opt/sched-fuzz/sctbench/benchmarks/radbench/Benchmarks/bug4
make

cd /opt/sched-fuzz/sctbench/benchmarks/radbench/Benchmarks/bug5
make

cd /opt/sched-fuzz/sctbench/benchmarks/radbench/Benchmarks/bug6
make
