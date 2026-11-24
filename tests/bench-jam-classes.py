#!/usr/bin/env python3
"""Compare canonical jam encoders on the same ordered set of input files."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import subprocess


def measure(binary, mode, cpu, inputs):
    command = ["/usr/bin/time", "-f", "rss_kib=%M", "taskset", "-c", str(cpu),
               str(binary.resolve()), mode, *map(str, inputs)]
    run = subprocess.run(command, capture_output=True, text=True)
    if run.returncode:
        raise RuntimeError(f"{binary} exited {run.returncode}\n{run.stderr}")
    records = []
    for line in run.stdout.splitlines():
        name, ns, bits = line.split("\t")
        records.append((name, int(ns), int(bits)))
    if [row[0] for row in records] != list(map(str, inputs)):
        raise ValueError(f"{binary} returned a different input sequence")
    return dict(records=records, ns=sum(row[1] for row in records),
                rss_kib=int(run.stderr.rsplit("rss_kib=", 1)[1].strip()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for label in ("develop", "branch", "candidate"):
        parser.add_argument(label, type=Path)
    parser.add_argument("inputs", type=Path, nargs="+")
    parser.add_argument("--cores", required=True, type=int, nargs=3)
    parser.add_argument("--rounds", type=int, default=6)
    args = parser.parse_args()
    if args.rounds < 1 or len(set(args.cores)) != 3:
        parser.error("use a positive round count and three distinct cores")
    builds = {"develop": args.develop, "branch": args.branch, "candidate": args.candidate}
    with ThreadPoolExecutor(3) as pool:
        for trial in range(args.rounds + 1):
            shift = trial % 3
            cores = args.cores[shift:] + args.cores[:shift]
            for mode in (("fib", "xeno") if trial % 2 else ("xeno", "fib")):
                jobs = [(name, cpu, pool.submit(measure, binary, mode, cpu, args.inputs))
                        for (name, binary), cpu in zip(builds.items(), cores)]
                for label, cpu, job in jobs:
                    print(json.dumps(dict(trial=trial, build=label, mode=mode, cpu=cpu,
                                          **job.result())), flush=True)


if __name__ == "__main__":
    main()
