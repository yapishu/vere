#!/usr/bin/env python3
"""Sample a static x86-64 Linux Vere process during a named stderr phase."""

import argparse
from bisect import bisect_right
from collections import Counter
import ctypes
import json
import os
from pathlib import Path
import platform
import random
import signal
import subprocess
import sys
import tempfile
import threading
import time


def sample(binary, args):
    libc = ctypes.CDLL(None, use_errno=True)
    seize, getregs, resume = 0x4206, 12, 7
    # SIGURG has an ignored default disposition. Direct it to the sampled thread.
    tgkill = 234
    registers = (ctypes.c_ulonglong * 27)()
    counts = Counter()
    start, end = args.start.encode(), args.end.encode()
    with tempfile.NamedTemporaryFile() as stderr, open(stderr.name, 'rb') as reader:
        with subprocess.Popen([binary, *args.arguments], stderr=stderr) as proc:
            def ptrace(operation, data=None):
                if libc.ptrace(operation, proc.pid, None, data) == -1:
                    err = ctypes.get_errno()
                    raise OSError(err, os.strerror(err))

            stopped = threading.Event()
            clock_id = ctypes.c_int()
            try:
                ptrace(seize)
                err = libc.clock_getcpuclockid(proc.pid, ctypes.byref(clock_id))
                if err:
                    raise OSError(err, os.strerror(err))
            except OSError:
                proc.kill()
                raise
            clock = clock_id.value

            def tick():
                try:
                    while not stopped.is_set():
                        target = time.clock_gettime(clock) + random.uniform(0.003, 0.006)
                        while (left := target - time.clock_gettime(clock)) > 0:
                            if stopped.wait(left):
                                return
                        if libc.syscall(tgkill, proc.pid, proc.pid, signal.SIGURG):
                            return
                except (ProcessLookupError, ValueError, OSError):
                    return

            timer = threading.Thread(target=tick)
            timer.start()
            active, pending = not start, b''
            try:
                while True:
                    _, status = os.waitpid(proc.pid, 0)
                    if not os.WIFSTOPPED(status):
                        proc.returncode = os.waitstatus_to_exitcode(status)
                        break
                    sig = os.WSTOPSIG(status)
                    if sig != signal.SIGURG:
                        ptrace(resume, sig)
                        continue
                    ptrace(getregs, ctypes.byref(registers))
                    ptrace(resume)
                    data = pending + reader.read()
                    lines = data.split(b'\n')
                    pending = lines.pop()
                    for line in lines:
                        if start and start in line:
                            active = True
                        if end and end in line:
                            active = False
                    if active:
                        counts[registers[16]] += 1  # RIP in user_regs_struct
            finally:
                stopped.set()
                timer.join()
                if proc.returncode is None:
                    proc.kill()
                stderr.seek(0)
                for line in stderr:
                    sys.stderr.buffer.write(line)
            if proc.returncode:
                raise SystemExit(f'{binary} exited with status {proc.returncode}')
    return counts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--start', default='', help='begin at this stderr substring')
    parser.add_argument('--end', default='', help='end at this stderr substring')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('binary', type=Path, help='static, non-PIE executable with symbols')
    parser.add_argument('arguments', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if sys.platform != 'linux' or platform.machine() != 'x86_64':
        parser.error('the sampler requires x86-64 Linux')
    binary = args.binary.resolve()
    with binary.open('rb') as stream:
        header = stream.read(20)
    if header[:6] != b'\x7fELF\x02\x01' or header[16:20] != b'\x02\x00\x3e\x00':
        parser.error('use an x86-64 non-PIE ELF executable')
    segments = subprocess.check_output(['readelf', '-lW', binary], text=True)
    if any(line.lstrip().startswith('INTERP ') for line in segments.splitlines()):
        parser.error('use a statically linked Vere executable')
    symbols = []
    for line in subprocess.check_output(['nm', '-n', binary], text=True).splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[1] in ('t', 'T', 'w', 'W'):
            symbols.append((int(fields[0], 16), fields[2]))
    if not symbols:
        parser.error('binary contains no text symbols')
    symbols.sort()
    addresses = [address for address, _ in symbols]
    counts = Counter()
    for address, count in sample(binary, args).items():
        index = bisect_right(addresses, address) - 1
        counts[symbols[index][1] if index >= 0 else 'unknown'] += count
    if not counts:
        raise SystemExit('no CPU samples in the requested phase')
    args.output.write_text(json.dumps(dict(counts.most_common()), indent=2) + '\n')


if __name__ == '__main__':
    main()
