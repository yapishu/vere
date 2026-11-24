# Ackermann and unit-stride Nock loops

The public Ackermann formula implements unsigned decrement by starting a counter at zero and incrementing it until its successor equals the input. Every non-base Ackermann call uses this decrement. Vere recognizes the counter loop in the compiled Nock arm and calculates its exit value in constant time. The surrounding recursive calls still execute. [Benchmark formula](https://github.com/urbit/benchmark/blob/ae0e498e19beba4ab585299adcbcce0427594e87/desk/bar/ackermann.nock) · [Hoon source](https://github.com/urbit/benchmark/blob/ae0e498e19beba4ab585299adcbcce0427594e87/desk/bar/ackermann.hoon)

## Which loops qualify

The arm compares a counter or its successor with a fixed bound. If the comparison fails, it increments only that counter and tail-calls itself. The compiler checks the formula, including the edited axis and whether the bound can change. A literal or a slot in a disjoint subtree qualifies as a bound. A slot or literal qualifies as an exit expression. Hints in the body require ordinary evaluation.

The first tail call resolves normally. When it reaches the same program and battery, the interpreter checks that the counter and bound are direct atoms, the exit is reachable by incrementing, and jet resolution and tracing permit skipping the calls. It then edits the counter once. For a successor test the exit value is one below the bound. For a direct comparison it is the bound itself. The original bytecode evaluates the exit expression, including any slot failure. This uses a fixed amount of scratch space and the existing tail-call instruction. The compiler technique of deriving an induction variable's exit value from its recurrence is also used in scalar evolution. [LLVM scalar evolution](https://llvm.org/devmtg/2009-10/ScalarEvolutionAndLoopOptimization.pdf)

## Live Dojo timings

All three binaries use Zig 0.15.2, ReleaseFast, x86-64 Linux musl and a 32-bit loom on an i7-12700K. The upstream control is `697fc95ddd`. The starting branch is `ec95989c9e` (runtime `c234e064f9`). Both branch binaries use the develop benchmark helper's flags plus `U3_CUE_VIEW` and `U3_JAM_CLASSES`. The counter optimization is enabled in the default build as well. [Build flags](bench-develop-build.sh)

Each build runs in a separate live fakezod from the same checkpoint. The commands run sequentially on CPU 4. Build order rotates and workload order alternates between rounds. The table gives medians across eight rounds after one warm-up. The Dojo `%bout` timer includes the Nock evaluation and the check against the expected product. [Per-trial samples](nock-counter-samples.jsonl)

| Formula and input | Develop | Starting branch | Counter optimization | Speedup vs starting branch |
| --- | ---: | ---: | ---: | ---: |
| Ackermann(2,30) | 1.442 ms | 1.006 ms | 0.404 ms | 2.49× |
| Ackermann(3,5) | 86.24 ms | 57.02 ms | 8.38 ms | 6.81× |
| Ackermann(3,6) | 673.03 ms | 439.37 ms | 33.79 ms | 13.00× |
| Decrement 1,000,000 | 50.98 ms | 33.99 ms | 0.0055 ms | ~6,180× |
| Add 1,000 and 2,000 | 25.88 ms | 17.04 ms | 0.240 ms | 71.17× |
| 64 calls to bigslot | 17.90 ms | 11.91 ms | 1.606 ms | 7.42× |

The benchmark desk's add and bigslot formulas each contain a local decrement loop. The single-decrement measurement approaches the overhead of the Dojo timing expression. After these rounds, summed king and worker PSS is 171,944 KiB on develop, 172,000 KiB on the starting branch, and 171,936 KiB with the counter optimization. These readings measure resident memory after evaluation.

To time the public input, bind the complete noun from `ackermann.nock` to `d-ackermann` with type `[* *]` in Dojo. The same formula accepts all the subjects below. The gate returns the number of calls whose complete product equals the expectation.

```hoon
=d-run |=([n=@ud s=* f=* e=*] ^-(@ud =/(i n |-(?:(=(i 0) n ?:(=(e .*(s f)) $(i (dec i)) !!))))))
~>(%bout (d-run 1 [3 6] +.d-ackermann 509))
~>(%bout (d-run 1 [3 8] +.d-ackermann 2.045))
```

## Larger inputs and checkpoint compatibility

Each entry here is a single checked evaluation. The four ships run concurrently on separate P-cores. Develop, the optimized benchmark build and the default ReleaseFast build resume copies of the optimized runtime's saved pier and evaluate the persisted formula. The starting branch resumes its own pier.

| Input | Product | Develop | Starting branch | Optimized benchmark build | Default ReleaseFast |
| --- | ---: | ---: | ---: | ---: | ---: |
| Ackermann(3,7) | 1,021 | 5.093 s | 3.617 s | 0.137 s | 0.154 s |
| Ackermann(3,8) | 2,045 | 38.050 s | 28.325 s | 0.565 s | 0.627 s |

The focused tail-call suite checks 314 cases on both loom widths, including bail behavior, hint delivery, calls to a different battery and the direct-atom boundary. The memory- and CPU-debug 64-bit configuration also checks the logical kick count. Both widths reproduce develop's 10,514-case Nock trace byte for byte, including 5,419 products and 5,095 exits. A separate instrumented replay matches develop's canonical lengths and SHA-256 hashes for all 17 event products from 233 through 249 (4,074,710,338 bytes).

Ordinary checkpoint replay over events 233–249 takes a median of 18.98 s on each build. Three rounds rotate the processes across three P-cores, with separate pier copies and a shared filesystem. These wall times include loading the checkpoint and saving the result. [Replay driver](bench-develop-replay.py)
