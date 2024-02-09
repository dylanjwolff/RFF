import os
import subprocess as sp
import json

timeout = 30

execs = os.listdir(".")
execs = [e for e in execs if ".py" not in e and ".json" not in e]
execs = ["sleep 2"]
sp.run("dd if=/dev/random of=input.bin bs=1 count=1000", shell=True)

results = {}
tsan = False
for e in execs:
    results[e] = {}
    try:
        p = sp.run(f"./{e}", capture_output=True, shell=True, timeout=timeout)
        print(p.stdout)
    except sp.TimeoutExpired:
        print("timeout")
        results[e]["status"] = "timeout"
        continue

    if tsan:
        e_lines = p.stderr.decode("utf8").splitlines()
        warns = [l for l in e_lines if "WARN" in l]
        non_leak = [l for l in warns if "leak" not in l]
        if len(non_leak) > 0:
            results[e]["status"] = "tsan"
    else:
        rc = p.returncode
        if rc != 0:
            results[e]["status"] = "nonzero"
        else:
            results[e]["status"] = "zero"

with open("status-results.json", "w") as f:
    f.write(json.dumps(results))
os.remove("input.bin")
