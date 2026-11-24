# Indexed jam reads and ordinary gate entry

The control is upstream develop at `697fc95ddd`. The branch control is `097606de80` with the nine cumulative optimization flags. The candidate adds indexed jam projection and ordinary interpreted gate entry at `a03daa887c`. Positive percentages mean less time or memory. Each column reports medians on the same i7-12700K with Zig 0.15.2, ReleaseFast and a 32-bit loom. The comparisons rotate builds across physical P-cores on a shared Linux host. [Branch build and fixture details](develop-benchmarks.md) · [Trial data](field-bench-samples.jsonl)

## Reading fields directly from jam

| Workload | Upstream develop | Optimized branch | Candidate | Less vs branch | Less vs develop |
|---|---:|---:|---:|---:|---:|
| 143 records, two axes (ms) | 1,721.34 | 444.95 | 197.82 | +55.5% | +88.5% |
| Reader peak RSS (MiB) | 1,372.6 | 532.6 | 314.4 | +41.0% | +77.1% |
| Whole event 17 (ms) | 1,973.88 | 404.87 | 606.85 | -49.9% | +69.3% |
| Whole event 17 peak RSS (MiB) | 1,372.6 | 695.4 | 776.7 | -11.7% | +43.4% |

The reader scans the first jammed noun and validates its tags and references. A bitmap ranks atom and cell starts, and a table records the bit offset of each cell's tail. Axis lookup follows the indexed path. An atom needs one payload read. A cell materializes its reachable children and retains sharing through backreferences. A request for the entire root uses sequential cue. The caller can issue more axis queries against the same index. [Implementation](../pkg/noun/cue_view.inc) · [Public API](../pkg/noun/serial.h)

The event benchmark reads axes 2 and 14 of each of 143 raw jobs from a frozen fake ship. On ordinary timestamped ovums these select the timestamp and card tag. The same axes apply to the five bootstrap records. Results encode each selection as a unit so that a missing axis is distinguishable from zero. All three builds agree on the length and SHA-256 of every projected result. The whole-root comparison reads the 198,994,510-byte event 17.

The timing includes index construction and validation for each record, both lookups, and output-list construction. The controls decode the entire noun, select the same axes, and release the root within the interval. File I/O and hashing the output are outside the interval. Peak RSS covers the whole reader process, including ivory boot and all input and output buffers. The event series has six measured rounds after warm-up. The whole-root series has three. Full decode is the cheaper choice on the branch when the only selection is the root.

## Entering an ordinary gate on its fields

| Workload | Upstream develop | Optimized branch | Candidate | Less vs branch | Less vs develop |
|---|---:|---:|---:|---:|---:|
| Dojo, dynamic argument gate (ms) | 40.36 | 33.25 | 30.77 | +7.4% | +23.8% |
| Dojo, captured context gate (ms) | 39.79 | 32.53 | 30.96 | +4.8% | +22.2% |
| Hoon compile (s) | 23.30 | 16.98 | 17.24 | -1.5% | +26.0% |
| Prelude compile (s) | 24.04 | 17.55 | 17.80 | -1.4% | +26.0% |
| Checkpoint replay, 17 events (s) | 11.27 | 10.61 | 11.19 | -5.4% | +0.7% |
| Cold replay, 138 events, single run (s) | 423.98 | 312.74 | 318.92 | -2.0% | +24.8% |

A Hoon gate call can build two cells to install a sample and then immediately take those cells apart in the callee. For a warm interpreted axis-2 kick, the candidate reads the new sample and the template's battery and context separately. At compile time it checks that every path through the supported entry prefix reaches instructions that consume the original subject. At run time it puts the resulting owned operands on the VM stack and resumes the ordinary bytecode after their loads. Jet selection, tracing, uncertain equality, missing projections and calls that need the full subject keep their ordinary execution path. The eligibility flag fits the padding in the bytecode header and is recomputed when restoring a checkpoint. [Gate entry](../pkg/noun/nock_fields.inc) · [VM integration](../pkg/noun/nock.c) · [Jet guards](../pkg/noun/jets.c)

The Dojo tests run inside three separate live fake zods. A higher-order loop calls its gate argument 300,000 to 300,006 times per trial. One gate selects an argument and the other reads a captured value on a branch. Each command checks its complete numerical result. Dojo and compiler timings use six measured rounds after warm-up. All compiler runs compare the complete output jam against the fixture. The checkpoint replay uses six fresh copies per build and rotates cores. The cold replay comparison has one observation per build and serves as a scale check. It cannot establish a repeatable replay speedup. [Dojo expressions](bench-nock-fields.dojo) · [Compiler driver](bench-develop-compiler.py) · [Replay driver](bench-develop-replay.py)

Field-consuming calls save 13–39% in isolation and the two Dojo workloads save 4.8–7.4% against the branch. The end-to-end Hoon and Prelude compiler medians increase by 1.5% and 1.4%, respectively.

A more speculative entry rule allowed a branch to request the entire subject after the entry guard. That prototype took 4.4% more time on Hoon compilation and 4.9% more on Prelude than the branch control. Its samples are recorded under `speculative-compiler`. The gate feature remains opt-in.

### Memory on a running ship

| Workload | Upstream develop | Optimized branch | Candidate | Less vs branch | Less vs develop |
|---|---:|---:|---:|---:|---:|
| Idle ship RSS (MiB) | 266.4 | 267.1 | 268.3 | -0.4% | -0.7% |
| Idle ship PSS (MiB) | 262.3 | 262.5 | 263.1 | -0.2% | -0.3% |

Each of the three fake zods resumes its own saved Dojo checkpoint, evaluates the captured gate with the same new arguments, and idles for four seconds. The table sums the king and worker from Linux smaps_rollup (KiB in the raw samples) and takes the median of three readings a second apart in one session per build. PSS apportions shared pages. This measures a single resumed workload. Upstream develop also resumes the candidate's saved ship and evaluates the gate with its persisted context.

### Isolated call costs

| Workload | Upstream develop | Optimized branch | Candidate | Less vs branch | Less vs develop |
|---|---:|---:|---:|---:|---:|
| Build a sample/context pair, tail (ns) | 78.93 | 85.26 | 67.13 | +21.3% | +15.0% |
| Build a sample/context pair, call (ns) | 110.94 | 114.15 | 92.06 | +19.3% | +17.0% |
| Branch on a sample cell, tail (ns) | 72.08 | 70.19 | 52.97 | +24.5% | +26.5% |
| Branch on a sample cell, call (ns) | 87.78 | 88.20 | 76.62 | +13.1% | +12.7% |
| Branch on sample equality, tail (ns) | 63.50 | 64.60 | 50.93 | +21.2% | +19.8% |
| Branch on sample equality, call (ns) | 89.74 | 89.00 | 72.78 | +18.2% | +18.9% |
| Return sample, tail (ns) | 56.07 | 59.18 | 35.88 | +39.4% | +36.0% |
| Return sample, call (ns) | 79.21 | 83.32 | 58.11 | +30.3% | +26.6% |
| Return context, tail (ns) | 57.63 | 61.09 | 39.17 | +35.9% | +32.0% |
| Return context, call (ns) | 79.27 | 84.97 | 59.47 | +30.0% | +25.0% |
| Return full core on one branch, tail (ns) | 67.72 | 68.66 | 70.25 | -2.3% | -3.7% |
| Return full core on one branch, call (ns) | 95.78 | 97.55 | 96.31 | +1.3% | -0.5% |

Each trial performs 500,000 calls per case, rotates eight inputs and checks every result. A tail call immediately returns its result. The other call returns into a caller that constructs a pair. The escape case exercises a gate that returns the full core on one branch.

## Correctness and reproduction

- The checked 32- and 64-bit builds match upstream's 10,514-case product/exit trace byte for byte. The focused gate cases cover changing batteries, samples and contexts, tail and non-tail calls, an exit after warming the call site, and a logging hint that must fire exactly once per invocation. The registration tests exercise hot, current-road and ancestor-road registrations and sample-dependent fine checks.
- The decoder tests include 512 generated streams with independently constructed nouns, 80 possible backreference targets, sparse stream offsets, missing axes, implicit zero padding and failed handle reuse. A 96-bit axis extracts a 40-level shared diamond and verifies pointer sharing after the index is freed.
- A separate audit build jams and hashes the complete effects and new Arvo core at every event. The trial data includes the per-record projection and replay digest manifests. The bootstrap core, all 138 cold replay products and all 17 checkpoint replay products match the upstream canonical lengths and SHA-256 hashes.
- The command `tests/bench-develop-build.sh "$bench/fields" 697fc95ddd a03daa887c CUE_VIEW GATE_FIELDS` builds the comparison from the pinned revisions. It enables `-Dcopt=-DU3_CUE_VIEW -Dcopt=-DU3_GATE_FIELDS` in addition to the cumulative flags on the candidate. The build targets `serial-test`, `nock-test`, `decision-test` and `tail-test` run the suites. Add `-Dmem-dbg=true -Dcpu-dbg=true -Dcopt=-DU3_FIELD_STATS` for the checked run and `-Dvere64=true` for a 64-bit loom. `U3_FIELD_MICRO=1 path/to/decision-test` runs the isolated call timings on any control built with the same test driver.
- Build `cue-bench` for all three configurations using the same benchmark driver. For upstream, copy the candidate's `pkg/noun/cue_bench.c` and its `cue-bench` test target into a source archive of the upstream revision. Run the projection driver on raw event jams with the four-byte LMDB mug removed. The output contains every record's duration, length and digest. [Corpus and ivory inputs](nock-cue.md) · [Projection driver](bench-cue-projection.py)

```sh
python3 tests/bench-cue-projection.py "$develop_cue" "$optimized_cue" "$view_cue" \
  ivory.jam event-*.jam --cores 2 4 8 --axes 2,14
```
