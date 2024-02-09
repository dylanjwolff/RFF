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
    no_tsan = pd.concat([no_tsan, pd.DataFrame(r["none"])])



# tsan = pd.DataFrame(r["tsan"])

print("nontrivial found")
print(no_tsan.apply(lambda x: x.apply(lambda y: isinstance(y, numbers.Number) and y > 0)).sum())
print()
print("nontrivial not found")
print(no_tsan.apply(lambda x: x.apply(lambda y: isinstance(y, numbers.Number) and y == 0)).sum())
print()
print("timeouts")
print((no_tsan == "TIMEOUT").sum())
print()
print("crashes")
print((no_tsan == "CRASH").sum())
print()
# print(((no_tsan["afl"] != 0) & (no_tsan["schedfuzz"] == 0)))
# print( no_tsan.to_csv())
# print((tsan > 0).sum())

period =  pd.read_csv("period.csv")
# period["benchmark"] = period["benchmark"].apply(lambda x: x.split('.')[1].lower())
# print(period)
period = period[["benchmark", "buggy_scheds"]]

no_tsan.index.name = "benchmark"

df = no_tsan.reset_index(level=0)
df = df.set_index("benchmark")
joined = period.join(df, how="left", on="benchmark")
print(joined)
df = df.reset_index(level=0)
# print(period[~period["benchmark"].isin(df["benchmark"])])
# print(df[~df["benchmark"].isin(period["benchmark"])])


