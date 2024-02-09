import os
import glob
import subprocess as sp
import shutil
import sys


out_dir = sys.argv[1]

dirs = glob.glob("pthread*")

os.mkdir(out_dir)
os.mkdir(f"{out_dir}-tsan")
files = ["harness.c", "parse_props.py", "add_harness.py"]
for suite_dir in dirs:
    for f in files:
        shutil.copy(f, suite_dir)

    sp.run(f"cd {suite_dir}; python3 parse_props.py {out_dir}", shell=True)
