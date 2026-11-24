# Structural sharing in list jets

For the cumulative branch comparison against upstream `develop`, see the [development benchmarks](develop-benchmarks.md). The measurements below use Vere 4.6.

Vere can reuse the unchanged tail of a list across a map or filter. The opt-in build flag is `-Dcopt=-DU3_LIST_SHARING`. It enables prepared sample gates. `+turn`, `+skim`, `+skip` and `+murn` evaluate each callback from left to right. For qualifying gates they defer copying a run of unchanged elements until an edit requires that prefix. On reaching the end they retain the original suffix once. A map recognizes an unchanged element by noun identity. A filter already knows which input elements it keeps. The traversals use constant auxiliary space.

Vere can unify equal subtrees by replacing interior pointers during noun equality. Gate preparation therefore certifies only sample programs whose equality operations each have a literal atom operand. Slots and literal gates also qualify. Resolved native jets, captured context, hints, unrestricted equality, tracing and profiling take the ordinary callback path. The jet still visits an improper list’s tail and raises its usual exit. Builds with pooled pair allocation also use the prefix constructor for `+weld`.

## Ship memory

Values are MiB of combined resident memory for the Vere process and its worker. Each cell is the median of three independent runs. The checkpoint workloads start from fresh copies of the same fakezod and event log. The cold boots start from the Vere 4.6 release pill with `urbit -F zod`. The settled fresh-boot sample is a separate three-second window beginning 30 seconds after the first checked Dojo response. The sampler reads Linux `smaps_rollup` about every 50 ms. Stable phases use the median sample in each run. A peak is the largest observed combined sample. RSS counts pages mapped by both processes twice. The samples also include proportional and private memory.

| Workload and phase | Stock 4.6 | Prepared-gate branch | List sharing |
|---|---:|---:|---:|
| Fresh fakezod boot, sampled peak | 2,671.1 | 2,633.2 | 2,633.2 |
| Fresh fakezod, 30 seconds after the first Dojo check | 760.4 | 759.9 | 756.7 |
| Resumed fakezod at Dojo | 86.3 | 86.2 | 86.2 |
| 160,000-item map, filter and fold, sampled peak | 118.4 | 118.3 | 118.3 |
| After clearing the Dojo bindings | 118.5 | 118.3 | 118.3 |
| One 160,000-item atom list | 97.3 | 97.2 | 97.2 |
| 16 independent edits of the atom list held together | 243.0 | 243.0 | 97.6 |
| One 160,000-record list | 110.7 | 110.5 | 110.5 |
| 16 successive revisions of the record list held together | 180.6 | 180.4 | 110.6 |

The independent edits each replace the first atom with a distinct literal. The successive revisions reuse one gate that increments a tagged header and returns each data record unchanged. Each revision takes the preceding list as input and lives in a separate Dojo binding. Full noun equality checks run after the retained-memory samples. The comparison covers these fakezod workloads and a short idle window.

## Live timing

Three ships run the same commands on separate physical cores of a shared i7-12700K. Workers rotate cores every round. One warm-up precedes 12 measured rounds on lists of 80,001 through 80,012 elements. Dojo measures the operation with `%bout`, and each ship compares the complete product with an independently constructed expected value outside the timed expression. Values are medians. Negative changes in elapsed time indicate faster execution.

| Operation | Stock 4.6 (ms) | Prepared branch (ms) | List sharing (ms) | Time vs branch | Time vs stock |
|---|---:|---:|---:|---:|---:|
| Concatenate two lists | 0.577 | 0.600 | 0.588 | -2.1% | +1.8% |
| Return each item | 2.587 | 0.778 | 0.406 | -47.9% | -84.3% |
| Replace item 1 | 3.206 | 0.999 | 0.691 | -30.9% | -78.5% |
| Replace item 40,000 | 3.209 | 0.982 | 0.933 | -4.9% | -70.9% |
| Replace item 80,000 | 3.449 | 1.236 | 1.457 | +17.8% | -57.7% |
| Update record 42 | 3.319 | 1.182 | 0.906 | -23.4% | -72.7% |
| Project a record field | 2.854 | 1.052 | 0.853 | -18.9% | -70.1% |
| Increment every atom | 2.688 | 0.990 | 0.910 | -8.0% | -66.1% |
| Remove item 42 | 2.943 | 1.062 | 0.622 | -41.5% | -78.9% |
| Keep every item | 2.503 | 1.067 | 0.446 | -58.2% | -82.2% |
| Keep only item 42 | 2.448 | 0.768 | 0.626 | -18.5% | -74.4% |
| Drop the first unit result | 4.093 | 1.618 | 1.188 | -26.6% | -71.0% |
| Remove alternate items | 15.775 | 11.797 | 11.762 | -0.3% | -25.4% |
| Add a captured value | 13.911 | 9.697 | 9.806 | +1.1% | -29.5% |

Updating record 42, replacing item 1, projecting a field and removing item 42 each beat the branch in all 12 measured rounds. The near-end edit loses all 12 rounds. It forces a second pass to copy almost the entire prefix. A captured gate follows the ordinary traversal.

## Isolated release builds

A second set of ships starts at the release tag. One build adds only prepared gates and another adds prepared gates plus list sharing. These builds use the stock allocator. Each figure is the median of 12 checked rounds.

| Operation | Stock (ms) | Prepared gates only (ms) | Gates and sharing (ms) | Time vs prepared |
|---|---:|---:|---:|---:|
| Update record 42 | 3.332 | 1.185 | 0.919 | -22.4% |
| Project a record field | 2.840 | 1.210 | 0.956 | -21.0% |
| Replace item 1 | 3.196 | 1.159 | 0.807 | -30.4% |
| Replace item 40,000 | 3.199 | 1.204 | 1.095 | -9.1% |
| Replace item 80,000 | 3.442 | 1.387 | 1.644 | +18.6% |
| Remove item 42 | 2.942 | 1.073 | 0.644 | -40.0% |
| Keep every item | 2.482 | 1.037 | 0.445 | -57.1% |
| Concatenate two lists | 0.615 | 0.618 | 0.609 | -1.5% |
| Add a captured value | 13.698 | 13.133 | 13.668 | +4.1% |

## Compiler workloads and verification

The public `urbit/benchmark` workloads use six measured rounds following a warm-up, with rotated cores and alternating workload order. Every run checks the complete jammed compiler product byte for byte against the stock golden. [Benchmark desk](https://github.com/urbit/benchmark/tree/ae0e498e19beba4ab585299adcbcce0427594e87)

| Compiler input | Stock (s) | Prepared branch (s) | List sharing (s) | Time vs branch |
|---|---:|---:|---:|
| Hoon | 26.643 | 18.960 | 19.057 | +0.5% |
| Prelude | 26.854 | 19.130 | 19.328 | +1.0% |

The list tests check all 256 keep/change masks for each of the four jets on both road orientations. They inspect the shared tail by identity, release the input, then compare the full product. A separate oracle evaluates 256 generated gate expressions per jet on the full Nock core. Cases cover invalid predicates, malformed lists, effect order, equality that unifies, profile flags, odd and even long prefixes, and reference counts. The release candidate and both development loom widths match the reference trace of 5,419 products and 5,095 exits.

Stock reopens a checkpoint written by the candidate and verifies all 16 saved edit results. Canonical jam lengths and SHA-256 hashes match stock for the bootstrap, 138 cold event products, and 17 saved-checkpoint event products.

The timed binaries use Zig 0.15.2, ReleaseFast, x86-64 Linux musl and a 32-bit loom at release tag `vere-v4.6` (`8ddc4b7869`). The cumulative control enables all branch optimizations through `3401f993e1`. [Prepared-gate configuration](nock-prepared.md) · [Dojo inputs](bench-list-sharing.dojo) · [Individual measurements](list-sharing-samples.jsonl)
