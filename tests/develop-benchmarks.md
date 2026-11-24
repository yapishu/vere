# Vere benchmarks against upstream develop

The control is upstream `urbit/vere` at `697fc95ddd`. The candidate is `ca2a0165a1`, five runtime commits directly on that revision. The compiled version strings are `5.0-697fc95` and `5.0-ca2a016`. Both include upstream’s deferred home-road reference counting. The comparison measures the cumulative branch with its nine optimization flags enabled. [Upstream revision](https://github.com/urbit/vere/commit/697fc95ddd5fb787e1f71d8a383a2587f1538c94) · [Candidate](https://github.com/yapishu/vere/commit/ca2a0165a1e830cff9e545151233683d038f9bff)

Measurements run on a shared Linux i7-12700K on 23 November 2026. The tables report medians. Positive percentages mean less elapsed time or resident memory, using `100 × (1 − candidate median / develop median)`. Paired timing runs exchange physical performance cores between rounds. [Individual measurements and verification hashes](develop-bench-samples.jsonl)

## Compiler, decoder and event replay

| Workload | develop | Optimized | Less time | Faster pairs |
| --- | ---: | ---: | ---: | ---: |
| Compile Hoon (s) | 23.442 | 17.207 | +26.6% | 6/6 |
| Compile prelude (s) | 24.382 | 17.660 | +27.6% | 6/6 |
| Nock +cue, 143 records (ms) | 2,732.2 | 486.5 | +82.2% | 6/6 |
| Pier decoder, 143 records (ms) | 1,928.1 | 410.6 | +78.7% | 6/6 |
| Fresh fakezod to checked Dojo reply (s, sampled) | 57.21 | 44.16 | +22.8% | 3/3 |
| Full replay, events 6–143 (s) | 432.122 | 320.420 | +25.8% | 4/4 |
| Checkpoint replay, first series (s) | 8.353 | 8.478 | -1.5% | 5/6 |
| Checkpoint replay, second series (s) | 8.248 | 7.803 | +5.4% | 5/6 |

The two compiler inputs come from the public benchmark desk. Each round starts fresh processes, reverses workload order between rounds, and times evaluation of the published Nock subject and formula. Six rounds follow one warm-up. Each run checks that the full compiler core in the input equals the ivory compiler core, checks the published input and product mugs, and compares the complete jammed output byte for byte to a reference. Reading the input and jamming the output sit outside the timer. [Benchmark desk and manifest](https://github.com/urbit/benchmark/tree/ae0e498e19beba4ab585299adcbcce0427594e87/desk/bar)

The decoder corpus consists of all 143 event records in the frozen fakezod log, totaling 240,504,745 jam bytes. Each entry point runs six times per build with a fresh process for each pass. The Nock measurement invokes the registered `+cue` gate. The pier measurement uses a reusable `u3s_cue_xeno_with` decoder. The timer covers decoding each record. Each output is re-encoded and checked against that record’s input length and SHA-256 outside the timer. [Decoder and corpus procedure](nock-cue.md)

Full replay starts with only the event log, bootstraps through event 5 and processes events 6–143, including a kernel update and a desk rebuild. The other replay starts from the event-232 checkpoint and processes 17 Dojo events. Both use private copies, start a fresh process per run and measure elapsed time from invocation of `urbit play` through exit. Copying the input is outside the timer. The OS file cache is shared. The short checkpoint replay has two six-pair series, one with output redirected to a regular file and one captured through a pipe. The median change switches sign between series. Rebuilding both binaries with the published helper and repeating the full replay on a second pair of cores gives 25.1% less elapsed time in one independent pair.

## Running ships

Both ships resume independent copies of a checkpoint created by upstream `develop` from a fresh `urbit -F zod`. Linux reports the expected executable for each ship and its worker. Dojo uses `%bout` and the workers exchange cores across 12 measured rounds following a warm-up.

### Public Nock programs

Each batch runs the published formula on the desk manifest’s subject and checks the complete expected noun on every call. The timer includes the check and the batch loop. [Source and expected products](https://github.com/urbit/benchmark/tree/ae0e498e19beba4ab585299adcbcce0427594e87/desk/bar)

| Operation | Calls per batch | develop (ms) | Optimized (ms) | Less time | Faster pairs |
| --- | ---: | ---: | ---: | ---: | ---: |
| ackermann | 1 | 698.516 | 475.394 | +31.9% | 12/12 |
| add | 2 | 53.598 | 36.288 | +32.3% | 12/12 |
| dec | 8,000 | 46.199 | 31.947 | +30.9% | 12/12 |
| factorial | 512 | 99.892 | 71.889 | +28.0% | 12/12 |
| fibonacci | 256 | 44.320 | 32.419 | +26.9% | 12/12 |
| bigslot | 64 | 18.634 | 12.531 | +32.7% | 12/12 |
| mmul | 16 | 17.864 | 12.821 | +28.2% | 12/12 |

### List operations

Each round constructs fresh lists of 80,001 through 80,012 items and reverses the order of the cases. Dojo checks the complete result against an independently constructed expected list after each timed operation. [Dojo inputs](bench-list-sharing.dojo)

| Operation | develop (ms) | Optimized (ms) | Less time | Faster pairs |
| --- | ---: | ---: | ---: | ---: |
| Concatenate two lists | 0.452 | 0.450 | +0.6% | 6/12 |
| Return each item | 2.048 | 0.271 | +86.8% | 12/12 |
| Replace item 1 | 2.486 | 0.478 | +80.8% | 12/12 |
| Replace item 40,000 | 2.460 | 0.651 | +73.5% | 12/12 |
| Replace item 80,000 | 2.501 | 0.972 | +61.1% | 12/12 |
| Update record 42 | 2.864 | 0.769 | +73.1% | 12/12 |
| Project a record field | 2.433 | 0.809 | +66.7% | 12/12 |
| Increment each atom | 2.199 | 0.713 | +67.6% | 12/12 |
| Remove item 42 | 2.204 | 0.497 | +77.4% | 12/12 |
| Keep every item | 2.062 | 0.277 | +86.6% | 12/12 |
| Keep only item 42 | 2.113 | 0.553 | +73.8% | 12/12 |
| Drop the first unit result | 3.391 | 0.983 | +71.0% | 12/12 |
| Remove alternate items | 14.444 | 10.438 | +27.7% | 11/12 |
| Add a captured value | 11.838 | 8.188 | +30.8% | 12/12 |

## Ship memory

Values are combined resident memory (MiB) of the Vere process and worker. Each entry is the median of three independent runs per build. A sampler reads `/proc/<pid>/smaps_rollup` approximately every 50 ms. Peak values are the largest samples in each run. Stable phases use the median of their samples. RSS counts a shared page in each process mapping it. The raw results also include proportional set size, private resident memory and swap.

| Workload | develop (MiB) | Optimized (MiB) | Less RSS |
| --- | ---: | ---: | ---: |
| Fresh fakezod, sampled boot peak | 2,487.0 | 2,359.3 | +5.1% |
| Fresh fakezod, 30 s after first Dojo check | 786.7 | 690.8 | +12.2% |
| Resumed fakezod at Dojo | 319.8 | 320.5 | -0.2% |
| 160,000-item map, filter and fold, sampled peak | 342.2 | 342.8 | -0.2% |
| After clearing those Dojo bindings | 342.2 | 342.9 | -0.2% |
| One 160,000-item atom list | 330.5 | 331.1 | -0.2% |
| 16 independent head edits held together | 451.1 | 331.2 | +26.6% |
| One 160,000-record list | 340.3 | 341.1 | -0.2% |
| 16 successive record revisions held together | 390.1 | 341.1 | +12.5% |

Fresh-boot time runs from the first memory sample after process launch to the first sample after a checked Dojo response. The fresh-boot idle window lasts three seconds and starts 30 seconds after the first checked Dojo response. The resumed workloads all start from the same stopped, develop-created checkpoint. Each pair runs sequentially on the same core, with the build order alternating across the three rounds. Each set of 16 revisions keeps the original input and all revisions live. The atom workload makes 16 distinct changes to the first item. The record workload repeatedly increments the first record while keeping the other 159,999 records. The held-memory sample precedes full noun equality checking, which can unify equal subtrees. These figures describe the sampled fakezod workloads.

## Correctness

- The upstream control and candidate agree on the canonical jam length and SHA-256 of the bootstrap core, all 138 cold replay products, and all 17 saved-checkpoint products. Each event product includes the effects and new Arvo core. Separate audit binaries perform the hashing outside the timing runs.
- Running the same differential test driver against upstream’s 32-bit runtime and the candidate’s 32-bit and 64-bit runtimes produces identical traces for 5,419 products and 5,095 expected exits. All three use memory and CPU debugging and runtime reference checks. The SHA-256 of the trace is `6c6ba3431c88493feb6ece2d722dc7b1388c27223db53c91d78216240c5315ae`.
- Upstream `develop` opens a checkpoint written by the optimized runtime and checks all 16 saved list revisions against their complete expected products.

## Reproduce the comparison

Use Bash, Python 3, Zig 0.15.2, Git, GNU `patch`, GNU coreutils and `taskset` on x86-64 Linux. Choose two available physical cores. The build helper exports both Git refs from the current checkout, applies the same opt-in compiler timing hook, and builds ReleaseFast for Linux musl with a 32-bit loom. Ship and replay commands run their usual entry points. The helper defaults to `origin/develop` and `HEAD` and checks that the selected upstream commit is an ancestor of the candidate.

```bash
git fetch origin develop
bench=$(mktemp -d -p "${XDG_CACHE_HOME:-$HOME/.cache}" vere-develop.XXXXXXXX)
tests/bench-develop-build.sh "$bench"
git clone https://github.com/urbit/benchmark.git "$bench/desk"
git -C "$bench/desk" checkout --detach ae0e498e19beba4ab585299adcbcce0427594e87
stock="$bench/develop/x86_64-linux-musl/urbit"
optimized="$bench/optimized/x86_64-linux-musl/urbit"
python3 tests/bench-develop-compiler.py "$stock" "$optimized" "$bench/desk" --cores 8 14
```

To rebuild the measured commits, pass `697fc95ddd ca2a0165a1` after the output directory. The optimized build enables `U3_DECISION_BENCH`, `U3_OWNER_BENCH`, `U3_REUSE_BENCH`, `U3_SPINE_BENCH`, `U3_DIRECT_EDIT_BENCH`, `U3_CUE_RANK`, `U3_NOUN_SHAPES`, `U3_GATE_SAMPLE` and `U3_LIST_SHARING`. The upstream build uses its defaults. [Build script](bench-develop-build.sh) · [Compiler driver](bench-develop-compiler.py)

For replay, supply an extracted, stopped pier. Set the expected first and last event numbers for that input. The driver makes independent scratch copies and checks that each process completes the expected range.

```bash
python3 tests/bench-develop-replay.py "$stock" "$optimized" "$cold_pier" \
  --full --first 6 --last 143 --cores 0 2 --scratch-dir "$bench"
python3 tests/bench-develop-replay.py "$stock" "$optimized" "$checkpoint_pier" \
  --first 233 --last 249 --cores 4 6 --rounds 6 --scratch-dir "$bench"
```

The two frozen event archives are local benchmark fixtures. The public compiler desk runs independently of them. The release pill supplies the same Arvo kernel input to both fresh boots. The executable revisions are the development commits identified above.

| Input | SHA-256 |
|---|---|
| `urbit-v4.6.pill` | `f8b08c53dd6d0aa9c4f7654f0a7bcd11e94089222888a3c99c0a10eedcbb377b` |
| `frozen-zod-143.tar.zst` | `3c29b3ad9b2acd2140acde430b72a9c53f4744d1ee5f158fc65550d8c7cb07d6` |
| `dojo-233-249.tar.zst` | `18a6f1a987435503601c05e6474c03c128c704c88528415d20ec1baff0b4fb78` |
