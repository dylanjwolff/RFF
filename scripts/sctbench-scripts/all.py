import subprocess as sp
import glob
import os
import json
import time

results = {}
execs = glob.glob("/opt/sched-fuzz/sctbench/benchmarks/concurrent-software-benchmarks/obj/**/**/*")
execs = glob.glob("/opt/sched-fuzz/sctbench/benchmarks/chess/obj/**/**/*") + execs
execs = glob.glob("/opt/sched-fuzz/sctbench/benchmarks/conc-bugs/**/run/*") + execs
execs = glob.glob("/opt/sched-fuzz/sctbench/benchmarks/inspect_benchmarks/obj/**/run/*") + execs
execs = glob.glob("/opt/sched-fuzz/sctbench/benchmarks/inspect_examples/obj/**/run/*") + execs
execs = glob.glob("/opt/sched-fuzz/sctbench/benchmarks/safestack/run/*") + execs
execs = glob.glob("/opt/sched-fuzz/sctbench/benchmarks/splash2/kernels/lu/non_contiguous_blocks/run/*") + execs
execs = glob.glob("/opt/sched-fuzz/sctbench/benchmarks/splash2/kernels/fft/run/*") + execs
execs = glob.glob("/opt/sched-fuzz/sctbench/benchmarks/splash2/apps/barnes/run/*") + execs
execs = glob.glob("/opt/sched-fuzz/sctbench/benchmarks/parsec-2.0/pkgs/apps/ferret/inst/amd64-linux.gcc/bin/ferret") + execs
execs = glob.glob("/opt/sched-fuzz/sctbench/benchmarks/parsec-2.0/pkgs/kernels/streamcluster/inst/amd64-linux.gcc/bin/streamcluster") + execs
execs = glob.glob("/opt/sched-fuzz/sctbench/benchmarks/parsec-2.0/pkgs/kernels/streamcluster2/inst/amd64-linux.gcc/bin/streamcluster") + execs
execs = glob.glob("/opt/sched-fuzz/sctbench/benchmarks/parsec-2.0/pkgs/kernels/streamcluster3/inst/amd64-linux.gcc/bin/streamcluster") + execs

# only instrument executables
execs = [e for e in execs if os.access(e, os.X_OK)]

names = {}
for e in execs:
    if "stringbuffer" in e:
        names[e] = "stringbuffer"
    if "safestack" in e:
        names[e] = "safestack"

sp.run(f"mkdir -p /opt/out", shell=True)

benchstart = time.time()
time_budget = "5m"
timeout = "5000+"
prefix = "MASK=0x1"
san = "none"


special_case = {}
special_case["aget"] = {"args": " -l dl.txt http://www.gnu.org/licenses/gpl.txt"}
special_case["pbzip2"] = {"args": " -k -f -p4 -1 -b1 @@", "inputs": "/opt/sched-fuzz/sctbench/benchmarks/conc-bugs/pbzip2-0.9.4/test.tar"}
special_case["bzip"] = {"args": " @@", "inputs": "/opt/sched-fuzz/sctbench/benchmarks/inspect_benchmarks/obj/bzip2smp.comb/run/bzip_input"}
special_case["ferret"] = {"args": " corel lsh queries 1 1 4 output.txt"}
special_case["streamcluster"] = {"args": " 2 5 1 10 10 5 none output.txt 4"}

results[san] = {}
for fuzzer in ["schedfuzz", "afl"]:
        results[san][fuzzer] = {}
        for e in execs:
            stem = e.split("/")[-1]

            outdir = f"/opt/out/{stem}-{fuzzer}-out"
            sp.run(f"rm -r {outdir}", shell=True)
            sp.run(f"rm {stem}.afl", shell=True)

            if stem in special_case.keys() and "args" in special_case[stem]:
                args = special_case[stem]["args"]
            else:
                args = ""

            sp.run(f"rm -r afl-in", shell=True)
            sp.run(f"mkdir afl-in", shell=True)
            if stem in special_case.keys() and "inputs" in special_case[stem]:
                inputs = execs = glob.glob(special_case[stem]["inputs"])
                for i in inputs:
                    sp.run(f"cp {i} afl-in", shell=True)
            else:
                sp.run(f"echo 'hi' > afl-in/hi.txt", shell=True)

            if fuzzer == "schedfuzz":
                try:
                    sp.run(f"./instrument.sh {e}", shell=True)
                except:
                    continue
                start = time.time()
                p = sp.run(f"{prefix} timeout {time_budget} ./fuzz.sh -i afl-in -o {outdir} -d -f input.bin -t {timeout} -- ./{stem}.afl {args}", shell=True, capture_output=True)
                elapsed_secs = time.time() - start
            elif fuzzer == "afl":
                try:
                    sp.run(f"e9afl {e}", shell=True)
                except:
                    continue
                start = time.time()
                p = sp.run(f"{prefix} timeout {time_budget} ./AFL/afl-fuzz -i afl-in -o {outdir} -d -f input.bin -m none -t {timeout} -- ./{stem}.afl {args}", shell=True, capture_output=True)
                elapsed_secs = time.time() - start
            crashes = glob.glob(f"{outdir}/crashes/*")
            crashes = [c for c in crashes if "README" not in c]
            sched_crashes = glob.glob(f"{outdir}/scheds/**/crash*")

            if e in names:
                stem = names[e]
            if elapsed_secs > 5:
                results[san][fuzzer][stem] = max(len(crashes), len(sched_crashes))
            else:
                if "All test cases time out" in p.stdout.decode("utf-8"):
                    results[san][fuzzer][stem] = "TIMEOUT"
                else:
                    results[san][fuzzer][stem] = "CRASH"

print(results)
benchelapsed_secs = time.time() - benchstart
print(f"run took {benchelapsed_secs}")
with open('/opt/out/result.json', 'w') as fp:
    json.dump(results, fp)

