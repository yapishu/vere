#!/usr/bin/env python3
"""Time full cue and indexed projections on the same ordered set of jam files."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import subprocess


def measure(binary, mode, cpu, args):
    cmd = ["/usr/bin/time", "-f", "rss_kib=%M", "taskset", "-c", str(cpu),
           str(binary.resolve()), mode, str(args.ivory)]
    if mode in ("project", "view"):
        cmd.append(args.axes)
    cmd.extend(map(str, args.inputs))
    run = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if run.returncode:
        raise RuntimeError(f"{binary} exited {run.returncode}\n{run.stderr}")
    rows = []
    for line in run.stdout.splitlines():
        name, ns, size, digest = line.split("\t")
        rows.append((name, int(ns), int(size), digest))
    if [row[0] for row in rows] != list(map(str, args.inputs)):
        raise ValueError(f"{binary} returned a different input sequence")
    rss = int(run.stderr.rsplit("rss_kib=", 1)[1].strip())
    return rows, rss


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["develop", "optimized", "view", "ivory"]:
        parser.add_argument(name, type=Path)
    parser.add_argument("inputs", nargs="+", type=Path)
    parser.add_argument("--cores", required=True, type=int, nargs=3)
    parser.add_argument("--axes", default="2,14")
    parser.add_argument("--modes", nargs=3, choices=["nock", "xeno", "project", "view"],
                        default=["project", "project", "view"],
                        help="decoder mode for each binary in positional order")
    parser.add_argument("--rounds", type=int, default=6)
    args = parser.parse_args()
    if len(set(args.cores)) != 3 or args.rounds < 1:
        parser.error("specify three distinct cores and a positive round count")
    builds = [(label, getattr(args, label), mode)
              for label, mode in zip(("develop", "optimized", "view"), args.modes)]
    with ThreadPoolExecutor(3) as pool:
        for trial in range(args.rounds + 1):
            offset = trial % 3
            cpus = args.cores[offset:] + args.cores[:offset]
            jobs = [(label, cpu, pool.submit(measure, exe, mode, cpu, args))
                    for (label, exe, mode), cpu in zip(builds, cpus)]
            expected = None
            for label, cpu, job in jobs:
                rows, rss = job.result()
                products = [(name, size, digest) for name, _, size, digest in rows]
                if expected is not None and products != expected:
                    raise ValueError(f"different projected products on {label}, trial {trial}")
                expected = products
                print(json.dumps(dict(trial=trial, build=label, cpu=cpu, rss_kib=rss,
                                      ns=sum(row[1] for row in rows), outputs=rows)), flush=True)


if __name__ == "__main__":
    main()
