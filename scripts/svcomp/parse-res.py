import json
import pandas as pd
import sys

fpath = sys.argv[1]


with open(fpath) as f:
    r = f.read()
r = json.loads(r)
no_tsan = pd.DataFrame(r["none"])
# tsan = pd.DataFrame(r["tsan"])
print("found nontrivial")
print((no_tsan > 0).sum())
print("not found nontrivial")
print((no_tsan == 0).sum())
print("immediately crashing")
print((no_tsan < 0).sum())
# print(((no_tsan["afl"] != 0) & (no_tsan["schedfuzz"] == 0)))
print( no_tsan["schedfuzz"] < 0 )
# print((tsan > 0).sum())
