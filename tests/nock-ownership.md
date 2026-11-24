# Nock operand ownership on Vere 4.6

Keeping the original subject and editing uniquely owned cells in place cuts a live two-million-step counter from 150.76 to 138.33 ms on a performance core. Cold replay takes 2.2% less time across two separate-build pairs, 2.9% less across two same-executable pairs and 4.7% less in a further same-executable pair using disk-backed scratch. Borrowing fields through private bytecode plans reduces reference-count traffic but adds 2.4% to mean cold replay time.

In the indexed 4.6 replay profile, 24.0% of sampled instruction pointers fall in four retain and release functions. Recursive destruction accounts for a substantial share. Sampling the bytecode separately shows an opportunity before gate calls. The VM selects a gate from its subject, reads arguments from that same subject and replaces the gate's sample. The subject already owns the selected gate throughout the reads. [CPU profile](nock-decisions.md)

Of 180,143 sampled projection entries, 15,205 begin `COPY SWAP FABL MUSM KICB`. The VM duplicates the subject, selects a field, installs it as the sample and calls the gate. These counts describe the bytecode found at sampled entries. The separate CPU profile measures time.

The next experiment specializes short slot reads and fixed-axis edits on the keep-and-reuse build. [Compiler and live ship results](nock-paths.md)

## Three implementations

**Borrow selected fields.** With `U3_DECISION_BENCH` and `U3_BORROW_BENCH`, the compiler recognizes two-slot equality, a pair read from a projected subject, and edits at axes 2, 3 and 6 with a replacement consisting of one slot or a pair of slots. The axes fit in 16 bits. The VM keeps the original subject alive through the reads. It takes references for fields that escape into the product. When equality compares distinct heap nouns, it pins both operands across Vere's unifying equality routine. The compiler emits ordinary bytecode for the whole region. A private hint holds the plan and its continuation. Calls, hints and branches inside the expression exclude it from this transformation.

**Keep the original subject.** With `U3_OWNER_BENCH`, if the left operand is exactly `[0 1]` and the operation consumes its subject, the compiler evaluates the right operand in the VM's existing keep mode. That leaves the original subject directly beneath the right result. This applies to cell construction, dynamic Nock evaluation, equality, edits and scries. The right operand may call a gate, emit a hint or fail. A successful evaluation arrives at the consuming instruction with the required two owned operands. The bytecode uses ordinary 4.6 instructions.

**Reuse a uniquely owned edit target.** With `U3_REUSE_BENCH`, the fixed-axis edit handlers ask the allocator whether the target cell is mutable on the current road. On success, the handlers use `u3i_edit`, Vere's existing copy-on-write operation. It clears cached mugs on the edited path and checks each child independently. A uniquely owned core can still have a shared payload. The existing fixed-axis construction handles shared roots. The measurements combine reuse with keeping the original subject.

## Workloads

All performance baselines enable the indexed compiler on release tag `vere-v4.6` (`8ddc4b786979574dbfcb655e3db1b634f658d0de`). Builds use Zig 0.15.2, ReleaseFast, x86-64 Linux musl and a 32-bit loom. The mixed-tag and tail-loop experiments are disabled in the timing binaries.

For live measurements, each worker boots a separate fakezod from the official 4.6 pill. A prepared list contains 262,144 pairs, half equal, including small atoms and cells. Each fold adds 7 for equality and 11 otherwise. One fold calls a gate, another performs the comparison inline, and a third calls a swap gate before the scoring gate. A counter updates its sample two million times. Each workload runs once to warm the ship, then twelve times per build. The accumulator changes every round. The paired workers exchange cores and reverse workload order. Each answer is checked against an independently calculated integer. Times come from the worker's `%bout` hint. [Dojo expressions](bench-nock-borrow.dojo)

The cold replay starts from the frozen fakezod event log, bootstraps through event 5, replays events 6–143 and includes the final checkpoint. Paired runs execute simultaneously on distinct physical performance cores of a shared i7-12700K, then exchange cores. Allocation counters and canonical product hashing run in separate processes from the timing trials.

## Live Dojo timings

Each cell gives the median baseline and candidate time in milliseconds and the elapsed-time change. The keep-and-reuse columns toggle both changes inside one executable.

| Workload | Borrow selected fields | Keep the original subject | Keep and reuse, P-cores | Keep and reuse, E-cores |
|---|---:|---:|---:|---:|
| Call scoring gate | 88.88 → 88.87 (−0.0%) | 94.05 → 96.24 (+2.3%) | 88.61 → 86.66 (−2.2%) | 138.81 → 133.44 (−3.9%) |
| Inline equality | 67.78 → 68.60 (+1.2%) | 72.91 → 71.64 (−1.7%) | 66.47 → 63.79 (−4.0%) | 101.69 → 94.62 (−7.0%) |
| Swap and score | 110.03 → 110.21 (+0.2%) | 113.03 → 115.15 (+1.9%) | 106.83 → 106.79 (−0.0%) | 169.01 → 164.14 (−2.9%) |
| Counter | 163.54 → 163.39 (−0.1%) | 163.57 → 153.46 (−6.2%) | 150.76 → 138.33 (−8.2%) | 256.42 → 236.70 (−7.7%) |

## Cold replay timings

| Comparison | Paired trials | Baseline mean | Candidate mean | Elapsed-time change | Within-pair changes |
|---|---:|---:|---:|---:|---|
| Borrow selected fields | 4 | 432.82 s | 443.09 s | +2.4% | −1.1%, +5.5%, +0.4%, +4.4% |
| Keep the original subject | 2 | 436.67 s | 440.62 s | +0.9% | −1.4%, +3.2% |
| Keep the original subject, same executable | 2 | 386.61 s | 381.06 s | −1.4% | −1.6%, −1.3% |
| Keep and reuse | 2 | 405.08 s | 396.24 s | −2.2% | −4.8%, +0.7% |
| Keep and reuse, same executable | 2 | 400.04 s | 388.62 s | −2.9% | −1.0%, −4.8% |
| Keep and reuse, same executable, after cleanup | 1 | 386.59 s | 368.35 s | −4.7% | −4.7% |

The two separate-build pairs and two same-executable pairs exchange physical cores. The shared machine also runs other benchmark processes, and temporary files in RAM are removed during some of the second pairs. The final trial starts after that cleanup, uses disk-backed scratch and keeps both settings in one executable. The small sample and variation between pairs limit the precision of the startup estimate.

## Urbit benchmark desk

The public benchmark desk supplies fourteen independent subject/formula pairs. The seven Dojo comparisons below use the verification inputs from its manifest. The ship checks the complete expected noun after every call. Each row gives the median time for a batch, with one warm-up and eight measured pairs. The workers exchange performance cores each round. Both use the same indexed Vere 4.6 executable, with subject ownership and cell reuse toggled together. The published text and jam representations decode to identical nouns for all twelve cases supplied in both formats. [Pinned benchmark desk](https://github.com/urbit/benchmark/tree/ae0e498e19beba4ab585299adcbcce0427594e87)

| Workload | Calls per batch | Baseline | Keep and reuse | Elapsed-time change | Faster pairs |
|---|---:|---:|---:|---:|---:|
| ackermann | 1 | 886.92 ms | 663.16 ms | −25.2% | 8/8 |
| add | 2 | 69.18 ms | 50.94 ms | −26.4% | 8/8 |
| dec | 8,000 | 55.50 ms | 43.66 ms | −21.3% | 8/8 |
| factorial | 512 | 117.58 ms | 95.23 ms | −19.0% | 7/8 |
| fibonacci | 256 | 50.95 ms | 42.06 ms | −17.5% | 8/8 |
| bigslot | 64 | 22.57 ms | 17.36 ms | −23.1% | 8/8 |
| mmul | 16 | 20.55 ms | 17.12 ms | −16.7% | 8/8 |

Two additional Dojo inputs check the workload size. At the shipped default of one million, a single decrement returns 999,999 in 63.61 → 49.61 ms (−22.0%). Eight sieve calls at input 8 each return 339, taking 595.18 → 460.70 ms per batch (−22.6%). The seven manifest workloads also match in separate fresh-process evaluations of the shipped jamfiles.

For the two full compiler workloads, each measurement starts a fresh 4.6 process with the embedded ivory pill. Before timing, the harness evaluates `..ride` and verifies structural equality between that compiler core and the one shipped in the jamfile. Timing covers one Nock evaluation, including its bytecode compilation. Input decoding, core preparation and product hashing occur outside the timer. Four paired trials alternate cores and workload order. Each product matches the published mug and the full jam SHA-256 across runs and settings.

| Compiler input | Baseline median | Keep and reuse median | Elapsed-time change | Faster pairs | Product mug |
|---|---:|---:|---:|---:|---:|
| hoon | 21.86 s | 21.58 s | −1.3% | 4/4 | 538,422,470 |
| prelude | 22.57 s | 22.12 s | −2.0% | 3/4 | 2,055,478,196 |

The fourth indexed prelude pair overlaps local builds and takes 40.05 and 45.01 seconds. Including it, the prelude median falls 2.0% and the mean rises 3.1%. Next, a separate comparison builds the released Vere interpreter with a timing hook and a second copy of the same tag with only the standalone ownership patch and the hook. These runs start after the local builds finish. A fresh process runs each input, including for the seven small workloads.

| Workload | Stock 4.6 median | Standalone patch median | Elapsed-time change | Faster pairs |
|---|---:|---:|---:|---:|
| ackermann | 782.931 ms | 554.352 ms | −29.2% | 4/4 |
| add | 29.925 ms | 21.260 ms | −29.0% | 4/4 |
| dec | 0.021 ms | 0.018 ms | −14.5% | 4/4 |
| factorial | 0.233 ms | 0.186 ms | −20.0% | 4/4 |
| fibonacci | 0.205 ms | 0.167 ms | −18.5% | 4/4 |
| bigslot | 0.336 ms | 0.243 ms | −27.8% | 4/4 |
| mmul | 1.229 ms | 0.957 ms | −22.2% | 4/4 |
| hoon | 26.435 s | 26.191 s | −0.9% | 2/4 |
| prelude | 27.201 s | 27.443 s | +0.9% | 0/4 |

Both settings reach the 30-second process limit on `atomcat`, `atomdog`, `atomgmp`, `sha512` and the manifest sieve at input 30. These probes run on two efficiency cores. For example, the atom workloads build powers of two and implement decrement by counting up from zero. The time limits do not supply a speed ratio. All 63 separate Nock reference vectors pass on stock, the standalone patch and both indexed settings, including the four specified exits. The compact report records individual workload results because time limits and batch sizes differ.

## Reference-count traffic

The instrumented allocator counts all entries to `u3a_gain` and `u3a_lose` over each complete cold replay. The normal-heap columns count entries that refer to indirect nouns on the active road. Direct atoms and senior pointers can reach the public functions without writing a count. Recursive releases inside the allocator are outside these entry counters.

| Build | Retain calls | Release calls | Normal-heap retains | Normal-heap releases |
|---|---:|---:|---:|---:|
| Indexed | 18,417,763,833 | 10,402,978,883 | 8,020,818,108 | 7,862,531,420 |
| Borrow selected fields | 17,677,781,340 | 9,426,984,939 | 7,318,017,280 | 6,923,004,674 |
| Keep the original subject | 17,659,161,362 | 9,644,386,513 | 7,262,220,884 | 7,103,937,297 |
| Keep and reuse | 14,496,075,789 | 9,644,064,045 | 6,332,298,960 | 5,892,292,779 |

At the specialized sites, borrowed blocks execute 1,099,126,469 sample edits. The fast paths elide 1,154,849,375 retain/release pairs relative to the indexed instruction sequence, including 1,118,800,605 pairs on heap nouns. Global counters also include compilation and the allocation and lifetime of the plans.

## Isolated evaluation

Each sample evaluates a compiled formula 750,000 times. The first six workloads hold sixteen prepared subjects, check every full result and release it. In the final row, each call consumes the sole reference to its core and returns the core for the next call. The loop checks every increment and both retained fields. The table gives median ns/call over eight samples on one physical P-core. Build order rotates and reverses.

| Workload | Indexed | Borrow selected fields | Keep the original subject | Keep and reuse |
|---|---:|---:|---:|---:|
| Compare two atom fields | 19.81 | 14.68 | 19.21 | 20.13 |
| Compare two cell fields | 22.64 | 30.79 | 23.57 | 23.49 |
| Read a pair from a subsubject | 26.50 | 31.63 | 27.70 | 26.42 |
| Replace a selected tail | 26.77 | 28.71 | 27.57 | 27.25 |
| Install a two-field sample | 66.94 | 64.45 | 70.26 | 74.05 |
| Update a shared core | 50.39 | 50.87 | 44.60 | 44.61 |
| Increment a uniquely owned sample | 23.03 | 23.59 | 19.22 | 16.18 |

## Correctness checks

The independent evaluator and stock 4.6 give the same transcript as the experimental compilers on 10,255 cases, including 5,297 products and 4,958 expected exits. The cases include aliased subtrees, distinct equal cells, an operand reachable through its peer, malformed projections and edit targets, wide slot axes and a full 65,536-entry literal pool. Selected cases check a virtualized scry and an observable `%slog` in a replacement expression. The focused edit tests warm the old hashes, keep aliases to the root or payload and try using the old root itself as its replacement. They check the result, the new hash and every held alias on both road orientations.

The cross-width check includes a 65-bit Nock axis. An odd number of 32-bit limbs fills only half of the last 64-bit word. In the development checkout, `u3i_halfs` clears that word before copying the limbs, so the atom has the same value regardless of the slab's prior contents. The differential test checks its bit length and the transcript matches on both loom widths. The release measurements use the 32-bit constructor.

The cold startup audit compares the bootstrap core and all 138 event products against stock 4.6. Each of the three builds matches the stock jam length and SHA-256 of every record, covering 29,203,773,406 bytes of event products. Feeding the new compiler's bytecode to the released interpreter loop also matches the differential suite. Memory-debug builds on both 32-bit and 64-bit looms pass.

The live checkpoint fixture resumes at event 233. The saved gate returns `1.550.000` after 50,000 calls. Stock, the borrowed-block build and the standalone keep-and-reuse patch match all 17 event products (4,074,710,338 bytes). A separate reader with new plan compilation disabled executes 219,535 restored plans, including 156,188 sample edits.
