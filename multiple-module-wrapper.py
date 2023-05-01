#!/usr/bin/env python3
import sys
import subprocess

def print_usage():
    print("Usage:")
    print("multiple-module-wrapper.py <profilers> -- <modules>")

if len(sys.argv) < 4:
    print_usage()
    exit(1)
try:
    delimiter = sys.argv.index("--")
except:
    print_usage()
    exit(1)
profilers = sys.argv[1:delimiter]
modules = sys.argv[delimiter+1:]

stats = {}

for profiler in profilers:
    stat = {}
    for module in modules:
        cmd = [profiler, module, "perf", "stat", "-p", "PID", "-x,", "-e", "cycles,instructions,branches,branch-misses,cache-references,cache-misses"]
        child = subprocess.Popen(cmd, shell=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        child.wait()
        outputs = child.stderr.readlines()
        for line in outputs:
            fileds = line.decode("utf-8").split(",")
            k = fileds[2]
            v = int(fileds[0])
            stat[k] = stat.get(k, 0) + v
    stats[profiler] = stat

print(stats)
print()
print("||cycles|instructions|branch-misses|cache-misses|")
print("|-|-|-|-|-|")
for k, v in stats.items():
    cycles = v["cycles:u"]
    instructions = v["instructions:u"]
    branch_misses = v["branch-misses:u"] / v["branches:u"]
    cache_misses = v["cache-misses:u"] / v["cache-references:u"]
    print("|{}|{}|{}|{}|{}|".format(k, cycles, instructions, branch_misses, cache_misses))
