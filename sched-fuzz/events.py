import struct
import random
import sys
import os
import json
from collections import defaultdict
from itertools import permutations

SEED = 0
AVG_DELAY = 3
DELAY_DIST = 0

def parse_iaddr(line):
    return line.split(']')[0].replace('[', '')

def parse_maddr(line):
    return line.split('|')[1].split('(')[1].replace(')', '')

SCHED_LEN = 2
f = open(sys.argv[1])
lines = f.readlines()
iaddr_and_maddr = [(parse_iaddr(l), parse_maddr(l)) for l in lines if ']' in l]
lines = [parse_iaddr(line) for line in lines if ']' in line]
lines = [int(line) for line in lines]

d = defaultdict(set)
for i, m in iaddr_and_maddr:
    d[m].add(int(i))

n_set = []
n2_set = []
for _, v in d.items():
    if len(v) > 1:
        n_set.append(list(v))
        n2_set.append(list(permutations(list(v), 2)))

all_pairs = [i for s in n2_set for i in s]
all_pairs = [{"r": i, "w": j} for (i, j) in all_pairs]
all_pairs_scores = [1]*len(all_pairs)
with open("event_pairs.json", "w") as out_file:
    json.dump({"rels" : all_pairs, "scores" : all_pairs_scores}, out_file, indent = 4)
print("Saw " + str(len(all_pairs)) + " possible event pairs in 'race prediction' log")
