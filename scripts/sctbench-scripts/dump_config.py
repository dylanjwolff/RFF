import subprocess as sp
import glob
import os
import json

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

special_case = {}
special_case["aget"] = {"args": " -l dl.txt http://www.gnu.org/licenses/gpl.txt"}
special_case["pbzip2"] = {"args": " -k -f -p4 -1 -b1 @@", "inputs": "/opt/sched-fuzz/sctbench/benchmarks/conc-bugs/pbzip2-0.9.4/test.tar"}
special_case["bzip"] = {"args": " @@", "inputs": "/opt/sched-fuzz/sctbench/benchmarks/inspect_benchmarks/obj/bzip2smp.comb/run/bzip_input"}
special_case["ferret"] = {"args": " corel lsh queries 1 1 4 output.txt"}
special_case["streamcluster"] = {"args": " 2 5 1 10 10 5 none output.txt 4"}
special_case["qsort_mt"] = {"args": " -n 32 -f 4 -h 4 -v" }
special_case["lu"] = {"args": " -p2 -b2 -n1" }

bin_to_period_name = {"aget": "aget-bug",
                      "interlockedworkstealqueue": "iwsq",
                      "interlockedworkstealqueuewithstate": "iwsqws",
                      "stateworkstealqueue": "swsq",
                      "workstealqueue": "wsq",
        }

config = {}
for e in execs:
    stem = e.split("/")[-1]
    if "stringbuffer" in e:
        name = "stringbuffer"
    elif "safestack" in e:
        name = "safestack"
    elif "streamcluster2" in e:
        name = "streamcluster2"
    elif "streamcluster3" in e:
        name = "streamcluster3"
    else:
        name = e.split("/")[-1]
        name = name.split('.')[0].lower()
        name = bin_to_period_name[name] if name in bin_to_period_name.keys() else name

    if stem in special_case.keys() and "args" in special_case[stem]:
         args = special_case[stem]["args"]
    else:
         args = ""

    if stem in special_case.keys() and "inputs" in special_case[stem]:
         inputs = glob.glob(special_case[stem]["inputs"])
    else:
         inputs = []
 
    config[name] = { "path" : e,
                     "stem" : stem,
                     "inputs" : inputs,
                     "args" : args }

with open('sctbench_subject_config.json', 'w') as fp:
    json.dump(config, fp)
