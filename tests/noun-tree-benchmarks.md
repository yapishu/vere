# Canonical map construction, key sets and Nock equality

The upstream control is `697fc95ddd`. The starting branch is `a0d701540a`, the bulk builder is `09790fe7b7`, the key projection is `b129508ea9`, and the cumulative runtime is `c234e064f9`. The three changes apply by default. The timing binaries use ReleaseFast, Zig 0.15.2 and a 32-bit loom. All branch builds use the feature flags in the develop build helper plus `U3_CUE_VIEW` and `U3_JAM_CLASSES`. The host is a shared Linux i7-12700K. [Build helper](bench-develop-build.sh) · [Per-trial timings, memory and replay hashes](noun-tree-samples.jsonl)

## Live ship operations

The map builder sorts cached 31-bit key mugs with four stable radix passes on large batches. It orders colliding buckets by Hoon's exact noun order and retains the last map entry for each equal key. With keys in order, a stack holds the open right spine of the Cartesian tree. Each key opens one node. Popping the stack closes completed subtrees directly into the canonical noun. The matched open and close operations are the connection to balanced-parentheses tree representations. The stack stores just the open nodes. [Cartesian trees and succinct binary tree representations](https://arxiv.org/abs/1410.4963)

A canonical map and its key set have the same binary-search and heap orders. The key jet checks the map invariants, projects the keys in postorder, and shares the existing pair of empty branches at leaves. Noncanonical map-shaped nouns use ordered set insertion.

| Workload, ms | Develop | Starting branch | Final | Less time vs develop |
| --- | ---: | ---: | ---: | ---: |
| Build a set of 20,000 atoms | 13.633 | 14.188 | 0.62 | 95.5% |
| Build a map of 20,000 pairs | 16.064 | 17.102 | 0.755 | 95.3% |
| Build a map from 20,000 pairs with 257 keys | 7.622 | 8.289 | 0.689 | 91.0% |
| Extract 20,000 atom keys | 15.059 | 14.46 | 2.119 | 85.9% |
| Extract 20,000 two-atom keys | 13.191 | 12.521 | 2.288 | 82.7% |
| Extract 256 keys | 0.103 | 0.096 | 0.024 | 76.7% |
| Build an eight-pair map | 0.007 | 0.008 | 0.006 | 14.3% |
| Build from 20,000 copies of one pair | 0.574 | 0.631 | 0.099 | 82.8% |

The isolated builder series takes 0.641 ms for a 20,000-pair map against 16.331 ms on its immediate parent. The isolated key projection takes 1.843 ms against 13.72 ms on the builder commit.

Nock opcode 5 can finish a child comparison immediately when both edges point to the same noun. For large comparisons, the memo table records newly visited pairs at unresolved binary forks and at intervals along chains. Only an exact completed comparison enters the table. Successful comparisons still unify the corresponding child pointers where the road permits it. [Adams and Dybvig on equality for trees and graphs](https://michaeldadams.org/papers/efficient_equality/)

| Workload, ms | Develop | Parent of equality change | Final | Less time vs develop |
| --- | ---: | ---: | ---: | ---: |
| Equal lists of 120,000 atoms | 25.805 | 26.555 | 3.178 | 87.7% |
| Same 120,000 atoms with one extra tail element | 3.448 | 3.966 | 2.138 | 38.0% |
| Equal lists of 120,000 two-atom records | 71.951 | 65.794 | 14.255 | 80.2% |
| Equal lists of 256 atoms | 0.003 | 0.003 | 0.003 | 0.0% |

On the record workload, an ablation with the identical-edge shortcut and dense memoization takes 35.904 ms. Sparse checkpoints take 14.906 ms in the same series, another 58.5% reduction. On equal atom lists the two take 2.938 and 2.994 ms.

Each series resumes independent copies of the same event-232 fakezod checkpoint. After warm-up, six trials rotate executable order on one physical core. Trial `r` starts its input range at `100 + 17*r`. Dojo times only the operation with `%bout`. Hashing each complete jammed tree sits outside the timer. All three builds agree on every hash, and each trial has a distinct tree hash for each construction or projection workload. The equality workload checks each loobean against its expected value. The 3 µs readings approach the timing resolution. [Dojo expressions](bench-noun-trees.dojo)

## Memory

For the 32-bit loom table, the native probe constructs the inputs on the home road and resets the allocator high-water mark on a fresh child road. The numbers include output nodes and temporary heap allocations. The road stack and senior input nouns sit outside this metric. The build enables `-Dcpu-dbg=true` for this measurement. Run `zig build -Doptimize=ReleaseFast -Dcpu-dbg=true tree-bench` and then `zig-out/bin/tree-bench CASE`. For a control build, the same driver compiles in place of `pkg/noun/jets_tests.c` in a source archive. [Heap measurement driver](../pkg/noun/tree_bench.c)

| Operation | Develop peak bytes | Starting branch peak bytes | Final peak bytes |
| --- | ---: | ---: | ---: |
| Build a set of 20,000 atoms | 1,589,072 | 1,589,072 | 966,480 |
| Build a map of 20,000 pairs | 1,589,072 | 1,589,072 | 966,480 |
| Build a map from 20,000 pairs with 257 keys | 999,248 | 999,248 | 327,680 |
| Extract 20,000 atom keys | 638,800 | 638,800 | 524,112 |
| Equal lists of 120,000 atoms | 4,911,792 | 4,911,792 | 512 |
| Equal lists of 120,000 two-atom records | 8,926,288 | 8,926,288 | 2,304,032 |

On a 64-bit loom, the peak heap increase for a 20,000-pair map is 3,178,304 bytes on the starting branch and 1,933,152 bytes on the final runtime. For equality of the two record lists it is 17,754,464 and 4,643,008 bytes. The sample file includes all six workloads on both widths.

Summed king and worker PSS at the end of the combined tree series is 173.1 MiB on develop, 178.0 MiB on the starting branch and 175.8 MiB on the final runtime. The equality series ends at 192.0, 192.5 and 188.6 MiB on develop, the parent of the equality commit and the final runtime. These are post-workload snapshots.

## Compiler and replay

| Compiler, seconds | Develop | Starting branch | Final | Less time vs develop |
| --- | ---: | ---: | ---: | ---: |
| Hoon | 23.281 | 17.297 | 16.715 | 28.2% |
| Prelude | 24.452 | 18.105 | 17.411 | 28.8% |

Fresh eval processes execute the same public compiler subjects and formulas on three rotating physical cores. Each median covers six trials after a warm-up. Every run checks the shipped compiler core against ivory, checks both published mugs, and matches the complete jam against its golden file byte for byte. Median process peak RSS for Hoon is 115.2, 118.5 and 118.5 MiB. Prelude uses 112.7, 115.8 and 115.8 MiB in the same order. [Published compiler workloads](https://github.com/urbit/benchmark/tree/ae0e498e19beba4ab585299adcbcce0427594e87/desk/bar)

Full replay starts from the event log, bootstraps through event 5 and processes events 6–143. Two runs per executable swap physical cores, using private tmpfs copies. Develop takes 426.8 and 405.0 s. The final runtime takes 282.7 and 282.3 s, a 32.1% reduction across the two runs. A separate single pair comparing the starting branch and final runtime takes 291.2 and 280.7 s. The shorter event-233–249 replay takes 1.483 s on develop, 1.467 s on the starting branch and 1.472 s on the final runtime across six serial, rotating-order trials on one core. These measurements include process startup and exit. Other builds and benchmarks share the host during portions of the trials. [Replay driver](bench-develop-replay.py)

## Product checks

* Both loom widths match develop's 10,514-case trace byte for byte, including 5,419 products and 5,095 exits. The memory- and CPU-debug builds reproduce the same trace. Its SHA-256 is `6c6ba3431c88493feb6ece2d722dc7b1388c27223db53c91d78216240c5315ae`.
* The bootstrap core, all 138 cold replay event products and all 17 checkpoint replay products match develop in canonical jam length and SHA-256. They cover 33,279,069,693 bytes of core and effect data.
* Individual map and set puts specify the expected shape for the bulk builders. The tests exercise last-value-wins handling for repeated keys, separately allocated equal keys, distinct keys with the same mug, and distinct mugs with the same heap priority. A pair of senior, shared DAGs exercises comparisons on a road where inner pointers cannot unify.
* Develop reopens the checkpoint saved by the final runtime after the last tree trial. The persisted map and three key-set queries reproduce the candidate's recorded hashes.
