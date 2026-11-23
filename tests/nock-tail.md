# Scalar replacement in self-tail calls

Vere can evaluate a recursive gate without allocating a new core for each iteration. With `U3_TAIL_BENCH`, the Nock compiler marks an arm whose recursive branch calls itself at axis 2 after replacing one pair in the sample. The interpreter keeps the two fields in C variables for up to 4,096 iterations, then builds one updated core. Each iteration still tests the loop condition and computes both updates from the previous fields.

The recognized test compares one field with a literal or an invariant slot holding a direct atom. An update can select either field or a descendant and apply up to 16 increments. Exit arms are slots or literals. This covers a counter, an already unrolled counter and an ordinary Hoon list traversal. One example counts elements starting at `a`.

```hoon
|=  [l=(list @ud) a=@ud]
^-  @ud
?~  l  a
$(l t.l, a +(a))
```

The normal tail kick resolves first. Batching requires the same compiled program, the same battery, a direct comparison limit, and a site that cannot resolve to a jet anywhere in the current road or its ancestors. The guard also checks the hot jet dashboard and runtime tracing and profiling flags. The matcher accepts only pure slot reads, comparisons, increments and the sample edit inside the loop. The original core owns the borrowed fields while the loop runs. On a failed projection or a bump outside the 31-bit fast path, the VM receives the subject after the last complete iteration and executes the unsupported step normally. The ordinary exit bytecode computes the result or raises its original bail.

The marker uses the standard two-byte operand encoding of a tail kick to call site zero. The existing opcode and call-site layout carry the marker through snapshots. The interpreter rechecks the battery when entering a batch. Unmarked sites keep their existing short instruction.

## Live Vere 4.6 measurements

Both builds use the indexed Nock changes on tag `8ddc4b786979574dbfcb655e3db1b634f658d0de`, Zig 0.15.2, ReleaseFast, x86-64 Linux musl and a 32-bit loom. Each Hoon expression runs in Dojo on two local fakezods. The medians exclude one warm-up and include eight paired calls per build with different accumulator inputs. The workers exchange physical P-cores each round, and workload order reverses. The `%bout` hint measures time inside the worker. Every displayed product is checked independently. The list walk uses a list prepared before timing. [Dojo commands](bench-nock-live.dojo)

| Hoon workload | Indexed | Indexed + tail loops | Result |
|---|---:|---:|---:|
| 2,000,000-step counter | 150.28 ms | 4.83 ms | 31.1× faster |
| Counter unrolled eight steps at a time, including remainder | 26.14 ms | 0.61 ms | 42.7× faster |
| Walk 120,000 elements and increment a count | 8.15 ms | 0.69 ms | 11.8× faster |
| 16-way dispatch with remainder and addition | 431.94 ms | 440.76 ms | 2.0% more time |
| Build and sum a 250,000-element list | 100.18 ms | 102.79 ms | 2.6% more time |
| Sum the range directly | 79.96 ms | 81.90 ms | 2.4% more time |

## Full startup replay on 4.6

The frozen fakezod fixture bootstraps from events 1–5, then replays events 6–143 from the same LMDB log. Each trial starts with an empty bytecode cache and no checkpoint. Timing includes the final save. Each pair runs simultaneously on two physical P-cores of a shared i7-12700K. The builds exchange cores between pairs.

| Pair | Indexed | Indexed + tail loops | Peak RSS in KiB, indexed / tail loops |
|---|---:|---:|---:|
| 1 | 377.52 s | 385.07 s | 1,189,780 / 1,189,896 |
| 2 | 367.19 s | 394.13 s | 1,189,784 / 1,189,896 |

Mean elapsed time rises from 372.36 s to 389.60 s (4.6%). Both pairs favor the indexed build. Two pairs on a shared host give limited precision on the size of the regression.

The instrumented run records 0 accelerated steps in 0 batches. The bootstrap core and all 138 event products match stock 4.6 in canonical jam length and SHA-256 digest.

### Where self-tail calls occur

A separate instrumented indexed replay resolves 270,556,714 interpreted tail kicks. Of those, 86,165,435 target the same compiled program. The probe samples 20,968 of these self-calls at pseudorandom intervals of 2,049–6,144 calls. It records the target axis and a structural print of the formula. These percentages measure call frequency.

Two arms account for 9,002 samples (42.9%). One is at axis 46 (formula mug 354569318, 5,269 samples), the other at axis 4 (mug 551205199, 3,733 samples). Both combine comparisons against the bare atoms `%void` and `%noun` with shape-guarded matches on cell heads such as `%atom`, `%cell` and `%core`. They contain multiple calls and subject edits. The mixed-dispatch experiment compiles both shapes into one lookup and measures the effect on these arms, live Hoon workloads and full replay. [Mixed-tag measurements](nock-mixed.md)

## Correctness

The focused suite checks 283 products and three expected crashes against independently constructed answers. It exercises zero trips, batch boundaries, 31-bit overflow, indirect atoms, malformed lists, borrowed cell results, nested edits and evaluations on both road orientations. It also checks 51 expected hint emissions, a tail kick into a different battery, and the hot, cold and inherited jet-registration guards. A stats build records 1,225,077 fast-path steps in this suite. The CPU-debug build checks the logical kick count across multiple batches.

The indexed VM, the specializing VM and a build that feeds the new compiler's bytecode to the ordinary tail-kick handler pass the focused suite. The candidate passes the 7,575 decision cases, `nock-test` and the memory-debug and reference-count checks. The workspace's 32-bit and 64-bit loom configurations also pass the focused suite.

A second replay starts at a checkpoint that the candidate writes on a running ship. Its event log includes a Dojo call combining the counter, list walk, unrolled counter and a fallback sum. The call returns `[4.000.321 120.321 4.000.327 8.002.321]`. Unmodified 4.6 and the candidate produce matching canonical jam lengths and SHA-256 digests of all 18 event products, covering 4,322,310,347 bytes of effects and cores. The candidate executes 2,369,421 steps in 582 batches during this replay.

```sh
zig build tail-test decision-test nock-test -Doptimize=ReleaseFast \
  -Dcopt=-DU3_DECISION_BENCH -Dcopt=-DU3_TAIL_BENCH -Dcopt=-DU3_TAIL_STATS
U3_CASE_GC=1 zig build tail-test decision-test nock-test -Dmem-dbg=true \
  -Doptimize=ReleaseFast -Dcopt=-DU3_DECISION_BENCH -Dcopt=-DU3_TAIL_BENCH
```

Performance builds omit `U3_TAIL_STATS`. Event-product hashes require a separate `U3_REPLAY_AUDIT` build.
