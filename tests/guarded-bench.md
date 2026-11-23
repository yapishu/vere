# Guarded Nock product benchmark

The experiment caches one computed field of a cell and fills the other field from the current subject. The compiler discovers eligible formulas in the autocons path. A slot analysis finds a common ancestor for every input read by the computed field. A hit requires that the ancestor be the same live noun and the hole axis be valid. The road's transient memo table holds the most recent entry per recipe. Compilation and execution use existing hint bytecodes. The experiment is compile-time opt-in.

The analysis handles slots, literals, pairs, shape, increment, equality, conditional, composition, and a statically closed form of recursive Nock 2. Its budget is 256 visited formula nodes. Opaque calls, jets, scries, and hints end analysis of that field. The experiment measures same-road reuse. Shadow mode reconstructs each predicted product, runs the ordinary code, and checks noun equality. The changed counter compares the current hole with the hole at the last cache fill.

## Reproduce on Linux

Build stock and experimental binaries from the same checkout with Zig 0.15.2. The example paths below use Vere's default x86-64 Linux build target.

```sh
bench=$(mktemp -d)
zig build -Doptimize=ReleaseFast -p "$bench/stock"
zig build -Doptimize=ReleaseFast -Dcopt=-DU3_GUARDED_BENCH -p "$bench/guard"
zig build guarded-bench -Doptimize=ReleaseFast -p "$bench/stock"
U3_GUARD=use zig build guarded-bench -Doptimize=ReleaseFast -Dcopt=-DU3_GUARDED_BENCH -p "$bench/guard"

"$bench/stock/bin/guarded-bench"
U3_GUARD=shadow "$bench/guard/bin/guarded-bench"
U3_GUARD=use "$bench/guard/bin/guarded-bench"

tests/bench-guarded-replay.sh "$bench/stock/x86_64-linux-musl/urbit" "$bench/guard/x86_64-linux-musl/urbit" 2
```

The replay driver extracts the fixture's event log into a temporary directory and uses a fresh pier for each run. Set `BENCH_CPU` to pin every replay to an available CPU. Set `U3_GUARD_MIN_NODES=8` to test a higher admission threshold, or `U3_MEASURE=1` to collect protected-checkpoint statistics through event 71. Each series starts with a shadow replay. Stock and enabled runs alternate order across rounds.

## Measurements

These measurements use commit `697fc95ddd` plus the benchmark, 32-bit loom words, ReleaseFast, and an Intel Core i7-12700K on a shared Linux host. Each microbenchmark sample starts on a fresh road and compiles and warms the formula before timing. The timed loop constructs the subject, evaluates the formula, checks the answer and releases it. Each entry is the median of five samples in ns/call. The recursive count walks a 128-cell list using Nock 2. The alternating case switches between that list and a 129-cell list. The cheap case increments twice with a fresh input on every call.

| Input sequence | Stock | Whole-product %memo | %memo on computed field after projection | Guarded |
|---|---:|---:|---:|---:|
| Shared list, changing envelope | 8,077 | 8,247 | 88.8 | 31.5 |
| Same list and envelope | 7,993 | 110.3 | 87.4 | 31.2 |
| Alternating lists, changing envelope | 8,031 | 8,214 | 92.4 | 7,998 |
| Cheap computation, changing input | 18.4 | 213.0 | 189.0 | 67.7 |

The projected memo can keep both alternating keys. The guarded prototype has one entry for the formula and misses on every alternation. The cheap case measures the cost of repeatedly admitting an unprofitable formula. Separate checks cover a discarded increment that bails, a missing hole, and a hole in the head of a nested result.

### Event replay

The zod-v1.21 fixture contains four bootstrap events followed by 68 replay events, including a kernel update, a Clay rebuild and Gall app installs. Fresh log copies are replayed with jets and existing memo hints active. Both stock and the three-node guarded build reach event 72 and produce the committed normalized Arvo state mug `1.875.565.524` when booted with the fixture's lite-boot procedure.

| Shadow replay at the three-node threshold | Count |
|---|---:|
| Recipes emitted during compilation | 1,240 |
| Guard probes | 33,437,982 |
| Predictions checked against ordinary bytecode | 3,389,551 |
| Predictions with a different product than the cache fill | 707,618 |
| Mismatched products or predictions that bailed | 0 |

The last count follows from equality checks passing and every predicted hit returning to the check. The 707,618 changed products represent 2.12% of probes. In enabled mode hits can bypass nested probes, so the enabled and shadow totals differ.

In a separate protected replay through event 71, stock and guarded each write 3,060 pages at the bootstrap checkpoint. The event-71 checkpoint writes 19,076 pages for stock and 19,061 for guarded. The home-road bytecode cache at that checkpoint uses 3,000,168 and 3,015,187 bytes respectively. Peak transient noun retention during a poke is unmeasured.

| Timed full replay | Stock seconds | Guarded seconds | Peak RSS, stock / guarded (KiB) |
|---|---:|---:|---:|
| Three-node minimum, pair 1 | 113.98 | 118.94 | 829,932 / 832,284 |
| Three-node minimum, pair 2 | 113.20 | 120.59 | 830,020 / 832,248 |
| Eight-node minimum, exploratory pair | 114.05 | 111.01 | 830,120 / 832,168 |

Each pair uses the same logical CPU and separate processes. The broad runs take 5.4% longer on average. The eight-node threshold reduces enabled probes from 33,429,516 to 216,034 and hits from 3,380,959 to 4,467. Its shadow run checks 4,684 predictions, including 3,561 changed products. Its single timing pair on a shared host is inconclusive. The synthetic case and shadow checks establish that a changing product can reuse work. The full replay charges the table lookups, reference counts, temporary allocations, and misses needed to find that work.
