# Mixed atom and cell dispatch on Vere 4.6

The indexed Nock compiler groups comparisons that use the same slot and shape guard. Hoon type switches also compare a bare atom such as `%noun` with the whole value and compare tags such as `%cell` with the head of a cell. Two arms with this pattern account for 42.9% of sampled self-tail calls in a 4.6 startup replay. [Profile of the two arms](nock-tail.md)

With `U3_DECISION_BENCH` and `U3_MIXED_BENCH`, the compiler partitions a contiguous run of those tests by shape and retains the order within each partition. The bare-atom test must compare a direct literal against a positive slot. The cell test must guard that exact slot and compare its immediate head against a direct literal. The first test always demands the base slot, so selecting it once preserves a failed projection. The cell check makes the head projection safe. The selected arm and default both receive their original subject and operand stack. Tests with hints, other axes, whole-cell literals or different projections end the run.

The compiler emits ordinary bytecode for both paths. For runs of three through 128 rows at axes up to 65,535, it also emits a private literal hint. The assembler reads the final bytecode, checks the two comparison blocks and their jump destinations, and builds one lookup table. A key contains the 31-bit literal and a bit distinguishing an atom from a cell head. Duplicate keys retain their first arm. The table holds immediate keys, the slot axis and byte offsets into its own program. The interpreter borrows the selector from the subject and jumps straight into the selected arm. Stock Vere executes the ordinary bytecode. Programs exceeding the compiler's indexing budget use that fallback.

The mixed lookup takes about half the time of indexed dispatch on a cycle through eight hits and two misses. Live Dojo folds over mixed inputs take 6.0% less time. Full startup replay changes by less than 1% on average in opposite directions across the two comparisons below. The feature stays opt-in.

## Isolated lookup cost

Each input runs through a compiled decision with the two atomic tags and six cell-head tags used by the Hoon benchmark. Every timed pass checks two million answers. These are median nanoseconds per call over eight alternating pairs on one physical P-core. Axes 126 and 510 are the slot axes observed in the two profiled arms.

| Input | Indexed, axis 126 | Mixed, axis 126 | Indexed, axis 510 | Mixed, axis 510 |
|---|---:|---:|---:|---:|
| First bare atom | 13.19 | 15.15 | 14.18 | 16.88 |
| Last matching cell tag | 38.94 | 15.80 | 43.67 | 17.54 |
| Unmatched atom | 30.00 | 13.62 | 32.18 | 15.55 |
| Unmatched cell tag | 38.39 | 14.84 | 43.23 | 16.91 |
| Cycle through all eight hits and both defaults | 36.64 | 17.67 | 40.33 | 19.32 |

## Live Dojo workload

The Hoon gate selects a value at axis 894. Its battery begins by comparing that slot against `%void` and `%noun`, then guards the same slot before testing its head at axis 1,788. The timed expression folds a prepared list, calls the gate once per element and sums the answers. The terminal's `%bout` hint measures evaluation inside each worker. Both ships start from the official 4.6 pill. Each row is the median of twelve paired measurements after one warm-up. Inputs vary per round, the workload order reverses and the workers exchange physical P-cores. All 130 answers, including warm-up, agree with independently calculated totals. [Runnable Dojo commands](bench-nock-mixed.dojo)

| Input distribution | List elements | Indexed median | Mixed median | Elapsed-time change |
|---|---:|---:|---:|---:|
| Two bare hits, six tagged hits, two defaults | 327,680 | 160.03 ms | 150.50 ms | −6.0% |
| Bare atoms, half defaults | 262,144 | 119.44 ms | 118.10 ms | −1.1% |
| Cells, one-quarter defaults | 262,144 | 127.97 ms | 115.93 ms | −9.4% |
| First bare atom only | 262,144 | 113.37 ms | 115.94 ms | +2.3% |
| Two-million-step counter control | — | 161.86 ms | 171.58 ms | +6.0% |

The compiler-only prototype splits the two shapes and uses a separate comparison block on each path. In a separate set of twelve pairs, the mixed-list fold takes 148.07 ms against 145.01 ms on its indexed baseline. Its four cold replay pairs average 421.50 s against 417.68 s (+0.9%). The fused table takes one fragment and performs one lookup for both shapes.

A diagnostic switch also lets two ships run the same executable with the mixed compiler enabled on one and disabled on the other. Each core class has twelve measured pairs. The mixed-list and cell-only gains appear in both core classes. The first-atom loop varies by core class. The unrelated counter's difference shrinks to 1.1% on P-cores and 0.4% on E-cores.

| Workload | P-core disabled | P-core enabled | Change | E-core disabled | E-core enabled | Change |
|---|---:|---:|---:|---:|---:|---:|
| Mixed list | 146.40 ms | 138.88 ms | −5.1% | 222.45 ms | 207.40 ms | −6.8% |
| Cell-only list | 119.02 ms | 111.69 ms | −6.2% | 180.32 ms | 164.31 ms | −8.9% |
| Bare atoms | 112.80 ms | 109.92 ms | −2.6% | 171.65 ms | 167.50 ms | −2.4% |
| First bare atom | 104.84 ms | 108.28 ms | +3.3% | 163.47 ms | 158.24 ms | −3.2% |
| Counter | 162.55 ms | 164.37 ms | +1.1% | 260.68 ms | 261.79 ms | +0.4% |

## Coverage in a startup replay

A cold fakezod replay through event 143 builds 19 mixed lookup tables. They handle 140,194,605 decisions. Of those, 132,418,014 (94.5%) select a cell. A separate instrumented build attributes 52.5% of all mixed lookups to the two arms that motivated the experiment.

| Formula mug | Slot axis | Atom lookups | Cell lookups |
|---|---:|---:|---:|
| 354569318 | 510 | 647,722 | 44,595,365 |
| 551205199 | 126 | 4,670,412 | 23,733,477 |

For scale, a saving of 20 ns at every one of those 140.2 million decisions would amount to 2.8 seconds across a roughly seven-minute replay. The isolated cyclic workload measures a saving of 19–21 ns. Actual tag distributions and memory access costs vary across the ship.

## Full replay timing

Both builds use the indexed Nock changes on `vere-v4.6` (`8ddc4b786979574dbfcb655e3db1b634f658d0de`), Zig 0.15.2, ReleaseFast, x86-64 Linux musl and a 32-bit loom. The frozen event log bootstraps through event 5, then processes events 6–143 from an empty bytecode cache. Each wall time includes the final checkpoint. Each pair runs concurrently on separate physical P-cores of a shared i7-12700K. The builds exchange cores for the second pair in each group. Instrumentation runs separately.

| Comparison | CPUs, baseline / mixed | Baseline | Mixed | Elapsed-time change |
|---|---:|---:|---:|---:|
| Release builds | 4 / 6 | 426.95 s | 429.29 s | +0.5% |
| Release builds | 6 / 4 | 431.65 s | 436.23 s | +1.1% |
| Release builds | 2 / 0 | 438.12 s | 448.87 s | +2.5% |
| Release builds | 0 / 2 | 386.77 s | 382.91 s | -1.0% |
| Same executable | 12 / 14 | 441.31 s | 433.97 s | -1.7% |
| Same executable | 14 / 12 | 384.12 s | 385.11 s | +0.3% |

Across four release-build pairs, mean elapsed time rises from 420.87 s to 424.33 s (+0.8%). Across two pairs using the same executable, mean elapsed time falls from 412.72 s to 409.54 s (-0.8%).

## Correctness

The independent evaluator compares full products and ordinary Nock crashes for 9,152 formulas and subjects, with 4,510 products and 4,642 expected exits. The candidate and stock 4.6 produce identical differential transcripts. Passing the candidate compiler's output to the stock interpreter's bytecode loop gives the same transcript. The suite covers matching and missing tags of both shapes, interleaving, duplicates, keys at the 31-bit boundary, axis and table-size limits, wide branch skips, a full literal pool, and effects and failure traces in selected arms. Successful cases run cold, warm and on a nested road. Memory debugging and reference checks pass on the release tag. The workspace runs the same suite on 32-bit and 64-bit looms, including the existing tail-loop and Nock tests.

Stock 4.6 and the mixed build agree on the canonical jam length and SHA-256 digest of the bootstrap core and each of the 138 startup event products. The products contain 29,203,773,406 bytes of effects and cores.

A second replay resumes from a checkpoint written by the candidate. Its Dojo command builds a 327,680-element list, scores it from 321 and returns `8.585.537`. The restored tables handle 98,706 atom and 234,025 cell lookups with zero tables compiled during replay. Across events 183–200, stock 4.6 and the candidate agree on the canonical jam length and SHA-256 digest of all 18 complete `[effects core]` products (4,314,451,444 bytes).

```sh
zig build decision-test nock-test -Doptimize=ReleaseFast \
  -Dcopt=-DU3_DECISION_BENCH -Dcopt=-DU3_MIXED_BENCH
U3_CASE_GC=1 zig build decision-test nock-test -Dmem-dbg=true -Doptimize=ReleaseFast \
  -Dcopt=-DU3_DECISION_BENCH -Dcopt=-DU3_MIXED_BENCH -Dcopt=-DU3_MIXED_STATS
```
