import subprocess as sp
import glob

working_dir = "integration_test_tmp"

sp.run(f"sudo chown -R $USER .", shell=True)
sp.run("docker build -f dockerfiles/Dockerfile.base -t schedfuzz-base .", shell=True)
sp.run("docker build -f dockerfiles/Dockerfile.sum -t fuzz-sum .", shell=True)
sp.run("docker build -f dockerfiles/Dockerfile.path -t fuzz-path .", shell=True)
sp.run("docker build -f dockerfiles/Dockerfile.period -t fuzz-period .", shell=True)

sp.run(f"mkdir -p {working_dir}", shell=True)
sp.run(f"docker run -v $(pwd)/{working_dir}:/opt/out -e NUM_TRIALS=1 -e FUZZERS=schedfuzz -e TARGET_KEY=CS/reorder_20 -e TIME_BUDGET=15s fuzz-period", shell=True)
sp.run(f"sudo chown -R $USER .", shell=True)

scheds = glob.glob(f"{working_dir}/**/scheds/**/*")
crashes = len([c for c in scheds if "crash" in c])
assert(crashes > 0)
sp.run(f"rm -r {working_dir}", shell=True)


sp.run(f"mkdir -p {working_dir}", shell=True)
sp.run(f"docker run -v $(pwd)/{working_dir}:/opt/out -e TIME_BUDGET=30s fuzz-sum", shell=True)
sp.run(f"sudo chown -R $USER .", shell=True)

scheds = glob.glob(f"{working_dir}/**/scheds/**/*")
crashes = len([c for c in scheds if "crash" in c])
assert(crashes == 0)
sp.run(f"rm -r {working_dir}", shell=True)

