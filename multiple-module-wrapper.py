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
try:
    logfile = open("profiler.log", "at")
except:
    logfile = open("profiler.log", "wt")

for profiler in profilers:
    stat = {}
    for module in modules:
        logfile.write("{}: {}\n".format(profiler, module))
        cmd = [profiler, module, "perf", "stat", "-p", "PID", "-x,", "-e", "cycles,instructions,branches,branch-misses,L1-dcache-loads,L1-dcache-load-misses"]
        child = subprocess.Popen(cmd, shell=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        child.wait()
        outputs = child.stderr.readlines()
        for line in outputs:
            logfile.write(line.decode("utf-8"))
            fileds = line.decode("utf-8").split(",")
            k = fileds[2]
            try:
                v = int(fileds[0])
            except:
                v = 0
            stat[k] = stat.get(k, 0) + v
    stats[profiler] = stat

logfile.close()

print(stats)
print()
print("||cycles|instructions|branch-misses|cache-misses|")
print("|-|-|-|-|-|")
for k, v in stats.items():
    cycles = v["cycles:u"]
    instructions = v["instructions:u"]
    branch_misses = v["branch-misses:u"] / v["branches:u"]
    cache_misses = v["L1-dcache-load-misses:u"] / v["L1-dcache-loads:u"]
    print("|{}|{}|{}|{}|{}|".format(k, cycles, instructions, branch_misses, cache_misses))
