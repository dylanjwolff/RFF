import subprocess as sp
import sys

NUM_TRIALS=1

sp.run("docker build -f dockerfiles/Dockerfile.base -t schedfuzz-base .", shell=True)

sp.run("docker build --build-arg=INST_FILTER=DYNAMIC -f scalability/sqlite3_threadtest5.Dockerfile -t fuzz-threadtest5-rff .", shell=True)
sp.run("docker build --build-arg=INST_FILTER=DYNAMIC --build-arg=POS_ONLY=1 -f scalability/sqlite3_threadtest5.Dockerfile -t fuzz-threadtest5-pos .", shell=True)


sp.run("docker build -f scalability/x264.Dockerfile -t fuzz-x264-rff .", shell=True)
sp.run("docker build --build-arg=POS_ONLY=1 -f scalability/x264.Dockerfile -t fuzz-x264-pos .", shell=True)

for trial_num in range(1, NUM_TRIALS + 1):
    sp.run(f"docker tag fuzz-threadtest5-rff fuzz-threadtest5-rff:{trial_num}", shell=True)
    sp.run(f"docker tag fuzz-threadtest5-pos fuzz-threadtest5-pos:{trial_num}", shell=True)

    sp.run(f"docker tag fuzz-x264-rff fuzz-x264-rff:{trial_num}", shell=True)
    sp.run(f"docker tag fuzz-x264-pos fuzz-x264-pos:{trial_num}", shell=True)


p = sp.run('docker images | grep fuzz | grep -v latest | tr -s " " | cut -f 1,2 -d " " | sed "s/ /:/g"', shell=True, capture_output=True)
errs = p.stderr.decode("utf-8")
print(errs)
images = p.stdout.decode("utf-8")
print("done")
print(images)
