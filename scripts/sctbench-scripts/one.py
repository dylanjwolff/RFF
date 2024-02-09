import subprocess as sp
import glob
import os
import json
import time

sp.run(f"mkdir -p /opt/out", shell=True)

name = os.getenv("TARGET_NAME") 
timeout = os.getenv("AFL_TIMEOUT") or "5000+"
time_budget = os.getenv("TIME_BUDGET") or "30s" 
fuzzers = os.getenv("FUZZERS").split(',') 

benchstart = time.time()
san = "none"
prefix = ""

with open("sctbench_subject_config.json") as f:
    subject_config = json.loads(f.read())

stem = subject_config[name]["stem"]
args = subject_config[name]["args"]
inputs = subject_config[name]["inputs"]
e = subject_config[name]["path"]

results={}
results[san] = {}
for fuzzer in fuzzers:
     results[san][fuzzer] = {}

     outdir = f"/opt/out/{stem}-{fuzzer}-out"
     sp.run(f"rm -r {outdir}", shell=True)
     sp.run(f"rm {stem}.afl", shell=True)

     sp.run(f"rm -r afl-in", shell=True)
     sp.run(f"mkdir afl-in", shell=True)
     if len(inputs) > 0:
         for i in inputs:
             sp.run(f"cp {i} afl-in", shell=True)
     else:
         sp.run(f"echo 'hi' > afl-in/hi.txt", shell=True)

     if fuzzer == "schedfuzz":
         try:
              sp.run(f"./instrument.sh {e}", shell=True)
         except:
              print(f"Error schedfuzz instrumenting {stem}!")
              continue

         # static period preprocessing
         if True:
            logs_pp = []
            src_root = os.path.dirname(e)
            p1 = sp.run(f"python3 get-offsets-from-mempairs.py {src_root}/mempair.{stem} {e} {src_root}", shell=True, capture_output=True)
            logs_pp = logs_pp + [p1.stdout]
            sp.run(f"rm -f ./{stem}.afl", shell=True)
            p1 = sp.run(f"./selective-instrument.sh {e} out.csv", shell=True, capture_output=True)
            logs_pp = logs_pp + [p1.stdout]

            with open('/opt/out/pp-logs.txt', 'w') as fp:
                fp.write("\n".join([l.decode("utf-8") for l in logs_pp]))             

            print("done with preprocessing");

         # dynamic preprocessing 
         if False:
             logs_pp = []
             p1 = sp.run(f"TO_LOG=true LOG_DIR=events.log LOG_DIR_MAP=mapping.log LD_PRELOAD=$(pwd)/libsched.so ./{stem}.afl {args}", shell=True, capture_output=True)
             logs_pp = logs_pp + [p1.stdout]
             p1 = sp.run(f"python3 ../race_predictor/fuzz_preprocess.py events.log mapping.log comb.log", shell=True, capture_output=True)
             logs_pp = logs_pp + [p1.stdout]
             p1 = sp.run("java -cp ../race_predictor/rapid.jar:../race_predictor/lib/* Main -t comb.log -s event_pairs.json -w true", shell=True, capture_output=True)
             logs_pp = logs_pp + [p1.stdout]
             p1 = sp.run("python3 all-rel-events.py event_pairs.json out.csv", shell=True, capture_output=True)
             logs_pp = logs_pp + [p1.stdout]

             sp.run(f"rm -f ./{stem}.afl", shell=True)

             p2 = sp.run(f"./selective-instrument.sh {e} out.csv", shell=True, capture_output=True)
             sp.run(f"rm -rf {outdir}", shell=True)

             with open('/opt/out/ppp-logs.txt', 'w') as fp:
                 fp.write(p2.stdout.decode("utf-8"))
             with open('/opt/out/pp-logs.txt', 'w') as fp:
                 fp.write("\n".join([l.decode("utf-8") for l in logs_pp]))             

             print("done with preprocessing");

         start = time.time()
         p = sp.run(f"{prefix} timeout {time_budget} ./fuzz.sh -i afl-in -o {outdir} -d -t {timeout} -- ./{stem}.afl {args}", shell=True, capture_output=True)
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

     with open('/opt/out/afl-logs.txt', 'w') as fp:
         fp.write(p.stdout.decode("utf-8"))

     if "All test cases time out" in p.stdout.decode("utf-8"):
         results[san][fuzzer][name] = "TIMEOUT"
     elif "not found or not executable" in p.stdout.decode("utf-8"):
         results[san][fuzzer][name] = "ERROR-NE"
     elif "Unable to execute target application" in p.stdout.decode("utf-8"):
         with open('/opt/out/pp-logs.txt', 'w') as fp:
             fp.write(p.stdout.decode("utf-8"))
         results[san][fuzzer][name] = "CRASH"
     elif "crashed suddenly, before receiving any input" in p.stdout.decode("utf-8"):
         results[san][fuzzer][name] = "ERROR-C"
     else:
         results[san][fuzzer][name] = max(len(crashes), len(sched_crashes))

print(results)
benchelapsed_secs = time.time() - benchstart
print(f"run took {benchelapsed_secs}")
with open('/opt/out/result.json', 'w') as fp:
    json.dump(results, fp)

