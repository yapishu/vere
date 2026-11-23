# Running a prepared gate on its sample

An incoming gate call supplies a battery, a sample and a context. A callback such as `|=([a=@ b=@] b)` reads axis 13 of the gate. That is axis 3 of the sample. A list operation can pass the sample directly and return its tail. On Vere 4.6, mapping this callback over about 80,000 pairs takes 1.06 ms, down from 2.23 ms on the branch with noun shapes and 2.89 ms on stock.

## Which gates qualify

Vere's higher-order jets already prepare a gate once and call it repeatedly through `u3j_gate_slam`. Callers include `+turn`, `+roll`, `+skim`, map and set traversals, and the parser jets. With `-Dcopt=-DU3_GATE_SAMPLE`, preparation resolves the ordinary jet site and examines interpreted batteries. A literal returns its existing value. A slot below axis 6 reads the corresponding field of the sample. For an expression built from slots, literals, autocons, cell tests, increment, equality and conditionals, preparation rewrites sample-relative slots and compiles the result through Vere's usual program cache. Each invocation transfers ownership of the sample to this entry. It avoids constructing and releasing the two cells in `[battery [sample context]]`.

The analysis visits at most 32 formula nodes and handles direct axes. A read of the context, an observation of the whole core, an unsupported instruction or a larger formula uses the ordinary gate. A resolved jet retains its entry point. Runtime tracing and CPU profiling execute the original program. A failing direct projection also returns to the ordinary gate to raise its exit.

## Running ships

Each row gives the median of nine measured evaluations after one warm-up. Three private fakezods boot from the same release pill and resume from their checkpoints for the comparison. Their workers rotate among three physical performance cores of one shared i7-12700K. Each round uses a fresh list of 80,001 through 80,009 elements, and workload order reverses between rounds. Dojo times the operation with `%bout`. Full noun equality against a separately constructed expected list, or the expected fold accumulator, is checked outside the timed expression. The control branch enables the earlier optimizations through `U3_NOUN_SHAPES`. [Dojo expressions](bench-nock-prepared.dojo) · [Individual samples](nock-prepared-samples.csv)

| Operation | Stock 4.6 | Branch | Prepared samples | Less time vs branch | Less time vs stock |
|---|---:|---:|---:|---:|---:|
| Map a record field | 2.894 ms | 2.233 ms | 1.061 ms | 52.5% | 63.3% |
| Map increment | 2.749 ms | 2.212 ms | 1.033 ms | 53.3% | 62.4% |
| Map a conditional | 3.337 ms | 2.591 ms | 1.137 ms | 56.1% | 65.9% |
| Filter by equality | 2.511 ms | 2.087 ms | 0.845 ms | 59.5% | 66.3% |
| Fold an incrementing accumulator | 3.054 ms | 2.410 ms | 1.391 ms | 42.3% | 54.5% |
| Map addition with a captured value | 14.630 ms | 10.367 ms | 10.335 ms | 0.3% | 29.4% |

All five qualifying operations improve in every measured round. The captured-value control uses the gate context and takes the ordinary path.

## The change alone on stock 4.6

A second pair of fresh ships isolates this change on the release tag. One binary enables only `U3_GATE_SAMPLE`. The two ships exchange physical cores each round and run the same six checks, sizes and warm-up procedure.

| Operation | Stock 4.6 | Stock + prepared samples | Less time |
|---|---:|---:|---:|
| Map a record field | 2.793 ms | 1.150 ms | 58.8% |
| Map increment | 2.697 ms | 0.972 ms | 64.0% |
| Map a conditional | 3.265 ms | 1.224 ms | 62.5% |
| Filter by equality | 2.417 ms | 0.813 ms | 66.4% |
| Fold an incrementing accumulator | 3.089 ms | 1.425 ms | 53.9% |
| Map addition with a captured value | 14.262 ms | 14.005 ms | 1.8% |

The implementation lives in `pkg/noun/jets.c` and `pkg/noun/jets.h`. To try it alone, set `change` to the prepared-gate commit after fetching the experiment branch and run these commands in a checkout at `vere-v4.6`. The release names its runtime flag field `wag_w`.

```sh
git show --format= "$change" -- pkg/noun/jets.c pkg/noun/jets.h | sed 's/u3C\.wag_h/u3C.wag_w/g' | git apply
zig build -Doptimize=ReleaseFast -Dcopt=-DU3_GATE_SAMPLE
```

## Full compilers

The published Hoon and prelude desk inputs run in fresh 4.6 processes. Each of six rounds rotates stock, branch and candidate across three physical cores, reverses workload order across rounds, checks the published mugs, and compares the entire jammed product with the stock reference. The timer covers Nock evaluation. [Benchmark desk](https://github.com/urbit/benchmark/tree/ae0e498e19beba4ab585299adcbcce0427594e87)

| Workload | Stock 4.6 | Branch | Prepared samples | Change vs branch | Less time vs stock |
|---|---:|---:|---:|---:|---:|
| Hoon | 26.113 s | 18.657 s | 18.949 s | 1.6% more | 27.4% |
| Prelude | 27.097 s | 19.286 s | 19.458 s | 0.9% more | 28.2% |

Four additional crossed pairs isolate the change on stock 4.6. Median Hoon time is 25.352 s on stock and 25.270 s with the patch (0.3% less). Prelude is 26.408 s and 26.378 s (0.1% less).

On a separate instrumented Hoon run, 533,368 out of 3,443,977 prepared calls select a sample entry. There are 1,227,926 preparations, so many gates see few invocations over which to recover analysis and compilation costs. On the prelude, 518,449 out of 3,397,460 prepared calls qualify.

## Where the boundary comes from

On the Hoon workload, 81,080,748 of 99,988,384 combined gate constructors have a return as the next instruction. A pseudorandom lifetime sample follows individual roots until an observed call, edit, release, reuse or collision. Of 59,346 sampled sample edits, 59,128 resulting gates reach a VM call. Of these calls, 22,190 return through a jet. The input core matters to jet resolution even when an interpreted arm could compute its product from two fields.

Three release prototypes measure the cost of looking at every call. In a two-pair screen, executing complete literal gate factories inside the caller saves 2.1% on Hoon and 1.0% on the prelude. Fusing a factory return with a simple sample edit changes the four-pair medians by +0.5% and −0.2% in time saved. A third prototype speculates on field reads at adjacent edits and kicks. It completes 4,875,043 of 68,199,499 such Hoon calls using fields, but takes 5.8% more time on Hoon and 5.2% more on the prelude in three crossed pairs. Prepared callbacks give the analysis a place to run once per gate.

## Verification

The prepared-gate tests compare calls with an independent evaluator on the full core. They exercise reused sites, nested and shared samples, direct and indirect axes, aliasing between a literal and an argument, both conditional arms, unsupported operators, logging, exits and the analysis budget. Generated sample-only expressions run on both atoms and trees. Ownership tests check the reference count of a child after its parent is consumed on each road orientation. A CPU-debug build switches tracing and profiling on and off after preparation. Its allocation counter records zero gate cells on the sample route and two on the ordinary route. A release check with the real `+dec` jet observes an ordinary native entry and checks every result of mapping it over a range.

The candidate matches the canonical jam length and SHA-256 from stock for the bootstrap and all 138 event products through event 143. It also matches all 17 event products from the saved Dojo checkpoint. In the other direction, stock 4.6 resumes the standalone candidate’s saved pier and checks the full 80,009-element projected list. All four differential traces agree across the release control, release candidate and both development loom widths, with 5,419 products and 5,095 expected exits.

The timed release is `vere-v4.6` at `8ddc4b786979574dbfcb655e3db1b634f658d0de`, built with Zig 0.15.2, ReleaseFast, Linux x86-64 musl and a 32-bit loom. The development checkout runs the differential suite on both loom widths with memory debugging and reference checks. [Branch control configuration](nock-shapes.md)
