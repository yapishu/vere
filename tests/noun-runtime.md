# Nock and noun runtime performance

The comparison uses upstream develop at `697fc95ddd` and the integrated runtime at `4b5e666f2b`. Both use the ordinary ReleaseFast configuration. The Nock compiler shares subject paths across decisions, the evaluator reuses cells it owns, and list jets retain unchanged suffixes. Jam recognizes structurally equal subtrees when choosing backreferences. Cue indexes token starts for lookups. During Clay's desk rebuild, a two-state fold evaluates each distinct shared subtree once per eligible call. [Fold and eligibility tests](nock-finite-folds.md) · [Jam encoding](jam-classes.md) · [Cue decoding](cue-word-decoding.md)

## End-to-end measurements

| Workload | Upstream develop | Integrated | Change in elapsed time |
|---|---:|---:|---:|
| Cold replay through event 143 | 439.6 s | 208.3 s | 52.6% less |
| Compile Hoon | 24.02 s | 17.12 s | 28.7% less |
| Compile Prelude | 24.83 s | 17.69 s | 28.8% less |
| 96 authenticated group and channel reads | 1.845 s | 1.411 s | 23.5% less |
| Resume a checkpoint and replay events 233–249 | 1.742 s | 1.787 s | 2.6% more |

Cold replay starts from independent copies of one frozen fake ~zod log and includes bootstrap, the kernel update, Clay's desk rebuild, and the final save. Each runtime runs once on each of two physical cores, in opposite order. The short replay starts from the same saved checkpoint in every trial and uses four pairs with alternating core assignments. All piers reside on tmpfs. A bytecode version change causes Vere to rebuild the saved program and jet caches on resume. [Individual trials](noun-runtime-samples.jsonl)

The compiler figures are medians of three measured runs after warm-up. They time evaluation inside fresh Vere processes. Every run verifies the published input and output mugs and compares the full output bytes with the reference jam. The live test runs two local ships from copies of the same populated checkpoint. Each batch reads `groups/v3/groups`, `groups/v1/groups` and `channels/v2/channels` 32 times each over authenticated loopback HTTP. The driver alternates the order on one physical core, checks every JSON response, and measures eight batches per build after warm-up. All eight paired batches favor the integrated runtime. The fixture has three groups with one channel each and no message history.

| Memory measurement | Upstream develop | Integrated |
|---|---:|---:|
| Cold replay, median peak RSS | 1,210.1 MiB | 1,086.0 MiB |
| Live group and channel reads, median combined king and worker PSS | 299.3 MiB | 312.1 MiB |

Times and memory measurements use x86-64 Linux musl, Zig 0.15.2, a 32-bit loom and a shared i7-12700K. Timing processes use physical performance cores. A separate process hashes replay products on another core. [Raw timings, memory samples, and digests](noun-runtime-samples.jsonl)

## Product equivalence

- Both loom widths match upstream byte for byte on 10,514 general Nock cases (5,419 products and 5,095 expected exits) and 4,614 focused subject-projection cases. The general trace also matches with CPU and memory debugging on x86-64 and under AArch64 Linux emulation. The fold tests exercise all 16 binary merge operators and the ordinary execution path for effects, changed contexts and unsupported inputs. [Differential tests](../pkg/noun/decision_tests.c) · [Fold tests](../pkg/noun/fold_tests.inc)
- On a 32-bit loom, the canonical jam length and SHA-256 match upstream for the bootstrap core and all 155 replay event products, including effects and the resulting Arvo core. This checks 33,279,069,693 bytes. The 64-bit build also matches through the desk rebuild at event 16. [Per-event reference lengths and hashes](noun-tree-samples.jsonl)
- The integrated runtime resumes a checkpoint written by the preceding optimized bytecode build at event 247 and saves event 248. Upstream develop resumes that checkpoint and completes event 249.

## Reproduce the comparison

The build helper takes two Git revisions and applies the same evaluation timer to each. Use the pinned pair for the upstream comparison or two adjacent runtime revisions to measure an individual change. The compiler input is public. [Build helper](bench-develop-build.sh) · [Compiler desk](https://github.com/urbit/benchmark/tree/ae0e498e19beba4ab585299adcbcce0427594e87)

```sh
bench=$(mktemp -d)
tests/bench-develop-build.sh "$bench/build" 697fc95ddd 4b5e666f2b
git clone https://github.com/urbit/benchmark.git "$bench/desk"
git -C "$bench/desk" checkout --detach ae0e498e19beba4ab585299adcbcce0427594e87
upstream="$bench/build/develop/x86_64-linux-musl/urbit"
integrated="$bench/build/optimized/x86_64-linux-musl/urbit"
python3 tests/bench-develop-compiler.py "$upstream" "$integrated" "$bench/desk" --cores 4 8 --rounds 3
```

The replay inputs are local fixtures. The SHA-256 of `frozen-zod-143.tar.zst` is `3c29b3ad9b2acd2140acde430b72a9c53f4744d1ee5f158fc65550d8c7cb07d6`. The SHA-256 of `dojo-233-249.tar.zst` is `18a6f1a987435503601c05e6474c03c128c704c88528415d20ec1baff0b4fb78`. For cold wall time and RSS, run `/usr/bin/time -f '%e %M' taskset -c CORE BINARY play -y -f -n 143 PIER` on a fresh copy for each run. For checkpoint timing, give the replay driver the extracted pier and `--first 233 --last 249 --rounds 4 --cores 6 10`. For product checks, add `-Dcopt=-DU3_REPLAY_AUDIT` to the build and compare every emitted `bootstrap-core` and `product` line with the reference manifest. [Replay driver](bench-develop-replay.py)
