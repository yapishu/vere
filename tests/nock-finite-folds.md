# Evaluating two-state folds on shared noun trees

Cold replay of the frozen ~zod log through event 143 takes **217.7 s**, compared with **301.8 s** on the branch control and **435.55 s** on upstream develop. The incremental reduction is **27.9%** across two paired trials. Most of the opportunity is in Clay's desk rebuild, where `++squash` repeatedly traverses shared stencil branches. An instrumented replay through event 16 records 44,817 reductions. [Trial data](nock-finite-fold-samples.jsonl)

| Cold replay | Control CPU | Fold CPU | Branch control | Fold | Less time | Peak RSS, control / fold |
|---|---:|---:|---:|---:|---:|---:|
| 1 | 12 | 4 | 308.01 s | 222.48 s | 27.8% | 1,085.1 / 1,085.9 MiB |
| 2 | 4 | 12 | 295.60 s | 212.95 s | 28.0% | 1,085.1 / 1,085.9 MiB |

Each process starts on its own copy of the frozen log. Wall time includes bootstrap, all replayed events and the final save. The branch builds enable the same optimizations, including compiled projections. The two processes exchange physical performance cores between trials on a shared i7-12700K. The upstream time is a single run of develop at `697fc95ddd`. The branch control uses the same checkout as the candidate, based on `7670c28786`, with the fold flag off.

## How the evaluator establishes a result

Vere recognizes an arm that reads a tagged leaf, or recursively replaces its sample with each child and merges the answers. The merge can be plain Nock over a pair or a curried gate built from the gate context. A bounded walk accepts subtrees whose leaf payloads are zero or one common noun reference. It follows physical sharing and builds a 16-bit result at each distinct interior node. Bit *t* answers the question “what value would this subtree produce if the merge had binary truth table *t*?” This evaluates all 16 tables in one traversal. It preserves child order even for an asymmetric merge. Computing over the shared representation follows the compressed-tree evaluation literature. [Lohrey and Maneth, *Tree Automata and XPath on Compressed Trees*](https://www.research.ed.ac.uk/en/publications/the-complexity-of-tree-automata-and-xpath-on-grammar-compressed-t/)

Vere checks both recursive dispatch sites against the same arm and verifies that changing the sample preserves their interpretation. It resolves the merge in the current context for all four possible operand pairs. A small evaluator supports slots, literals, equality and conditionals. For the native set-union jet it checks the active jet function, registration and arguments. A curried constructor can use a warmed `%fast` registration only when the ordinary registration check already succeeds. The resulting four answers select one bit of the root summary. A lazy leaf, a third value, a scry, a logging hint on an evaluated path, or a failed dispatch check sends the call through its ordinary bytecode. Smaller recursive calls can qualify below that point.

Each scan admits up to 4,096 distinct interior nodes with a C recursion limit of 192. The fixed scratch table uses 96 KiB on a 32-bit loom and 192 KiB on a 64-bit loom. A new scan ignores all earlier entries. After an unverified merge, the same battery and context skip the next 64 qualifying probes. This limits repeated proof work when the merge has effects or produces a third value.

## Other workloads

| Workload | Branch control | Fold |
|---|---:|---:|
| Compile Hoon | 17.74 s | 17.45 s |
| Compile Prelude | 18.39 s | 18.08 s |
| Synthetic shared tree, 16 direct merges | 120.57 ms | 0.30 ms |
| Synthetic shared tree, 16 curried merges | 266.22 ms | 0.83 ms |
| Synthetic tree, merge outside the two-state domain | 100.25 ms | 99.52 ms |

The compiler measurements are medians of three fresh processes per build on the same core with alternating build order by round. They time evaluation, check the complete output jam against the reference bytes, and record zero fold reductions in both public compiler inputs. The synthetic measurements use five fresh processes per build. The first two rows for shared trees give the median time to evaluate all 16 binary operators on a recursively shared Fibonacci tree with 22 interior construction steps. Each operator's expected product is calculated as the tree is built. The direct and curried versions use different Nock formulas from the kernel. The third synthetic case repeatedly doubles a shared pair of leaves 17 times and uses a merge that returns 99 on leaves containing zero and 73. It measures the cost of repeated guard misses.

## Correctness and reproduction

The branch's two loom widths reproduce all 10,514 upstream general trace records. Focused tests exercise all 16 operators, both gate forms, changed ambient functions, failed projections, observable hints in constructors and merges, and a recursive jet whose registration depends on the sample. The focused products agree on both road orientations. Upstream reopens the candidate checkpoint at event 248 and completes event 249.

The 32-bit candidate matches upstream's canonical lengths and SHA-256 hashes for the bootstrap core, all 138 cold-replay products and all 17 checkpoint-replay products, totaling 33,279,069,693 bytes. The 64-bit candidate also matches upstream for the bootstrap and each product through the desk rebuild at event 16. [Per-run manifest digests](nock-finite-fold-samples.jsonl)

Build ReleaseFast with Zig 0.15.2. For the matched controls at `3b4883761f`, use the branch feature flags in the workload profile plus `U3_COMPILED_PROJECTIONS`. Add `U3_DAG_FOLD` for the candidate and `U3_REPLAY_AUDIT` for product manifests. Run `urbit play -y -f -n 143 /path/to/fresh-pier` for cold replay. The focused tests run with `U3_FOLD_CHECK=1 path/to/decision-test`, and the synthetic timings use `U3_FOLD_BENCH=1`. Add `U3_FOLD_STATS` to assert that the focused calls enter the optimization and to report the number of reductions. [Branch build flags](nock-workload-profile.md) · [Replay driver](bench-develop-replay.py) · [Compiler timer](bench-desk-eval.patch)
