import subprocess as sp
import glob
import os
import json
import time

class SchedfuzzRunner:
    def __init__(self, abbrev):
        self.abbrev = abbrev

    def pp_and_instrum(self, e, stem, args):
        if "static-pp" in self.abbrev or "pp-static" in self.abbrev:
            sp.run(f"rm -f out.csv", shell=True)
            logs_pp = []
            src_root = os.path.dirname(e)
            p1 = sp.run(f"python3 get-offsets-from-mempairs.py {src_root}/mempair.{stem} {e} {src_root}", shell=True, capture_output=True)
            logs_pp = logs_pp + [p1.stdout]
            sp.run(f"rm -f ./{stem}.afl", shell=True)
            p1 = sp.run(f"./selective-instrument.sh {e} out.csv", shell=True, capture_output=True)
            logs_pp = logs_pp + [p1.stdout]
        elif "dynamic-pp" in self.abbrev or "pp-dynamic" in self.abbrev:
            sp.run(f"rm -f out.csv events.log mapping.log comb.log event_pairs.json", shell=True)
            logs_pp = []
            p1 = sp.run(f"./instrument.sh {e}", shell=True, capture_output=True)
            logs_pp = logs_pp + [p1.stdout]
            p1 = sp.run(f"TO_LOG=true LOG_DIR=events.log LOG_DIR_MAP=mapping.log LD_PRELOAD=$(pwd)/libsched.so ./{stem}.afl {args}", shell=True, capture_output=True)
            logs_pp = logs_pp + [p1.stdout]
            p1 = sp.run(f"python3 ../race_predictor/fuzz_preprocess.py events.log mapping.log comb.log", shell=True, capture_output=True)
            logs_pp = logs_pp + [p1.stdout]
            p1 = sp.run("java -cp ../race_predictor/rapid.jar:../race_predictor/lib/* Main -t comb.log -s event_pairs.json -w true", shell=True, capture_output=True)
            logs_pp = logs_pp + [p1.stdout]
            p1 = sp.run("python3 all-rel-events.py event_pairs.json out.csv", shell=True, capture_output=True)
            logs_pp = logs_pp + [p1.stdout]
            sp.run(f"rm -f ./{stem}.afl", shell=True)

            p1 = sp.run(f"./selective-instrument.sh {e} out.csv", shell=True, capture_output=True)
            logs_pp = logs_pp + [p1.stdout]

            print("done with preprocessing");
        else:
            print("instrum only")
            logs_pp = []
            p1 = sp.run(f"./instrument.sh {e}", shell=True, capture_output=True)
            logs_pp = logs_pp + [p1.stdout]

        with open('/opt/out/pp-logs.txt', 'w') as fp:
            fp.write("\n".join([l.decode("utf-8") for l in logs_pp]))             

        print("done with preprocessing");

    def run(self, trial_num, prefix, time_budget, timeout, outdir, stem, args, libs):
        if "no-rff" in self.abbrev:
            prefix = prefix + " NO_RFF=1"
        if "no-pos" in self.abbrev:
            prefix = prefix + " NO_POS=1"
        if "pos-only" in self.abbrev:
            prefix = prefix + " POS_ONLY=1"
        if "no-afl-cov" in self.abbrev:
            prefix = prefix + " NO_AFL_COV=1"
        if "depth-3" in self.abbrev:
            prefix = prefix + " MAX_DEPTH=3"
        if "delay-1000" in self.abbrev:
            prefix = prefix + " SWITCH_EVERY_2N=1000"
        if "delay-10" in self.abbrev and not "delay-1000" in self.abbrev:
            prefix = prefix + " SWITCH_EVERY_2N=10"
        if "always-rand" in self.abbrev:
            prefix = prefix + " ALWAYS_RAND=1"
        if "l2" in self.abbrev:
            prefix = prefix + " L2=1"
        if "non-weighted" in self.abbrev:
            prefix = prefix + " NON_WEIGHTED_SAMPLE=1"
        if "max-sp" in self.abbrev:
            prefix = prefix + " SCORE_PATTERN=max"
        if "avg-sp" in self.abbrev:
            prefix = prefix + " SCORE_PATTERN=avg"
        if "absmax-dp" in self.abbrev:
            prefix = prefix + " DIFF_PATTERN=absMax"
        if "abssum-dp" in self.abbrev:
            prefix = prefix + " DIFF_PATTERN=absSum"
        if "sum-dp" in self.abbrev and not "abssum-dp" in self.abbrev:
            prefix = prefix + " DIFF_PATTERN=sum"
        if "avg-dp" in self.abbrev:
            prefix = prefix + " DIFF_PATTERN=avg"
        if "no-sched-counting" not in self.abbrev:
            prefix = prefix + " SCHED_COUNTING=1"
        if "thread-affinity-200" in self.abbrev:
            prefix = prefix + " THREAD_AFFINITY=200"
        if "thread-affinity--200" in self.abbrev:
            prefix = prefix + " THREAD_AFFINITY=-200"
        if "thread-affinity-500" in self.abbrev:
            prefix = prefix + " THREAD_AFFINITY=500"
        if "thread-affinity--500" in self.abbrev:
            prefix = prefix + " THREAD_AFFINITY=-500"
        if "thread-affinity-800" in self.abbrev:
            prefix = prefix + " THREAD_AFFINITY=800"
        if "thread-affinity--800" in self.abbrev:
            prefix = prefix + " THREAD_AFFINITY=-800"
        if "max-multi-mutations-2" in self.abbrev:
            prefix = prefix + " MAX_MULTI_MUTATIONS=2"
        if "max-multi-mutations-3" in self.abbrev:
            prefix = prefix + " MAX_MULTI_MUTATIONS=3"
        if "rp-pairs" in self.abbrev:
            prefix = prefix + " ALL_PAIRS=0"
        if "all-rff" in self.abbrev:
            prefix = prefix + " ALL_RFF=1"
        if "power-coe" in self.abbrev:
            prefix = prefix + " POWER_COE=1"
        if "stage-max-128" in self.abbrev:
            prefix = prefix + " SCHED_STAGE_MAX=128"
        if "freq-experiment" in self.abbrev:
            prefix = prefix + " ALL_PAIRS=1 GLOBAL_SCHED_MAX=10000 SCHED_COUNTING=1 RECORD_EXACT_RFS=1"


        if libs:
            ldlib = f"LD_LIBRARY_PATH={libs}"
        else:
            ldlib = ""

        print(f"env prefix was {trial_num} {prefix} {ldlib}")

        self.start = time.time()
        if timeout == "default":
            p = sp.run(f"RANDOM_SEED={trial_num} {prefix} {ldlib} timeout --signal=9 --kill-after={time_budget} {time_budget} ./fuzz.sh -i afl-in -o {outdir} -d -- ./{stem}.afl {args}", shell=True, capture_output=True)
        else:
            p = sp.run(f"RANDOM_SEED={trial_num} {prefix} {ldlib} timeout --signal=9 --kill-after={time_budget} {time_budget} ./fuzz.sh -i afl-in -o {outdir} -d -t {timeout} -- ./{stem}.afl {args}", shell=True, capture_output=True)
        self.elapsed_secs = time.time() - self.start

        self.logs = p.stdout.decode("utf-8")

        with open('/opt/out/afl-logs.txt', 'w') as fp:
            fp.write(self.logs)

        self.outdir = outdir

    def parse_fuzzer_stats(self, fname):        
        stats = {}
        with open(fname) as f:
            lines = f.readlines()
            for line in lines:
                if "bitmap_cvg" in line:
                    stats["cvg_pct"] = float(line.split(':')[-1]
                        .replace("%", "").strip())
                if "paths_found" in line:
                    stats["num_new_cov"] = int(line.split(':')[-1]
                        .strip())
        return stats

 
    def analyze(self, trial_num):
        outdir = self.outdir
        elapsed_secs = self.elapsed_secs
        results = {}

        crashes = glob.glob(f"{outdir}/crashes/*")
        crashes = [c for c in crashes if "README" not in c]
        sched_crashes = glob.glob(f"{outdir}/scheds/**/crash*")
        interesting_scheds = glob.glob(f"{outdir}/scheds/**/*")
        interesting_scheds = [s for s in interesting_scheds if "crash" not in os.path.basename(s)]
        total_scheds = None
        try:
            with open(f"{outdir}/num_scheds.txt") as f:
                 total_scheds = int(f.readline().strip())
        except:
            pass

        if "All test cases time out" in self.logs:
            results = {"error": "TIMEOUT", "trial_num": trial_num}
        elif "not found or not executable" in self.logs:
            results = {"error": "NOTEXEC", "trial_num": trial_num}
        elif "Unable to execute target application" in self.logs:
            with open('/opt/out/pp-logs.txt', 'w') as fp:
                fp.write(self.logs)
            results = {
                    "error" : None,
                    "buggy_scheds": 1,
                    "scheds_to_bug": 1,
                    "total_scheds": 1,
                    "time_to_bug": elapsed_secs, 
                    "trial_num": trial_num
            }
        elif "crashed suddenly, before receiving any input" in self.logs:
            results = {"error": "CRASH",  "trial_num": trial_num }
        else:
            stats = self.parse_fuzzer_stats(f"{outdir}/fuzzer_stats")
            best = None
            best_time = None
            for f in sched_crashes + crashes:
                mtime = os.path.getmtime(f)
                if best is None or mtime < best_time:
                    best = f
                    best_time = mtime
            ttb = best_time - self.start if best is not None else None

            best_num = None
            best_num_val = None
            for f in sched_crashes:
                num = int(os.path.basename(f).split('-')[1].split(':')[0])
                if best_num is None or best_num_val > num:
                    best_num = f
                    best_num_val = num

            results = {
                    "error": None,
                    "buggy_scheds": max(len(crashes), len(sched_crashes)),
                    "scheds_to_bug": best_num_val,
                    "total_scheds": total_scheds,
                    "time_to_bug": ttb,
                    "interesting_scheds": len(interesting_scheds),
                    "trial_num": trial_num,
            }
            for k, v in stats.items():
                results[k] = v
        return results



def main():
    sp.run(f"mkdir -p /opt/out", shell=True)

    key = os.getenv("TARGET_KEY") 
    timeout = os.getenv("AFL_TIMEOUT") or "5000+"
    time_budget = os.getenv("TIME_BUDGET") or "30s" 
    fuzzers = os.getenv("FUZZERS").split(',') 
    trials = os.getenv("NUM_TRIALS") or "2"
    trials = int(trials)

    benchstart = time.time()
    san = "none"
    prefix = "AFL_NO_AFFINITY=1 AFL_FAST_CAL=1 "

    with open("sctbench_subject_config.json") as f:
        subject_config = json.loads(f.read())

    name = subject_config[key]["name"]
    stem = subject_config[key]["stem"]
    args = subject_config[key]["args"]
    inputs = subject_config[key]["inputs"]
    libs = subject_config[key]["libs"]
    e = subject_config[key]["path"]

    results={}
    results[san] = {}
    for fuzzer in fuzzers:
        results[san][fuzzer] = {}
        results[san][fuzzer][key] = []
        for trial_num in range(trials):
            outdir = f"/opt/out/{stem}-{fuzzer}-out"
            sp.run(f"rm -r {outdir}", shell=True)
            sp.run(f"rm {stem}.afl", shell=True)

            sp.run(f"rm -r afl-in", shell=True)
            sp.run(f"mkdir afl-in", shell=True)
            if len(inputs) > 0:
                for i in inputs:
                    sp.run(f"cp {i} afl-in", shell=True)
            else:
                sp.run("echo 'hi' > afl-in/hi.txt", shell=True)

            if "schedfuzz" in fuzzer:
               runner = SchedfuzzRunner(fuzzer)

            runner.pp_and_instrum(e, stem, args)
            print(f"running trial {trial_num} for {fuzzer} on {key}")
            runner.run(trial_num, prefix, time_budget, timeout, outdir, stem, args, libs)
            results[san][fuzzer][key].append(runner.analyze(trial_num))

    print(results)
    benchelapsed_secs = time.time() - benchstart
    print(f"run took {benchelapsed_secs}")
    with open('/opt/out/result.json', 'w') as fp:
        json.dump(results, fp)

    sp.run('cp *.csv /opt/out', shell=True, executable="/bin/bash")

if __name__ == "__main__":
    main()
