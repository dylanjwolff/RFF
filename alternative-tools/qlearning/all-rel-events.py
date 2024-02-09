import json
import sys

fpath = sys.argv[1]
if len(sys.argv) > 2:
    opath = sys.argv[2] 
else:
    opath = "out.csv"

with open(fpath) as f:
    contents = f.read()
    data= json.loads(contents)

all = set([])
for rel in data["rels"]:
    all.add(hex(int(rel["r"])))
    all.add(hex(int(rel["w"])))

with open(opath, 'w') as f:
    for instr in all:
        f.write(f"{instr},\n")
print(all)