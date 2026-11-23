# Computing with the shape of a Nock subject on Vere 4.6

On the full Hoon compiler workload, the shape optimization eliminates 26,586,870 cell allocations and reduces median evaluation time from 21.00 to 18.81 seconds. On the prelude it eliminates 27,522,531 allocations and reduces the median from 21.40 to 19.13 seconds. The control enables all previous branch optimizations, including ranked cue.

## How it works

* **Elide a lexical binding.** Nock 8 evaluates a formula and pairs the result with the old subject. When the first operand evaluated by the continuation projects into the binding exactly once and later projections refer to the old subject, the compiler substitutes the binding and rewrites the axes. It checks at most 32 formula nodes and emits ordinary bytecode. An unsupported operation, another use of the binding, or an observation of the whole pair selects the ordinary compiler path. The binding can itself call, log or bail.
* **Build an escaping closure.** When the continuation constructs a literal-headed core over the new subject, the VM has the battery, sample and context. It creates the two cells directly. The triple constructor takes two slots from the road’s cell pool in allocation order when available. A following literal `%fast` hint runs the usual jet registration checks with operands already owned by the program and its product.

For example, axis 5 of `[binding subject]` is the tail of the binding and axis 6 is the head of the original subject. A continuation pairing those fields can evaluate the binding once, project from the two existing values and allocate just the result pair. A failing binding or an emitted hint still precedes the second projection.

Allocation counts cover the evaluation interval in separate instrumented processes. Hoon uses 521,789,437 cells on the control and 495,202,567 on the candidate. The candidate executes the combined two-cell constructor 99,988,384 times and handles 48,442,851 adjacent registration hints. The prelude uses 537,337,682 and 509,815,151 cells, with 103,066,571 combined constructors and 49,711,160 adjacent hints.

An exploratory Hoon run samples 509,525 allocations at pseudorandom intervals averaging 1,024 allocations. For 85,209 samples, the probe observes a projection and release within 32 subsequent allocations, without observing a reference-count gain or insertion into a parent via `u3i_cell`. The probe sees selected projection and ownership paths and censors samples when an address is reused or its table entry collides.

## Full compiler workloads

Each of six rounds runs three fresh processes on distinct physical performance cores of a shared i7-12700K. The builds rotate among CPUs 4, 6 and 8. Workload order reverses each round. Each process loads the matching ivory core and evaluates the published jamfile. Timing covers Nock evaluation. Every run checks the published mugs and compares the full jammed product to the stock reference. Percentages use the ratios of median elapsed times. [Benchmark desk](https://github.com/urbit/benchmark/tree/ae0e498e19beba4ab585299adcbcce0427594e87) · [Individual samples](nock-shapes-samples.csv)

| Workload | Stock 4.6 | Prior branch | With shapes | Less time vs stock | Less time vs branch | Faster vs branch |
|---|---:|---:|---:|---:|---:|---:|
| hoon | 25.89 s | 21.00 s | 18.81 s | 27.3% | 10.4% | 6/6 |
| prelude | 26.70 s | 21.40 s | 19.13 s | 28.3% | 10.6% | 6/6 |

## Running ships

Each comparison boots two private fakezods from the same 4.6 pill. The ships exchange physical cores and reverse benchmark order across rounds. Dojo times the expression with `%bout`. Each row lists eight measured observations per ship after a warm-up. The stock and prior-branch comparisons use separate pairs of ships.

The compile row parses and evaluates a fresh Hoon source string in the running ship using `+ream` and `+slap`. Each source nests 64 calls to `+add`, starts with a different integer and produces a checked sum. The seven public desk workloads check their complete expected result on every invocation in a timed batch. Positive percentages below indicate less elapsed time.

| Workload | Prior branch → shapes | Less time vs branch | Stock → shapes | Less time vs stock |
|---|---:|---:|---:|---:|
| Compile and evaluate | 13.55 → 11.75 ms | +13.3% | 15.22 → 12.22 ms | +19.7% |
| ackermann | 600.56 → 613.74 ms | -2.2% | 905.98 → 617.66 ms | +31.8% |
| add | 45.65 → 46.30 ms | -1.4% | 69.84 → 47.52 ms | +32.0% |
| dec | 39.45 → 41.11 ms | -4.2% | 58.66 → 41.36 ms | +29.5% |
| factorial | 89.03 → 88.83 ms | +0.2% | 123.00 → 90.54 ms | +26.4% |
| fibonacci | 39.55 → 39.41 ms | +0.4% | 53.44 → 39.60 ms | +25.9% |
| bigslot | 16.19 → 16.22 ms | -0.2% | 23.35 → 16.33 ms | +30.1% |
| mmul | 15.82 → 16.32 ms | -3.2% | 22.20 → 15.97 ms | +28.1% |

## Cold replay

Two crossed pairs replay the same frozen fakezod on CPUs 0 and 10, bootstrapping through event 5 and running events 6–143. The timer includes the final save. The prior branch averages 353.88 seconds and the candidate averages 314.22 seconds (+11.2% less elapsed time). The within-pair reductions are +7.3% and +14.9%. These two observations per build provide limited precision.

## Correctness and configuration

The differential suite checks 5,419 products and 5,095 expected exits. The branch control and candidate produce identical result and exit traces on 32-bit and 64-bit looms with memory debugging and reference checks. Targeted cases check a logging binding before a gate call and before a failing argument, keep and lose positions, large axes and the analysis budget. A 261-site program exercises byte and short literal and registration operands, repeated valid registration, rejected clues and changes of subject. Separate allocation checks retain shared children and exercise a road with zero, one or two pooled cells available.

The candidate matches the stock bootstrap core and all 138 event products in the cold replay, plus all 17 products from events 233–249 of a saved Dojo checkpoint. Each comparison uses canonical jam length and SHA-256.

The performance build uses Zig 0.15.2, ReleaseFast, Linux x86-64 musl and a 32-bit loom on release tag `vere-v4.6` at `8ddc4b786979574dbfcb655e3db1b634f658d0de`. The prior branch enables `U3_DECISION_BENCH`, `U3_OWNER_BENCH`, `U3_REUSE_BENCH`, `U3_SPINE_BENCH`, `U3_DIRECT_EDIT_BENCH` and `U3_CUE_RANK`. The candidate adds `-Dcopt=-DU3_NOUN_SHAPES`. The development checkout also runs `decision-test` and `nock-test` with the shape flag enabled by itself.
