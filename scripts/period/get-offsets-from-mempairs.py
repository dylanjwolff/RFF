import subprocess as sp
from line2addr import line2addr as l2a
import os
import sys

def get_addrs(file, line, binary, bin_lines):
    res = l2a.get_file_line(file, line, binary, bin_lines=bin_lines)
    return res

src_instr_points = sys.argv[1]
binary = sys.argv[2]
src_base_dir = sys.argv[3]

with open(src_instr_points) as f:
    lines = f.readlines()
lines_a = [line.split(" ")[0] for line in lines]
lines_b = [line.split(" ")[1] for line in lines]
lines = lines_a + lines_b
lines = list(set(lines))
lines.sort()
print(f"{len(lines)} total events")

instrms = []

bin_lines = l2a.get_binary_lines(binary)
for line in lines:
    split_line = line.split(":")
    addrs = get_addrs(split_line[0], split_line[1], binary, bin_lines)
    if len(addrs) <= 0:
        continue

    print(f"found addresses: {addrs}")
    addrs = [int(addr, 16) for addr in addrs]
    low = min(addrs)
    high = max(addrs)
    addrs = range(low, high + 1)

    for addr in addrs:
        instrms.append((addr, line))

instrms = (dict(instrms))
# @TODO different lines with duplicate addresses will cause problems
instrms_s = "\n".join([f"{hex(region)},{loc},{i},0" for i, (region, loc) in enumerate(instrms.items())])

out_file = "out.csv"
with open(out_file, "w") as f:
    f.write(instrms_s)

