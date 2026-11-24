# Compiled paths through a Nock subject

A Nock axis describes a path of heads and tails from the subject. For example, axes 14 and 15 select the two children of axis 7. With `U3_COMPILED_PROJECTIONS`, the compiler can arrange for the VM to reach axis 7 once, test its first child, and read its second child if that branch runs. A sequence of compatible tests can carry the same subtree through several decisions. The optimization applies to Nock formulas independently of their batteries and jets. [Compiler pass](../pkg/noun/nock_projection.inc) · [VM](../pkg/noun/nock.c)

| Workload | Upstream develop | Branch control | Compiled projections | Less time vs. branch |
|---|---:|---:|---:|---:|
| Compile Hoon | 23.23 s | 17.09 s | 17.17 s | -0.4% |
| Compile Prelude | 23.77 s | 17.12 s | 17.21 s | -0.5% |
| Cold replay through event 143 | 443.90 s | 280.68 s | 277.37 s | +1.2% |
| 96 populated group and channel reads | 1.848 s | 1.354 s | 1.460 s | -7.8% |

The compiler rows give medians of six fresh-process trials per workload with builds rotating across physical cores. Each trial checks the complete output jam. The cold replay includes the event-16 desk rebuild and uses two paired runs for the branch and candidate and one upstream run, each starting from a fresh copy of the same log. The paired branch and candidate times are 285.71 → 281.60 s and 275.64 → 273.14 s after exchanging cores. The live row uses the branch control with the same cache rebuild as the candidate. The last part of the exploratory chaining series overlaps a hash audit on one core. [Trial data](nock-compiled-projection-samples.jsonl)

## What sharing saves

During Hoon evaluation the VM reuses a subtree 54,016,120 times and avoids 193,468,222 pointer steps. Prelude avoids 202,812,317 steps. Cold replay, including bootstrap, avoids 2,267,953,769. These counters account for the short path the VM sometimes walks to recover the next common ancestor. Timing builds omit the counters. A separate Hoon profile of the single-instruction prototype observes a cell test followed by its head on 25.4 million executions. Compiling pattern matches to decision trees similarly exploits information about subterms gained along a branch. [Original path profile](nock-gate-return.md) · [Maranget, *Compiling Pattern Matching to Good Decision Trees*](https://www.cs.tufts.edu/~nr/cs257/archive/luc-maranget/jun08.pdf)

The four-byte operand identifies the common axis and the remaining paths of up to two steps for each field. The VM follows a branch before touching its selected field. A successful cell test can supply the head directly. The original subject stays on the VM stack and owns the borrowed subtree. A consuming slot gains its result before releasing the subject. Comparisons against immediate atoms up to 65,535 can participate. Tests against literals in the constant pool use ordinary bytecode. Each replacement adds two or three bytes of bytecode. The pass uses temporary space for up to 65,535 instructions during compilation.

Two small Nock programs isolate the cost of traversal. One selects a payload by tag. The other follows up to four heads through inputs of varying depth. Each round checks four million results over 512 inputs per shape and axis. The figures are median nanoseconds per completed evaluation across eight alternating trials on one physical core.

| Envelope axis | Tag, branch | Tag, candidate | Less time | Nested heads, branch | Nested heads, candidate | Less time |
|---|---:|---:|---:|---:|---:|---:|
| 6 | 12.1 ns | 11.5 ns | 5.3% | 20.4 ns | 12.9 ns | 36.6% |
| 30 | 14.9 ns | 13.4 ns | 10.2% | 22.1 ns | 14.4 ns | 34.9% |
| 510 | 20.7 ns | 17.9 ns | 13.7% | 30.9 ns | 18.3 ns | 40.7% |

## Extending the rule to literals in the constant pool

An alternative also carries the ancestor across equality with a statically known atom in the program's literal pool. It avoids 298,409,553 Hoon pointer steps and 4,866,385,685 cold replay steps. Its two cold replay pairs take 304.52 → 302.78 s and 297.76 → 303.87 s. Across four additional paired compiler rounds, the medians on both desks are within 0.3% of the branch. A separate head-to-head series of four pairs gives median Hoon times of 18.60 s for both and Prelude times of 19.60 s for the narrower rule and 19.27 s for the broader one. The two exploratory replay pairs for the immediate-only rule take 291.78 → 283.52 s and 299.44 → 288.82 s. The selected rule keeps the existing equality path for pooled literals.

## Application reads and memory

Four local fakezods start from copies of one populated checkpoint. Each timed batch serves 96 authenticated JSON responses across two versions of the groups endpoint and the channels endpoint. Each response includes the same three groups or channels and matches the warm-up response in full. This fixture exercises metadata reads without message history. The driver rotates request batches among resident ships on the same core, runs one warm-up and measures eight batches per build.

The new opcodes require a separate bytecode version. On a version change, Vere clears the saved program and jet hank caches. An ordinary branch binary with the same cache invalidation isolates its effect. Boot and the warm-up requests finish before timing starts. The branch takes 1.573 s with the retained cache and 1.354 s with a rebuilt one. Worker CPU time per batch is 1.250 s with the rebuilt branch and 1.350 s with the candidate. Combined king and worker PSS measures 299.6 MiB with its retained cache, 313.7 MiB after the control rebuilds and 311.5 MiB with compiled projections. These memory samples follow each batch in the same process lifetime. Compiler peak RSS is 118.5 → 118.4 MiB for Hoon and 115.8 → 115.7 MiB for Prelude.

## Product checks and reproduction

Both 32- and 64-bit looms reproduce the upstream trace for 10,514 general cases and 4,614 targeted cases. The targeted suite checks retained and consumed fields, overlapping reads, all three branch widths, short and wide literal indexes, indirect atoms, non-atomic literals and missing fields on taken and untaken branches. Successful products run on both road orientations. On a fresh copy of the populated ship, the candidate saves a checkpoint and upstream develop reopens it and serves the same 96 checked responses.

The upstream and candidate 32-bit replay manifests match byte for byte for the bootstrap core, all 138 cold replay events and 17 checkpoint events. This checks canonical jam lengths and SHA-256 hashes across 33,279,069,693 bytes. [Per-event upstream hashes](noun-tree-samples.jsonl)

The branch control is `8e3af88ab0` and the upstream control is `697fc95ddd`. Builds use Zig 0.15.2, ReleaseFast and the x86-64 Linux musl target on a shared i7-12700K. For the comparison at `7670c28786`, add `-Dcopt=-DU3_COMPILED_PROJECTIONS` to the branch flags. Run `U3_PROJECTION_CHECK=1 U3_CASE_TRACE=1 path/to/decision-test` for the focused trace. For the traversal workloads run `U3_PROJECTION_MICRO=1 path/to/decision-test`, adding `U3_PROJECTION_NESTED=1` for the nested heads. `U3_PROJECTION_STATS` enables the phase counters and the focused test's branch-width check. The raw samples include the per-program table, inline pair, whole-formula rebasing and chained-read comparisons. [Branch flags](nock-workload-profile.md) · [Compiler timer](bench-desk-eval.patch) · [Compiler driver](bench-develop-compiler.py)
