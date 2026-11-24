#!/usr/bin/env python3
"""Compare Vere replay wall times on independent copies of a frozen pier."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import re
import subprocess
import tempfile
import time


def replay(binary, pier, cpu, args):
    command = ["taskset", "-c", str(cpu), str(binary), "play", "-y"]
    if args.full:
        command.append("-f")
    command.append(str(pier))
    start = time.monotonic()
    run = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         timeout=args.timeout)
    seconds = time.monotonic() - start
    output = run.stdout.decode(errors="replace")
    complete = re.search(rf"play \({args.last}\): done\b", output)
    events = re.search(rf"^play: events {args.first}-{args.last}\b", output, re.MULTILINE)
    if (run.returncode or not events or not complete or
            "playback complete" not in output or "mug mismatch" in output):
        raise RuntimeError(f"{binary} replay failed (exit {run.returncode})\n{output[-3000:]}")
    return seconds


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("develop", type=Path)
    parser.add_argument("optimized", type=Path)
    parser.add_argument("pier", type=Path, help="frozen, stopped input pier")
    parser.add_argument("--cores", type=int, nargs=2, required=True)
    parser.add_argument("--first", type=int, required=True, help="expected first replayed event")
    parser.add_argument("--last", type=int, required=True, help="expected last replayed event")
    parser.add_argument("--full", action="store_true", help="bootstrap from the event log")
    parser.add_argument("--rounds", type=int, default=4)
    parser.add_argument("--timeout", type=int, default=900, help="seconds per process")
    parser.add_argument("--scratch-dir", type=Path, help="filesystem for disposable copies")
    args = parser.parse_args()
    if len(set(args.cores)) < 2 or args.rounds < 1 or args.first > args.last:
        parser.error("specify distinct cores, a positive round count and an ordered event range")
    if not (args.pier / ".urb/log").is_dir():
        parser.error("the input pier must contain .urb/log")
    builds = {"develop": args.develop.resolve(), "optimized": args.optimized.resolve()}
    for trial in range(args.rounds):
        cores = args.cores if trial % 2 == 0 else args.cores[::-1]
        with tempfile.TemporaryDirectory(dir=args.scratch_dir) as scratch:
            for label in builds:
                subprocess.run(["cp", "-RL", "--reflink=auto", "--sparse=always", "--",
                                str(args.pier), str(Path(scratch) / label)], check=True)
            with ThreadPoolExecutor(2) as pool:
                jobs = [(label, cpu, pool.submit(replay, binary, Path(scratch) / label, cpu, args))
                        for (label, binary), cpu in zip(builds.items(), cores)]
                for label, cpu, job in jobs:
                    print(json.dumps(dict(trial=trial, build=label, cpu=cpu,
                                          wall_s=job.result())), flush=True)


if __name__ == "__main__":
    main()
