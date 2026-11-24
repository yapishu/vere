# Nock slot reads and gate edits on Vere 4.6

The two changes reduce median Hoon compilation time by 3.9% and prelude compilation time by 4.0% on top of the previous optimized build. Seven workloads running in Dojo take 5.6–14.1% less time. Against stock Vere 4.6, the combined build takes 22.0% less time for Hoon and 22.1% less for the prelude.

Both changes operate on Nock bytecode that Vere already emits. The comparison baseline is Vere 4.6 with indexed decisions, subject ownership and mutable cell reuse enabled. The experimental build adds `U3_SPINE_BENCH` and `U3_DIRECT_EDIT_BENCH`. [Previous ownership measurements](nock-ownership.md)

## What the compiler workloads do

One instruction-pointer profile of each whole process, including preparation and output, attributes 10.9% of Hoon samples and 11.8% of prelude samples to the short-axis walker. A separate count finds 347,973,418 and 363,154,885 short-axis reads respectively. Across these reads, 96.7% and 96.5% either have fewer than four direction bits or consist of a run of tails followed by four direction bits. Axes 6, 14, 510 and 30 account for over 100 million reads in the Hoon input.

For the 207 axes in this family, a read-only 64 KiB table maps the axis to a small integer. The VM dispatches once, follows the prescribed number of tails and reads the remaining head or tail fields at constant offsets. Every hop checks that its input is a cell. Other short axes use the ordinary walker. The table lives outside the loom and contains only path identifiers. GNU `size` reports an increase of 71,104 bytes of text and read-only data in the combined release build.

For edits at axes 2, 3 and 6, the VM already checks whether it owns the target exclusively on the current road. The direct edit uses that fact to overwrite the selected field and clear the affected mugs. At axis 6 it checks the payload separately. A shared payload gets a replacement cell that retains its context. Both changes preserve the bytecode format.

## Live ships

The workers load the seven verification inputs from the public desk. Every call checks the full expected result. A batch runs once for warm-up and eight times per worker for measurement. The workers exchange physical performance cores and reverse workload order. Each row shows median batch times. The separate-build comparison restarts the disposable ships from their checkpoints. The Hoon and Nock expressions are the same in both comparisons. [Pinned benchmark desk](https://github.com/urbit/benchmark/tree/ae0e498e19beba4ab585299adcbcce0427594e87)

| Workload | Calls per batch | Separate builds | Same binary, off → on | Faster pairs, separate builds |
|---|---:|---:|---:|---:|
| ackermann | 1 | 711.77 → 614.94 ms (−13.6%) | 715.44 → 629.41 ms (−12.0%) | 8/8 |
| add | 2 | 53.49 → 47.23 ms (−11.7%) | 54.17 → 46.97 ms (−13.3%) | 8/8 |
| dec | 8,000 | 45.62 → 40.56 ms (−11.1%) | 46.46 → 41.36 ms (−11.0%) | 7/8 |
| factorial | 512 | 99.75 → 94.19 ms (−5.6%) | 100.22 → 91.60 ms (−8.6%) | 7/8 |
| fibonacci | 256 | 43.52 → 40.95 ms (−5.9%) | 43.36 → 39.54 ms (−8.8%) | 7/8 |
| bigslot | 64 | 18.86 → 16.51 ms (−12.4%) | 18.54 → 16.30 ms (−12.1%) | 7/8 |
| mmul | 16 | 18.49 → 15.89 ms (−14.1%) | 17.87 → 15.96 ms (−10.7%) | 8/8 |

Two extra ship checks exercise different subjects. Decrementing 1,000,000 takes 53.59 → 46.54 ms (−13.2%). Eight checked sieve calls with subject 8 take 498.20 → 430.25 ms (−13.6%).

In the isolated loop that carries its uniquely owned gate into the next call, the median changes from 17.83 to 11.37 ns/call. Enabling direct edits alone takes 12.53 ns/call. The loop checks the counter and both retained fields.

## Full compiler workloads and startup

Each compiler trial starts a fresh process, loads the matching ivory compiler, evaluates the exact upstream jamfile, then checks the published input and product mugs and the full jammed product. Only Nock evaluation is timed. The separate builds run eight paired trials across two pairs of physical performance cores. The same-binary comparison uses four paired trials. Each comparison swaps the workers between cores and reverses workload order.

| Compiler input | Separate builds | Same binary, off → on | Faster pairs, separate builds |
|---|---:|---:|---:|
| hoon | 20.96 → 20.14 s (−3.9%) | 21.89 → 20.60 s (−5.9%) | 7/8 |
| prelude | 21.78 → 20.91 s (−4.0%) | 24.40 → 22.84 s (−6.4%) | 7/8 |

On cores 8 and 10, the four-pair median changes by −3.5% for Hoon and −3.6% for the prelude.

On cores 4 and 6, the four-pair median changes by −4.3% for Hoon and −4.0% for the prelude.

Two crossed pairs replay the frozen fakezod through event 143. Mean elapsed time is 370.10 → 352.53 seconds (−4.7%). The within-pair changes are −4.8%, −4.7%. Both builds peak at about 1.14 GiB of resident memory, within 56 KiB in each pair. Scratch is disk-backed. The machine is shared and the trial count is small.

## Direct comparison with stock Vere

This comparison includes the indexed compiler and both rounds of slot and ownership changes. The stock process runs the release interpreter with the same timing hook. Both evaluate the published jamfile with its verification subject in a fresh process on each of four crossed pairs. Short workloads include bytecode compilation in their Nock time. These single-call numbers and the Dojo batch timings measure different work.

| Workload | Stock 4.6 median | Combined median | Elapsed-time change | Faster pairs |
|---|---:|---:|---:|---:|
| ackermann | 789.185 ms | 501.074 ms | −36.5% | 4/4 |
| add | 30.172 ms | 19.126 ms | −36.6% | 4/4 |
| dec | 0.023 ms | 0.019 ms | −19.9% | 4/4 |
| factorial | 0.234 ms | 0.171 ms | −27.2% | 4/4 |
| fibonacci | 0.201 ms | 0.151 ms | −25.2% | 4/4 |
| bigslot | 0.337 ms | 0.221 ms | −34.5% | 4/4 |
| mmul | 1.231 ms | 0.854 ms | −30.6% | 4/4 |
| hoon | 25.911 s | 20.204 s | −22.0% | 4/4 |
| prelude | 26.749 s | 20.844 s | −22.1% | 4/4 |

## Cache comparison

A trial VM keeps eight entries for reads of fields from a core context. Keying only on a senior context produces four hits during the full Hoon compilation. Keeping references to both the context and result produces 36,833,673 hits, but median compilation time is 20.3% longer than baseline.

## Correctness

The short-axis check evaluates all 65,535 positive 16-bit axes on a tree with distinguishable leaves. A consuming slot and an autocons that retains the subject produce 131,070 checked results. For every specialized path, a truncated subject at each possible hop gives the expected Nock exit, totaling 1,858 exits. The 32-bit and 64-bit workspace builds pass with memory debugging and reference checking. The release build from the supplied patch passes the same checks.

The 10,255-case differential suite matches the stock transcript. It checks complete products, expected failures, warmed mugs, aliased roots and payloads, and edits that put an old root into the replacement value. All 63 public Nock reference vectors pass. The startup audit matches the bootstrap core and every one of the 138 subsequent event products against stock, including jam lengths and SHA-256 over 29,203,773,406 bytes. The saved Dojo checkpoint matches stock on all 17 products of events 233–249, covering 4,074,710,338 bytes.

Measurements use an Intel Core i7-12700K, Ubuntu 26.04.1 LTS, Linux 7.0.0-42-generic, Zig 0.15.2, ReleaseFast and a 32-bit loom. Other tasks run on the host. One exploratory three-build comparison shares a physical core with the checkpoint audit in its last round. The reported compiler medians use separate measurements. The published verification subjects for atomcat, atomdog, atomgmp, SHA-512 and sieve exceeded the 30-second limit in the earlier baseline measurements. The sieve measurement above uses a smaller subject.

The next experiment measures a rank index for decoding stored nouns, including fresh fakezod boot and replay. [Cue measurements](nock-cue.md)
