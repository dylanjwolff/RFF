import os
import pandas as pd
import json

filename = "bench-results/status-results-tsan.json"
with open(filename) as f:
    r = f.read()
r = json.loads(r)

df = pd.DataFrame(r)
df = df.T
print((df == "nonzero").sum())
print(df)



