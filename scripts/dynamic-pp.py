import subprocess as sp
import glob
import os
import json
import time
import sys

def dynamic_pp(uninst, sut_name, args):
    sp.run(f"rm -f out.csv events.log mapping.log comb.log event_pairs.json", shell=True)
    sp.run(f"./instrument.sh {uninst}", shell=True)
    sp.run(f"TO_LOG=true LOG_DIR=events.log LOG_DIR_MAP=mapping.log LD_PRELOAD=$(pwd)/libsched.so ./{sut_name}.afl {args}", shell=True)
    sp.run(f"python3 ../race_predictor/fuzz_preprocess.py events.log mapping.log comb.log", shell=True)
    sp.run("java -cp ../race_predictor/rapid.jar:../race_predictor/lib/* Main -t comb.log -s event_pairs.json -w true", shell=True)
    sp.run("python3 all-rel-events.py event_pairs.json out.csv", shell=True)
    sp.run(f"rm -f ./{sut_name}.afl", shell=True)

    sp.run(f"./selective-instrument.sh {uninst} out.csv", shell=True)


def main():
    input = sys.argv[1]

    sut_name = os.getenv("SUT_NAME") 
    uninst = os.getenv("UNINST")
    args = os.getenv("ARGS")

    args = args.replace("@@", input)

    dynamic_pp(uninst, sut_name, args)


if __name__ == "__main__":
    main()
