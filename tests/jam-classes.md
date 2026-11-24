# Canonical jam by subtree class

Vere can encode a noun by numbering its distinct subtrees and computing their first bit positions in the same walk. The Hoon `jam` jet uses this layout to write into an exactly sized loom slab. The IPC encoder uses it to allocate its byte buffer once. Both paths use the existing jam atom and backreference encoding. The feature flag is `U3_JAM_CLASSES`. [Class numbering and output](../pkg/noun/jam_classes.inc) · [Production entry points](../pkg/noun/serial.c)

## How the offsets work

A cell's class is the ordered pair of its children's class IDs. A direct atom is identified by its value. For indirect atoms, the table uses the mug to locate candidates and checks equality of the full payload. A second table recognizes physical sharing when a cell can have more than one incoming edge. The encoder conservatively memoizes cell identities across roads and during deferred reference counting. Both tables belong to the current invocation.

The first pass tentatively counts each opening cell tag and its children. When a cell closes, both child IDs are known. For a new class, its first position is the saved opening cursor. For an existing class, the encoder rewinds the cursor to that opening and charges a backreference to the first occurrence. For example, two separately allocated copies of `[42 7]` receive one class ID. Rewinding the second copy is safe because every descendant class already occurred under the first copy. The first positions survive the rewind. Repeated atoms follow jam's usual width rule, including the case where a literal wins a tie.

At the end of that walk the cursor is the exact output length. The encoder releases the search tables, keeps the class records, and visits them in preorder to emit the bytes. Both walks use explicit road stacks. Vere's separate `ur` library also represents interned nouns and supplies an independent canonical encoder for the tests. [Independent encoder](../pkg/ur/serial.c)

## Real event records

| Workload | Upstream develop | Branch | Candidate | Less vs branch | Less vs develop |
|---|---:|---:|---:|---:|---:|
| Hoon slab, 143 records (ms) | 3,012.9 | 3,306.1 | 1,185.5 | +64.1% | +60.7% |
| Hoon slab, other 142 (ms) | 112.5 | 108.6 | 46.6 | +57.1% | +58.6% |
| Hoon slab, process peak RSS (MiB) | 1,707.4 | 1,108.5 | 820.0 | +26.0% | +52.0% |
| IPC buffer, 143 records (ms) | 2,774.6 | 3,058.6 | 1,165.7 | +61.9% | +58.0% |
| IPC buffer, other 142 (ms) | 114.2 | 115.8 | 50.7 | +56.2% | +55.6% |
| IPC buffer, process peak RSS (MiB) | 1,455.4 | 856.6 | 819.9 | +4.3% | +43.7% |

The controls are upstream `urbit/vere:develop` at `697fc95ddd` and this branch at `e52a0c6ab8`. The candidate is `9475dc5bb0`. The branch and candidate use the cumulative optimization flags and `U3_CUE_VIEW`. The candidate also enables `U3_JAM_CLASSES`. Ordinary gate-field entry stays off in both. Builds use Zig 0.15.2, ReleaseFast, x86-64 Linux musl and a 32-bit loom. Percentages report reduction in the median. [Branch configuration and fixtures](develop-benchmarks.md) · [Individual samples and per-record medians](jam-class-samples.jsonl)

Each process reads the same 143 canonical jams from the frozen fake ship, in order, and decodes each record immediately before timing its production encoder. Every invocation checks the bit length, byte length and every output byte against the input file. Timing covers class construction, output allocation, writing and scratch cleanup. It excludes cue, file I/O, output comparison and releasing the output. The corpus contains 240,504,745 bytes, of which event 17 accounts for 198,994,510. The 142-record row sums the same trials with event 17 removed. Peak RSS comes from GNU time and covers the complete process, including cue and the verification buffers. Both branch builds use the same decoder. The upstream comparison also reflects the heap layout left by its decoder.

There is one warm-up and six measured triples for each encoder. The builds rotate across three physical P-cores of a shared i7-12700K, and the order of the two encoders alternates. Other benchmark processes used separate cores on the same host, so memory bandwidth and CPU frequency can affect absolute times.

## Inside a running ship

| Workload | Upstream develop | Branch | Candidate | Less vs branch | Less vs develop |
|---|---:|---:|---:|---:|---:|
| List of 80,000 consecutive atoms (ms) | 25.069 | 24.590 | 10.819 | +56.0% | +56.8% |
| 80,000 distinct pairs (ms) | 51.151 | 51.008 | 26.889 | +47.3% | +47.4% |
| 80,000 records with 1,984 possible values (ms) | 16.447 | 15.639 | 12.591 | +19.5% | +23.4% |
| 80,000 references to one record (ms) | 12.384 | 12.727 | 7.298 | +42.7% | +41.1% |
| One MiB atom inside a pair (ms) | 0.247 | 0.259 | 0.226 | +12.7% | +8.5% |
| 20,000 separate two-atom messages (ms) | 12.901 | 11.680 | 6.824 | +41.6% | +47.1% |

Three independent fake zods resume the same saved checkpoint. Dojo's `%bout` hint times the Hoon call. The check outside the timed expression cues the result and compares it against the whole input. A changing prefix gives every trial a distinct noun. The shared-record case reuses one record; the repeated-record case constructs pairs from the counter modulo 64 and 31. The small-message workload performs 20,000 individual jam calls and checks the list of decoded pairs. It runs on one physical P-core with alternating process order. All cases have six measured trials after warm-up. [Dojo expressions](bench-jam-classes.dojo)

## Compiler, replay and memory

| Workload | Upstream develop | Branch | Candidate | Less vs branch | Less vs develop |
|---|---:|---:|---:|---:|---:|
| Hoon compile (s) | 23.11 | 16.93 | 17.31 | -2.2% | +25.1% |
| Prelude compile (s) | 24.20 | 17.41 | 17.66 | -1.4% | +27.0% |
| Checkpoint replay, first series (s) | 11.97 | 11.52 | 12.60 | -9.4% | -5.3% |
| Checkpoint replay, second series (s) | 8.36 | 8.32 | 7.98 | +4.1% | +4.6% |
| Cold replay, one observation (s) | 439.84 | 337.32 | 332.82 | +1.3% | +24.3% |
| Resumed ship PSS (MiB) | 274.6 | 276.1 | 275.6 | +0.2% | -0.4% |
| Worker peak RSS (MiB) | 228.5 | 229.9 | 229.7 | +0.1% | -0.5% |

The compiler medians rise 2.2% on Hoon and 1.4% on Prelude against the branch. The compiler driver compares complete Hoon and Prelude products with golden jams. Its timer measures compilation and excludes encoding the returned product. Each checkpoint series has six fresh copies per build, with rotating cores. The second series starts after the compiler trials finish. The ratio of medians changes sign between series. Within simultaneous pairs the median time change is 0.4% slower in the first series and 0.1% faster in the second. This fixture shows no repeatable replay gain. The cold replay starts from the event log, bootstraps through event 5, processes events 6–143 and saves. There is one cold timing per build, which limits the precision of that row. A separate instrumented replay checks the full effects and core after every event. [Compiler workload](bench-develop-compiler.py) · [Replay workload](bench-develop-replay.py)

The live memory readings cover one resumed session per build after the same 20,000-message trials and a large pair-list encode/decode. PSS sums the king and worker after two seconds of idle time. Peak RSS is the worker's Linux high-water mark since startup.

## Correctness and reproduction

- All 143 corpus records reproduce their original canonical bytes and bit length through both encoders on 32- and 64-bit looms.
- The independent `ur` encoder checks the output on generated streams, including 512 seeded trees. Focused inputs include two separately allocated 72-level shared diamonds, a 4,096-cell spine, two unequal heap atoms with the same mug, senior data on a child road, and a deferred home-road gain window. The checked builds pass the noun tests and match upstream's 10,514-case Nock product/exit trace on both loom widths.
- The audited bootstrap core, all 138 cold replay products, and all 17 checkpoint replay products match upstream's canonical jam lengths and SHA-256 hashes. These comparisons cover 33,279,069,693 encoded bytes. Upstream also reopens a checkpoint written by the candidate, reads the saved jam of the 80,000-pair list, and reproduces the whole atom.
- From revision `9475dc5bb0`, build the comparison with `tests/bench-develop-build.sh "$bench/compare" 697fc95ddd 9475dc5bb0 CUE_VIEW JAM_CLASSES`. Build the `jam-bench` target with the same flags for each configuration. To build that target at either control revision, copy the candidate's `pkg/noun/jam_bench.c` and its target declaration into a source archive of that revision. The C driver accepts either `fib` or `xeno` followed by one or more canonical input files. The Python driver rotates the cores and records the per-record measurements. Raw event jams are the LMDB event values after their four-byte mug prefix. [Event corpus](nock-cue.md) · [Benchmark driver](bench-jam-classes.py)

```sh
python3 tests/bench-jam-classes.py "$develop_jam" "$branch_jam" "$candidate_jam" \
  event-*.jam --cores 2 6 10 --rounds 6
```
