import subprocess as sp
import glob
import os
import json
import time

execs={}

execs["none"] = glob.glob("working-execs/*")
execs["tsan"] = glob.glob("working-execs-tsan/*")

timeout = "5m"
os.mkdir("afl-in")
sp.run("echo 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' > afl-in/test.case", shell=True)

results = {}

# for san in ["none", "tsan"]:
for san in ["none"]:
    results[san] = {}
    if san == "tsan":
        prefix = "TSAN_OPTIONS=abort_on_error=1:symbolize=0"
    else:
        prefix = "MASK=0x1"

    # for fuzzer in ["schedfuzz"]:
    for fuzzer in ["schedfuzz", "afl"]:
        results[san][fuzzer] = {}

        for e in execs[san]:
            stem = e.split("/")[-1]

            outdir = f"/opt/out/{stem}-{fuzzer}-{san}-out"
            sp.run(f"rm -r {outdir}", shell=True)
            sp.run(f"rm {stem}.afl", shell=True)

            if fuzzer == "schedfuzz":
                sp.run(f"./instrument.sh {e}", shell=True)
                start = time.time()
                sp.run(f"{prefix} timeout {timeout} ./fuzz.sh -i afl-in -o {outdir} -d -f input.bin -- ./{stem}.afl", shell=True)
                elapsed_secs = time.time() - start
            elif fuzzer == "afl":
                sp.run(f"e9afl {e}", shell=True)
                start = time.time()
                sp.run(f"{prefix} timeout {timeout} ./AFL/afl-fuzz -i afl-in -o {outdir} -d -f input.bin -m none -- ./{stem}.afl", shell=True)
                elapsed_secs = time.time() - start

            crashes = glob.glob(f"{outdir}/crashes/*")
            crashes = [c for c in crashes if "README" not in c]
            sched_crashes = glob.glob(f"{outdir}/scheds/**/crash*")
            if elapsed_secs > 5:
                results[san][fuzzer][stem] = max(len(crashes), len(sched_crashes))
            else:
                results[san][fuzzer][stem] = -1

print(results)
with open('/opt/out/result.json', 'w') as fp:
    json.dump(results, fp)

