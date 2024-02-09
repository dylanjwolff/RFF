import glob
import subprocess as sp
import os
import json

special_case_args = {
    "CB/aget-bug2": " -l dl.txt http://www.gnu.org/licenses/gpl.txt",
    "CB/pbzip2-0.9.4": " -k -f -p4 -1 -b1 @@",
    "Inspect_benchmarks/qsort_mt": " -n 32 -f 4 -h 4 -v" ,
    "Splash2/lu": " -p2 -b2 -n1" ,
    "NEW/xz": " -9 -k -T 4 -f @@", 
    "NEW/axel-2.17.10": " -n 3 -o test.pdf https://www.microsoft.com/en-us/research/uploads/prod/2019/09/sosp19-final193.pdf",
    "NEW/bzip2smp": " --no-ht -1 -p2 @@ output.txt",
    "NEW/lbzip2": " -k -t -1 -d -f -n2 @@", 
    "NEW/lrzip": " -t -p2 @@",
    "NEW/pbzip2-1.0.5": " -k -f -p3 @@", 
    "NEW/pbzip2-1.1.13": " -k -f -p3 @@", 
    "NEW/pixz-1.0.5": " -c -k -p 3 @@", 
    "NEW/pixz-1.0.7": " -c -k -p 3 @@", 

    "bzip": " @@", 
    "ferret": " corel lsh queries 1 1 4 output.txt",
    "streamcluster": " 2 5 1 10 10 5 none output.txt 4",

}

special_case_inputs = {
    "CB/pbzip2-0.9.4": ["/workdir/PERIOD/evaluation/CB/pbzip2-0.9.4/test.tar"],
    "NEW/bzip2smp": ["/workdir/PERIOD/evaluation/NEW/bzip2smp/bzip_input"],
    "NEW/lbzip2": ["/workdir/PERIOD/evaluation/NEW/lbzip2/lbzip2-2.5/tests/32767.bz2"],
    "NEW/lrzip": ["/workdir/PERIOD/evaluation/NEW/lrzip/POC"],
    "NEW/pbzip2-1.0.5": ["/workdir/PERIOD/evaluation/NEW/pbzip2-1.0.5/test.tar"],
    "NEW/pbzip2-1.1.13": ["/workdir/PERIOD/evaluation/NEW/pbzip2-1.1.13/test.tar"],
    "NEW/pixz-1.0.5": ["/workdir/PERIOD/evaluation/NEW/pixz-1.0.5/test.tar"],
    "NEW/pixz-1.0.7": ["/workdir/PERIOD/evaluation/NEW/pixz-1.0.7/test.tar"],

}

special_case_names = { 
    "aget": "aget-bug",
    "interlockedworkstealqueue": "iwsq",
    "interlockedworkstealqueuewithstate": "iwsqws",
    "stateworkstealqueue": "swsq",
    "workstealqueue": "wsq",

    "RADBench/bug2": "test-ctxt",
    "RADBench/bug3": "test-js",
    "RADBench/bug4": "test-time",
    "RADBench/bug5": "test-cvar",
    "RADBench/bug6": "test-rw",
}

dependencies = {
    "RADBench/bug2": [
        "wget https://ftp.mozilla.org/pub/mozilla.org/nspr/releases/v4.6.7/src/nspr-4.6.7.tar.gz",
        "wget https://ftp.mozilla.org/pub/mozilla.org/js/js-1.7.0.tar.gz"
    ],
    "RADBench/bug3": [
        "wget https://ftp.mozilla.org/pub/mozilla.org/nspr/releases/v4.7.3/src/nspr-4.7.3.tar.gz",
        "wget https://ftp.mozilla.org/pub/mozilla.org/js/js-1.8.0-rc1.tar.gz"
    ],
    "RADBench/bug4": [
        "wget https://ftp.mozilla.org/pub/mozilla.org/nspr/releases/v4.6.4/src/nspr-4.6.4.tar.gz",
    ],
    "RADBench/bug5": [
        "wget http://ftp.mozilla.org/pub/mozilla.org/nspr/releases/v4.8.6/src/nspr-4.8.6.tar.gz",
    ],
    "RADBench/bug6": [
        "wget https://ftp.mozilla.org/pub/mozilla.org/nspr/releases/v4.8/src/nspr-4.8.tar.gz",
    ],
}

special_case_libs = {
    "RADBench/bug2": "/workdir/PERIOD/evaluation/RADBench/bug2/src/js/src/Linux_All_DBG.OBJ:/workdir/PERIOD/evaluation/RADBench/bug2/src/nspr-4.6.7/target/dist/lib/",
    "RADBench/bug3": "/workdir/PERIOD/evaluation/RADBench/bug3/bin/install/lib:/workdir/PERIOD/evaluation/RADBench/bug3/src/js/src/Linux_All_DBG.OBJ",
}
    # "RADBench/bug4": "src/nspr-4.6.4/target/dist/lib",
    # "RADBench/bug5": "src/nspr-4.8.6/target/dist/lib/",


# Use clang, not PERIOD compiler. Comment out to get PERIOD
p = sp.run(f"grep -r -l 'dbds-clang' evaluation | xargs -I {{}} sed -i 's/\$ROOT_DIR[^ ]*dbds-clang-fast/clang/g' {{}}", shell=True)
p = sp.run(f"grep -r -l 'harness-with-DBDS.sh' evaluation/RADBench | xargs -I {{}} sed -i 's/.*harness-with-DBDS.sh//g' {{}}", shell=True)
p = sp.run(f"grep -r -l 'wllvm' evaluation/RADBench/bug3 | xargs -I {{}} sed -i 's/wllvm/clang/g' {{}}", shell=True)
# binary instrum not compatible with ASAN / TSAN
p = sp.run(f"grep -r -l 'sanitize=' evaluation  | xargs -I {{}} sed -i 's/-fsanitize=[^ ]*//g' {{}}", shell=True)

p = sp.run(f"sed -i 's/wget.*$/&\\n\\ttar -xf xz-5.2.5.tar.gz/g' /workdir/PERIOD/evaluation/NEW/xz/build.sh", shell=True)

radbench_dirs = glob.glob("evaluation/RADBench/bug*")
config = {}
for rbd in radbench_dirs:
    name = rbd.split("evaluation/")[1]
    for dep in (dependencies[name] if name in dependencies.keys() else []):
        sp.run(f"cd {rbd}/src; {dep}", shell = True)
        sp.run(f"cd {rbd}; make", shell = True)
    # sp.run("cd evaluation/RADBench; ./buildAll.sh", shell=True)
    sp.run(f"cd {rbd}; ./cleanDir.sh; make", shell=True)

    
    key = name
    e = f"/workdir/PERIOD/{rbd}/bin/{special_case_names[name]}"
    stem = os.path.basename(e)
    if key in special_case_libs:
        libs = special_case_libs[name]
    else:
        libs = []
    args = []
    inputs = []

    config[key] = { "name": name, "path" : e, "stem" : stem, "libs" : libs, "args" : args, "inputs" : inputs}

cve_dirs = glob.glob("evaluation/ConVul-CVE-Benchmarks/CVE*")
print(cve_dirs)
sp.run(f"cd evaluation/ConVul-CVE-Benchmarks; ./build_all.sh", shell = True)
for cve_d in cve_dirs:
    sp.run(f"echo 'no build needed' > {cve_d}/build.sh; chmod +x {cve_d}/build.sh", shell = True)

cats = glob.glob("evaluation/*/*/build.sh")
cats = cats + glob.glob("evaluation/NEW/*/build.sh")
cats = cats + ["evaluation/SafeStack/build.sh"]
cats2 = sorted(list(set([cat.split("/")[1] for cat in cats])))

non_progs = ["config.rpath", "gitlog-to-changelog", "update-po", "depcomp", "missing", "install-sh", "compile", "config.status", "config.guess", "test-driver", "config.sub", "null_macros", "configure", "config.status", "libtool"]

not_working = []
for b in cats:
    build_basename = os.path.basename(b)
    d = os.path.dirname(b)
    p = sp.run(f"cd {d}; ./{build_basename}", shell=True)

    fs = glob.glob(f"{d}/**/**/**/*")
    fs = fs + glob.glob(f"{d}/**/**/*")
    fs = fs + glob.glob(f"{d}/**/*")
    fs = fs + glob.glob(f"{d}/*")
    es = [e for e in fs if os.access(e, os.X_OK) and os.path.isfile(e)]
    e = [e for e in es if os.path.basename(e) not in non_progs and not e.endswith(".sh")]
    
    if "xz" in d and "pixz" not in d:
        e = ["/workdir/PERIOD/evaluation/NEW/xz/xz-5.2.5/src/xz/xz"]
    if len(e) == 1:
        e = os.path.join(os.getcwd(), e[0])
        stem = os.path.basename(e)
        name = stem.lower()
        key = d.replace("evaluation/", "")

        if key in special_case_args.keys():
            args = special_case_args[key]
        else:
            args = ""

        if key in special_case_inputs.keys():
            inputs = special_case_inputs[key]
        else:
            inputs = []

        # TODO special case libs

        if name in special_case_names.keys():
            name = special_case_names[name]
        if "stringbuffer" in e:
            name = "stringbuffer"

        config[key] = { "name": name, "path" : e, "stem" : stem, "libs" : [], "args" : args, "inputs" : inputs}
    else:
        s = f"Non-unique exec {d} has {len(e)}"
        not_working = not_working + [" ".join(e)]

print("\n\n".join(not_working))
with open('sctbench_subject_config.json', 'w') as fp:
    json.dump(config, fp)
