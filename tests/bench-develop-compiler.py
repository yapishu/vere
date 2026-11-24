#!/usr/bin/env python3
"""Compare the develop and optimized eval binaries on urbit/benchmark compiler inputs."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


def evaluate(binary, frame, cpu):
    command = ["taskset", "-c", str(cpu), str(binary), "eval", "--cue", "--newt"]
    # Vere opens its standard streams in nonblocking mode. A regular file
    # accepts the complete jam even when it exceeds a pipe buffer.
    with tempfile.TemporaryFile() as output:
        run = subprocess.run(command, input=frame, stdout=output, stderr=subprocess.PIPE,
                             env={**os.environ, "U3_DESK_BENCH": "1"}, timeout=300)
        if run.returncode:
            raise RuntimeError(f"{binary} exited {run.returncode}: {run.stderr!r}")
        output.seek(0)
        product = output.read()
    if not re.search(rb"desk ivory_core=(\d+) shipped_core=\1 equal=yes", run.stderr):
        raise RuntimeError(f"Compiler cores differ for {binary}: {run.stderr!r}")
    match = re.search(rb"desk ns=(\d+) input_mug=(\d+) product_mug=(\d+)", run.stderr)
    if not match:
        raise RuntimeError(f"Missing benchmark counters from {binary}")
    ns, input_mug, product_mug = map(int, match.groups())
    return product, dict(ns=ns, input_mug=input_mug, product_mug=product_mug)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("develop", type=Path)
    parser.add_argument("optimized", type=Path)
    parser.add_argument("desk", type=Path, help="checkout of urbit/benchmark")
    parser.add_argument("--cores", type=int, nargs=2, required=True)
    parser.add_argument("--rounds", type=int, default=6, help="measured pairs after warm-up")
    parser.add_argument("--goldens", type=Path, help="optional directory of NAME-stock.jam files")
    args = parser.parse_args()
    if len(set(args.cores)) < 2 or args.rounds < 1:
        parser.error("specify two distinct cores and at least one measured round")
    bar = args.desk / "desk/bar"
    manifest = {row["name"]: row for row in json.loads((bar / "tests.json").read_text())}
    workloads = ("hoon", "prelude")
    frames = {}
    goldens = {}
    for name in workloads:
        jam = (bar / (name + ".noun")).read_bytes()
        frames[name] = b"\0" + len(jam).to_bytes(4, "little") + jam
        if args.goldens:
            goldens[name] = (args.goldens / (name + "-stock.jam")).read_bytes()
    builds = {"develop": args.develop.resolve(), "optimized": args.optimized.resolve()}
    with ThreadPoolExecutor(2) as pool:
        for trial in range(args.rounds + 1):
            order = workloads if trial % 2 == 0 else reversed(workloads)
            cores = args.cores if trial % 2 == 0 else args.cores[::-1]
            for name in order:
                jobs = [(label, cpu, pool.submit(evaluate, binary, frames[name], cpu))
                        for (label, binary), cpu in zip(builds.items(), cores)]
                for label, cpu, job in jobs:
                    jam, counts = job.result()
                    if name not in goldens:
                        goldens[name] = jam  # The first build in the warm-up is develop.
                    if jam != goldens[name]:
                        raise ValueError(f"{label} produced a different jam for {name}")
                    for field, key in (("input_mug", "noun_mug"), ("product_mug", "result_mug")):
                        if counts[field] != int(manifest[name][key].replace(".", "")):
                            raise ValueError(f"{label}: {name} did not match published {key}")
                    print(json.dumps(dict(trial=trial, workload=name, build=label, cpu=cpu,
                                          sha256=hashlib.sha256(jam).hexdigest(), **counts)),
                          flush=True)


if __name__ == "__main__":
    main()
