
tally = {}
ict = {}
count = 0
with open("comb.log") as f:
    line = f.readline()
    i = 0
    while line:
        i += 1
        if i % 1000 == 0:
            print(f"done with {i}k")
        split = line.split('|')
        tid = split[0]
        split2 = split[1].split('(')
        op = split2[0]
        addr = split2[1].replace(")", "")
        instr = split[2].strip()
        if addr not in tally.keys(): 
            tally[addr] = { "writes" : set([]), "instrs" : set([]) }

        if op == "w":
            tally[addr]["writes"].add(tid)
        tally[addr]["instrs"].add(instr) 
        line = f.readline()
        if instr not in ict.keys():
            ict[instr] = 0
        ict[instr] += 1

    
uniq = set([])
for _, v in tally.items():
    
    if len(v["writes"]) > 1:
        for instr in v["instrs"]:
            uniq.add(instr)
print(len(uniq))
for instr in uniq:
    print(f"{instr} has count {ict[instr]}")
