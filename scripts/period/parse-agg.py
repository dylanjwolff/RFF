import json
import pandas as pd
import sys
import numbers

paths = sys.argv[1:]


no_tsan = pd.DataFrame()
for fpath in paths:
    with open(fpath) as f:
        r = f.read()
    r = json.loads(r)
    print(r)
    x = pd.concat({k: pd.concat({ki: pd.DataFrame(vi) for ki, vi in v.items()}) for k, v in r["none"].items()})
    print(x)
    print("------")
    no_tsan = pd.concat([no_tsan, x]) 

print("done")
no_tsan.to_csv("full-data.csv")
# results = (no_tsan.unstack().T.swaplevel().unstack().mean().unstack().unstack().T)
# results = no_tsan
# print(results)
# results.to_csv("agg-results.csv")
