# Compiling Nock case dispatch

## Vere 4.6

Stock is the unmodified `vere-v4.6` tag at `8ddc4b786979574dbfcb655e3db1b634f658d0de`. The candidate applies the Nock compiler and VM changes to the same tag and enables `U3_DECISION_BENCH`. Both use Zig 0.15.2, ReleaseFast, x86-64 Linux musl and 32-bit loom words. Each pair runs concurrently on two physical performance cores of one i7-12700K. Stock and candidate exchange cores every round. Times are wall-clock means across four pairs on a shared host.

| Workload | Stock 4.6 | Indexed 4.6 | Change |
|---|---:|---:|---:|
| Full replay from the event log through event 143 | 436.15 s | 377.16 s | 13.5% less time |
| Replay from the event-34 checkpoint through event 143 | 0.635 s | 0.535 s | 15.7% less time |
| Fresh fakezod to first Dojo prompt | 66.79 s | 66.02 s | 1.2% less time |

The full replay starts from a frozen copy of a new `urbit -F zod` event log. It bootstraps from events 1–5, then processes events 6–143, including the Clay kernel update and `%base` rebuild. Each timed run starts with only the event log and ends after the final checkpoint. Timings exclude the log copy. The event-34 checkpoints come from each build replaying a copy of that same log whose final event is 34. The released replay loop clears its bytecode cache before saving. Each resumed trial includes loading the checkpoint and saving again after the 109 remaining events. Those events are startup traffic from one fake ship.

Fresh boot uses the official `urbit-v4.6.pill` from a local file and records time from process launch to the first `~zod:dojo>` prompt. Each process gets separate Ames and HTTP ports. The `%cash` pill supplies memo entries during boot. Full log replay passes an empty memo list to the bootstrap routine. These paths exercise different amounts of Nock compilation.

| Full replay pair | Stock seconds | Indexed seconds | Peak RSS in KiB, stock / indexed |
|---:|---:|---:|---:|
| 1 | 435.35 | 370.88 | 1,189,680 / 1,189,896 |
| 2 | 442.86 | 383.80 | 1,189,680 / 1,189,896 |
| 3 | 445.64 | 387.65 | 1,189,676 / 1,189,896 |
| 4 | 420.76 | 366.33 | 1,189,680 / 1,189,788 |

The four within-pair elapsed-time reductions are 14.8%, 13.3%, 13.0%, 12.9%. Whole-replay CPU assignments alternate between logical CPUs 4 and 6, restored replay between 0 and 2, and fresh boot between 12 and 14. Peak RSS comes from the timed replay process. All state hashing and dispatch counting use separate binaries.

The first four pairs each reported completion through event 143 before the driver printed a shell error at teardown. The driver file changed while Bash was executing it. A fifth pair uses a fixed copy, with the other benchmark jobs finished, and exits successfully. Stock takes 419.59 s and the candidate takes 356.53 s (15.0% less time). The four-pair mean above excludes this confirmation run.

### Contribution of the hash index

A third build keeps case factoring, branch threading, slot traversal, and the stack optimizations. Its one-line change disables emission of the hash index so every factored case uses linear comparisons. In two paired full replays on CPUs 12 and 14, the linear build takes 396.00 s and 388.35 s, compared with 381.70 s and 376.37 s for the indexed build. Their means are 392.18 s and 379.03 s (3.4% less time with the index).

Each microbenchmark sample warms the program and times 500,000 checked calls, cycling through every tag and the default. The table shows median ns/call over ten samples per build on CPU 14. The two groups of five run in opposite build order.

| Case arms | Stock 4.6 ns/call | Indexed 4.6 ns/call | Speedup |
|---:|---:|---:|---:|
| 4 | 50.40 | 18.30 | 2.8× |
| 8 | 90.55 | 22.60 | 4.0× |
| 16 | 154.60 | 19.45 | 7.9× |
| 32 | 284.70 | 21.65 | 13.2× |
| 64 | 535.60 | 22.75 | 23.5× |
| 128 | 1,024.05 | 24.10 | 42.5× |

### Correctness on 4.6

The bootstrap core at event 5 and all 138 event products have identical canonical jam lengths and SHA-256 digests. Each product contains both emitted effects and the new core. The products span 29,203,773,406 bytes. Stock also replays the candidate's event-34 checkpoint, and the candidate replays stock's. Both match the full stock audit at all 109 subsequent events.

| Audit item | SHA-256 |
|---|---|
| Frozen epoch-0 event database | `f617355f37973ddde851c2fb116b43e243d7ed1708b6862c87e2a82bf5ef332a` |
| Official 4.6 bootstrap pill | `f8b08c53dd6d0aa9c4f7654f0a7bcd11e94089222888a3c99c0a10eedcbb377b` |
| Bootstrap core | `cff05e565a7bf26e66827aa1ad085d51ad829541d76c1743154db9985765f0b4` |
| Final event product | `2e7157cf83cf4456ca7d97fe68df4372f636208269668ccd767a8205ee47df9c` |
| Differential transcript | `a82add33102219e7c59e58b22b00a91bed1f1d060704bb3f2db9fbe015c68069` |

The reference evaluator covers 7,575 cases, yielding 3,061 products and 4,514 expected exits. The stock and indexed transcripts agree byte for byte. The candidate passes the same cases with memory debugging and reference-count checks. The indexed full audit records 146 constructed tables, 185,187,225 matching lookups and 32,799,896 default lookups.

### Live Dojo benchmarks on 4.6

Two running fakezods use the stock and indexed ReleaseFast binaries and the official 4.6 pill. The Hoon timing hint `~>(%bout expression)` measures evaluation inside the worker. Boot, typing, parsing, Dojo output and command round-trip latency are outside the timed expression. The definitions and sample calls can be entered one line at a time. [Dojo commands](bench-nock-live.dojo)

| Live workload | Stock median, ms | Indexed median, ms |
|---|---:|---:|
| Counter, 2,000,000 steps | 138.62 | 149.27 |
| 16-way dispatch, 1,000,000 iterations | 480.03 | 419.72 |
| Build and sum 250,000 list elements | 103.09 | 97.91 |
| Sum the range directly | 82.52 | 78.61 |
| Map, then fold a prepared list of 120,000 elements | 53.42 | 52.88 |
| Map and fold in one traversal | 49.38 | 49.20 |
| Counter, one step per call | 138.44 | 148.99 |
| Counter, eight steps per call | 24.83 | 26.10 |
| 16-way dispatch, one step per call | 472.21 | 418.79 |
| 16-way dispatch, four steps per call | 438.15 | 383.30 |

Each ship warms each workload once, then contributes eight measured calls per row. Both ships run together on physical P-cores, exchanging logical CPUs 4 and 6 every pair. The workload order reverses every round. The inputs vary per round and every displayed noun is checked against an independently calculated integer result. The separate loop comparisons vary the trip count by up to eight and include the leftover iterations. Additional checks exercise zero, short loops, exact block boundaries and partial final blocks. The list for map/fold is prepared before timing. Its mapped values are consumed in input order in both variants. The range sum traverses the integers in opposite orders across its two implementations and relies on the commutativity of atom addition.

In the paired loop experiment, manually unrolling the counter eight steps at a time takes the indexed VM from 148.99 ms to 26.10 ms (5.71×). Unrolling the 16-way loop four steps at a time takes it from 418.79 ms to 383.30 ms (8.5% less time). Both source variants use the same evaluator. Each dispatch iteration still computes a remainder, calls the selection gate and accumulates its result. The ordinary counter takes longer on the patched evaluator in every pair.

An independent sample of a complete patched replay through event 143 records 70,386 instruction pointers at randomized intervals of 3–7 ms. Of these, 16,883 (24.0%) land in `u3a_gain`, `u3a_lose`, `_me_lose_north` or `_me_lose_south`. Another 11,127 (15.8%) land in the slot walkers `_n_path3` or `u3r_at`. These are exclusive instruction-pointer samples. Some ownership work and slot traversal is necessary even after specialization. The compiler can also retain the changing fields of a self-tail call between iterations. The separate experiment measures this on running ships and compares event products with stock 4.6. [Compiled tail loops](nock-tail.md)

The ownership experiments measure how many retains and releases can be eliminated when a gate reads or edits fields of its subject. They compare borrowed projections, keeping an existing subject reference and reusing uniquely owned edit targets. [Ownership measurements](nock-ownership.md)

## Compiler and VM changes

Hoon tag matches often compile to a run of Nock 6 arms. Each arm tests whether a slot holds a cell, reads a tag from that cell, and compares it with a literal. On the zod-v1.21 replay, a sample of 22,548 instruction pointers in stock Vere placed 3,984 in `u3r_at`, including 3,531 with `_n_burn` as the caller. The sampler interrupts the running child at randomized intervals of roughly 3–7 ms and resolves its instruction and caller addresses against the binary. A separate sampled opcode trace finds that the two most frequently sampled bytecode programs repeatedly test axis 6 and compare the value at axis 12.

With `U3_DECISION_BENCH`, the compiler recognizes a run of at least three comparisons on the same positive slot with an identical optional shape guard. It evaluates the guard once and keeps the selected tag above the subject until dispatch finishes. A failed guard jumps to the default before the tag slot is read. On any selected path the arm receives the original subject and operand stack.

For four through 128 arms with 31-bit immediate keys in a program of at most 65,535 instructions, the assembler builds an open-addressed index using the final bytecode offsets. It checks each linear comparison, conditional jump and destination before building the index. A lookup compares the complete key and keeps the earliest arm for repeated keys. The index holds no noun pointers. It lives in the compiled program's literal pool and goes through the ordinary program-copy, checkpoint and collection paths.

Larger programs retain all 65,536 literal slots for their ordinary instructions and use the linear comparisons.

The compiler retains executable linear bytecode for the entire decision. A compiler-private literal hint marks the index. Its ordinary behavior, together with three stack instructions, restores the original stack and enters the linear comparisons. This gives the stock VM a way to execute a checkpoint containing an indexed program. The compiler's public Nock hint whitelist controls entry to this opcode.

There are three other savings in the timed build. The compiler threads forward branches that push a known boolean immediately before a conditional. The VM can use a selected noun while its subject still owns it and can consume a boolean directly at a short conditional jump. It also walks short axes three directions at a time and forwards the children of a freshly constructed pair when the following instructions immediately take it apart. Indirect axes use the ordinary fragment implementation. The experiment uses the existing VM and jets and does not require a native code generator.

## Reproduce the development-checkout measurements

These commands use Zig 0.15.2, the Linux x86-64 target and the real Git LFS fixture archives in `tests/fixtures`.

```sh
bench=$(mktemp -d)
zig build -Doptimize=ReleaseFast -p "$bench/stock"
zig build -Doptimize=ReleaseFast -Dcopt=-DU3_DECISION_BENCH -p "$bench/candidate"
zig build -Doptimize=ReleaseFast -Dcopt=-DU3_REPLAY_AUDIT -p "$bench/audit-stock"
zig build -Doptimize=ReleaseFast -Dcopt=-DU3_DECISION_BENCH -Dcopt=-DU3_REPLAY_AUDIT -Dcopt=-DU3_DECISION_STATS -p "$bench/audit-candidate"

bin=x86_64-linux-musl/urbit
for version in 1.21 2.12; do
  BENCH_CPU=2 tests/bench-decisions.sh time "$bench/stock/$bin" "$bench/candidate/$bin" "$version" 2
  BENCH_CPU=4 BENCH_PEER_CPU=6 tests/bench-decisions.sh cross "$bench/stock/$bin" "$bench/candidate/$bin" "$version" 2
  tests/bench-decisions.sh audit "$bench/audit-stock/$bin" "$bench/audit-candidate/$bin" "$version"
done

zig build decision-test -Doptimize=ReleaseFast -p "$bench/stock"
zig build decision-test nock-test -Doptimize=ReleaseFast -Dcopt=-DU3_DECISION_BENCH -p "$bench/candidate"
zig build decision-test nock-test -Doptimize=ReleaseFast -Dvere64=true -Dcopt=-DU3_DECISION_BENCH
U3_CASE_GC=1 zig build decision-test -Doptimize=ReleaseFast -Dmem-dbg=true -Dcopt=-DU3_DECISION_BENCH
U3_CASE_BENCH=1 taskset -c 2 "$bench/stock/bin/decision-test"
U3_CASE_BENCH=1 taskset -c 2 "$bench/candidate/bin/decision-test"
```

The replay driver extracts only the event log, starts a new process for every sample and alternates the execution order in each successive pair. Its timer includes bootstrap, compilation, event execution and the final snapshot. The audit hashes the canonical jam of the entire `[effects new-core]` product immediately after each successful replay poke. The comparison requires contiguous event numbers, matching byte lengths and SHA-256 digests, and the expected end event. A separate digest covers the installed bootstrap core before the first poke. Cross mode starts a stock and candidate replay together and exchanges their CPU affinities on the second round.

The differential test interprets generated formulas using the Nock rules for its terminating subset and compares the result or expected exit with compiled execution. Successful cases exercise cold and warm execution plus a nested road with the opposite stack direction. Targeted cases cover a completely occupied 65,536-entry literal pool, a winning arm’s bail trace, memo hits, duplicate and colliding tags, indirect keys and axes, shape-guard failure, invalid slots, non-boolean conditions, unused branches, large branch displacements, and the observable slog from a selected arm. `U3_CASE_TRACE=1` prints a deterministic transcript of input and result mugs for comparisons across builds.

## Measurements on the development checkout

The host is an Intel Core i7-12700K running Linux. The binaries use Zig 0.15.2 ReleaseFast and the same checkout at `697fc95ddd` plus the experiment. Timed replays use 32-bit nouns. Verification and timing use separate builds.

### Case dispatch

Each trial warms a compiled formula, then evaluates 500,000 checked calls on a child road. Subjects cycle through all tags and one default. The table shows median ns/call across ten samples on performance-core logical CPU 12. The three binaries run in forward and reverse order. The middle column uses the factoring, borrowing and stack optimizations with linear dispatch.

| Arms | Stock | Factored, linear | Indexed |
|---:|---:|---:|---:|
| 4 | 42.7 | 23.1 | 17.4 |
| 8 | 75.4 | 28.3 | 20.3 |
| 16 | 125.2 | 35.4 | 18.8 |
| 32 | 224.1 | 64.0 | 21.4 |
| 64 | 413.1 | 109.5 | 22.4 |
| 128 | 787.1 | 193.8 | 22.9 |

A control pins every call to the first arm. At 128 arms on efficiency-core logical CPU 18, stock takes 35.4 ns and indexed takes 31.3 ns. Sending every call to the default on the same core takes 2,752.9 ns and 34.3 ns respectively. These figures are medians of five samples. Set `U3_CASE_BENCH=first` or `miss` to select those inputs.

### Full replay time

The two crossover rounds use performance-core logical CPUs 4 and 6 for zod-v1.21 and 8 and 14 for zod-v2.12. Stock and candidate run concurrently and exchange cores in round two. Both fixtures run during the same window on a shared host. Wall time includes writing the final snapshot.

| Fixture | Round | Stock seconds | Indexed seconds | Peak RSS in KiB, stock / indexed |
|---|---:|---:|---:|---:|
| zod-v1.21 | 1 | 120.37 | 107.58 | 830,124 / 830,336 |
| zod-v1.21 | 2 | 118.93 | 107.12 | 830,124 / 830,272 |
| zod-v2.12 | 1 | 116.29 | 103.08 | 710,104 / 709,920 |
| zod-v2.12 | 2 | 114.07 | 102.99 | 710,040 / 709,916 |

Mean elapsed time falls 10.3% on zod-v1.21 and 10.5% on zod-v2.12. Peak RSS differs by at most 212 KiB within these pairs. The aggregate replay savings include the compiler and VM changes together. The dispatch microbenchmark isolates the lookup more directly.

One additional sequential pair on CPU 12 takes 117.04 s stock and 100.38 s indexed for zod-v1.21. On CPU 0, zod-v2.12 takes 116.52 s and 98.14 s. Other host jobs finish between these stock and candidate runs, so the simultaneous crossover supplies the headline estimate.

### Replay verification

| Fixture | Bootstrap through | Post-bootstrap products | Canonical product bytes compared | Final product SHA-256 |
|---|---:|---:|---:|---|
| zod-v1.21 | 4 | 68 | 4,278,084,711 | `5a5d470756a287f32a28fe8b1f5fdebe1d68e004899adecad0c04d2efb214588` |
| zod-v2.12 | 3 | 67 | 5,932,958,683 | `d9f7ebd90af83cacb0bf120173a3de5bc7e3fb529eca2b87e54f510e61681c9d` |

The bootstrap digests match stock on both fixtures. The 64-bit evaluator also matches the 32-bit stock digest for each of the 68 zod-v1.21 pokes. Booting each replayed pier with the experimental binary produces the repository’s normalized golden state mugs of `1.875.565.524` and `1.222.863.659`.

The zod-v1.21 audit records 127 compiled tables, 67,026,757 matching lookups and 9,716,058 default lookups. The zod-v2.12 audit records 131 tables, 67,727,239 matches and 10,050,132 defaults. These counters include initialization as well as replay.

The differential suite compares 7,575 inputs against the reference evaluator, including 3,061 products and 4,514 exits. The stock 32-bit, experimental 32-bit and experimental 64-bit transcripts agree byte for byte. Their SHA-256 is `a82add33102219e7c59e58b22b00a91bed1f1d060704bb3f2db9fbe015c68069`. The same cases pass with the memory-debug build and runtime reference-count checks. A separate checkpoint test runs the indexed VM through event 15 of zod-v1.21, restarts with the stock evaluator, and matches all 57 remaining event products.

The correctness scope covers noun products, ordinary exits, the selected slog and trace cases, bootstrap cores and replay effects. Bytecode inspection and timing hints expose implementation details. Resource-exhaustion boundaries can move when the compiler or VM allocates a different number of words. The replay logs cover two kernel-update workloads on this host.
