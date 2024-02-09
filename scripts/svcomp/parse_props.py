import subprocess as sp
import glob
import yaml
import sys

from add_harness import add_harness

out_dir = sys.argv[1]

yml_fs = glob.glob("*.yml")

total = 0
for f in yml_fs:
    with open(f, "r") as f:
        yml = yaml.load(f, Loader=yaml.Loader)

        for prop in yml["properties"]:
            use = False
            # "no-overflow", "def-behavior", "unreach-call"
            for violable_prop in ["valid-memsafety", "valid-deref", "valid-free", "valid-memtrack", "no-data-race", "unreach-call"]:
                if violable_prop in prop["property_file"] and not prop["expected_verdict"]:
                    use = True

            if use:
                add_harness(yml["input_files"], out_dir=out_dir)
                add_harness(yml["input_files"], tsan_dir=f"{out_dir}-tsan")
                total = total + 1

print(f"{total} examples which could be found by testing (true positives)")


