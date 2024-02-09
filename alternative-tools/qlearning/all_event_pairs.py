import json

fname = "comb.log"
locks_fname = "mapping.log"

vars = {}
with open(fname) as f:
    lines = f.readlines() 
with open(locks_fname) as f:
    lines = lines + f.readlines()

    for line in lines:
        try:
            thread, access, instr = line.strip().split('|')
        except:
            continue

        if access.startswith("r(") or access.startswith("acq("):
            var = access.split('(')[1].split(')')[0]
            op = "READ"
        elif access.startswith("w(") or access.startswith("rel("):
            var = access.split('(')[1].split(')')[0]
            op = "WRITE"
        elif access.startswith("a_rw("):
            var = access.split('(')[1].split(')')[0]
            op = "A_RW"
        else:
            continue

        if var not in vars:
            vars[var] = {}
        
        if thread not in vars[var]:
            vars[var][thread] = {}

        if op not in vars[var][thread]:
            vars[var][thread][op] = set([])

        vars[var][thread][op].add(instr)
        
    pairs = []
    p = set([])
    for var in vars:
        for thread in vars[var]:
            if "READ" in vars[var][thread]:
                for r in vars[var][thread]["READ"]:
                    for other_thread in vars[var]:
                        if other_thread != thread and "WRITE" in vars[var][other_thread].keys():
                            for w in vars[var][other_thread]["WRITE"]:
                                t = (int(r), int(w))
                                if t not in p:
                                    p.add(t)
                                    pairs = pairs + [{"r": int(r), "w": int(w)}]
                        
                        elif other_thread != thread and "A_RW" in vars[var][other_thread].keys():
                            for w in vars[var][other_thread]["A_RW"]:
                                t = (int(r), int(w))
                                if t not in p:
                                    p.add(t)
                                    pairs = pairs + [{"r": int(r), "w": int(w)}]

            elif "A_RW" in vars[var][thread]:
                for r in vars[var][thread]["A_RW"]:
                    for other_thread in vars[var]:
                        if other_thread != thread and "WRITE" in vars[var][other_thread].keys():
                            for w in vars[var][other_thread]["WRITE"]:
                                t = (int(r), int(w))
                                if t not in p:
                                    p.add(t)
                                    pairs = pairs + [{"r": int(r), "w": int(w)}]
                        
                        elif other_thread != thread and "A_RW" in vars[var][other_thread].keys():
                            for w in vars[var][other_thread]["A_RW"]:
                                t = (int(r), int(w))
                                if t not in p:
                                    p.add(t)
                                    pairs = pairs + [{"r": int(r), "w": int(w)}]

    dto = {}
    dto["rels"] = pairs
    dto["scores"] = [0.0] * len(pairs)
    s = json.dumps(dto)

with open("all_pairs.json", "w") as f:
    f.write(s)

