import subprocess as sp
import os
import glob
import re

main_exp = re.compile("main\(.*\)\s*\{")
def add_harness(file, source_only = True, out_dir = ".", tsan_dir = None):
    stem = os.path.splitext(file)[0]
    if os.path.exists(stem):
        os.remove(stem)

    ext = os.path.splitext(file)[1]
    if ext == ".i":
        if source_only:
            file = stem + ".c"
        new_file = stem + "-fuzz.c"
    else:
        new_file = stem + "-fuzz" + ext
    with open(file, 'r') as f:
        contents = f.read()

    # very hacky, but probably ok temporarily because SV-Comp is curated
    m = main_exp.search(contents)
    if m is None:
        print(f"skipping {file}")
        return
    new_contents = contents[0:m.end()] + "\n\tharness_init();\n" + contents[m.end():]

    with open(new_file, 'w') as f:
        f.write('#include "harness.c"\n')
        f.write(new_contents)

    if os.path.exists(stem):
        os.remove(stem)

    sp.run(f'clang -g -lpthread {new_file} -o {out_dir}/{stem}', shell=True)
    if tsan_dir is not None:
        sp.run(f'clang -fsanitize=thread -g -lpthread {new_file} -o {tsan_dir}/{stem}', shell=True)

