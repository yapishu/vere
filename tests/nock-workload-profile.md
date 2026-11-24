# Where Nock spends CPU time

On branch control `6e828eb179`, reference counting and slot lookup together account for 36.7% of the CPU samples during ship replay and 27.1% during local application reads with three populated groups. The largest individual symbol is the Nock interpreter. Its samples include all the opcode work that the C compiler inlines into it. [Individual measurements](nock-workload-profile.jsonl)

| Workload | Samples | Interpreter | Gain and lose | Slot lookup | Noun equality |
|---|---:|---:|---:|---:|---:|
| Ship replay, events 6–143 | 51,488 | 23.4% | 19.9% | 16.8% | 6.9% |
| Compile Hoon | 3,233 | 26.3% | 21.5% | 15.9% | 5.5% |
| Compile Prelude | 3,297 | 25.9% | 22.2% | 16.7% | 5.2% |
| Live Dojo desk status | 1,496 | 24.5% | 14.8% | 12.0% | 4.7% |
| Live HTTP app reads, empty collections | 1,835 | 24.4% | 14.2% | 13.3% | 5.6% |
| Live HTTP group and channel reads, populated | 1,511 | 26.6% | 14.4% | 12.8% | 3.6% |

The replay starts its sampling window after bootstrap and includes the kernel update and desk rebuild. The resumed local fakezod handles 48 `+vats` requests, each followed by an arithmetic sentinel that returns 321. A separate worker run serves 96 authenticated HTTP scries each to `groups/v3/groups`, `groups/v1/groups`, `channels/v2/channels` and `contacts/all`. All 384 replies equal the expected empty JSON objects. For a populated comparison the groups desk creates three local groups, each with a chat channel. A further 288 authenticated scries across the two group endpoints and the channel endpoint reproduce the complete expected JSON on every request. Both runs profile the ship worker. The populated fixture exercises group and channel metadata without message history. A checkpoint replay of events 233–249 yields just 12 in-window samples.

The sampler interrupts the worker or evaluation process on randomized process-CPU-clock intervals and maps the x86-64 instruction pointer to the linked text symbol. Intervals are 3–6 ms for replay and compilation and 1–3 ms for live requests. Signal delivery can land at a syscall boundary, especially between short requests. The percentages describe exclusive sample locations. The CPU-clock trigger avoids attributing long idle intervals as CPU usage. The interpreter column is `_n_burn`. Gain and lose covers `u3a_gain`, `u3a_lose` and both road-specific release routines. Slot lookup covers `_n_path3` and `u3r_at`. Noun equality is `u3r_sing_imp`.

## A union whose result is already available

The native set-union wrapper runs 340,966,108 times during events 6–143. Both inputs are empty once, only the left input is empty 109,752,548 times, only the right is empty 131,648,021 times, and the two nonempty operands have the same noun reference 99,564,769 times. Just 769 calls have two nonempty, distinct references. Those operands may still be structurally equal. These exact counts cover entry into the jet and exclude recursive calls inside its implementation. Hoon compilation has 50,567 entries in total and Prelude 58,786. A separate per-event pass reproduces the exact total. Event 16 alone accounts for 340,894,829 calls (99.98%). Its jammed job is a Clay `%park` on wire `/c/sync` for the `%base` desk. The other 137 events together account for 71,279 calls. This case measures provisioning and rebuilding a desk. The live application workloads exercise a different part of the runtime.

The jet already returns an input in all four identity cases. The hypothesis is that the cost is preparing and dispatching the call. A prototype looks at an axis-6 sample edit followed by a kick. It checks that the site still resolves to the enabled, trusted set-union jet at axis 2 and that its registered parent lies at axis 7, independent of the replacement sample. With the normal registration check satisfied, it can read the captured set at axis 30, inspect the new sample and transfer the correct input to the result slot. Other cases take the ordinary call. This changes no noun layout and adds no cache. [Set-union jet](../pkg/noun/jets/d/in_uni.c) · [Jet dispatch and registration](../pkg/noun/jets.c)

The shortcut fires 340,965,336 times during cold replay. Two full-replay pairs on separate physical cores take 283.32 → 279.61 s and 284.46 → 278.66 s, respectively (1.3% and 2.0% less elapsed time). The pairs overlap in time on a shared machine. The first pair pins control and candidate to cores 14 and 12, and the second uses 6 and 8. All runs finish through event 143. This measures the effect of skipping the final edit and kick while retaining the cost of producing both operands and validating the site.

Across four paired compiler rounds, Hoon takes 16.77 → 17.05 s (+1.6%) and Prelude 17.92 → 18.07 s (+0.9%). Hoon takes the shortcut 47,247 times and Prelude 55,361 times. Both reproduce the upstream compiler bytes. On the populated fakezod, a batch of 96 authenticated group and channel scries takes a median 1.71 → 1.76 s across three alternating pairs on the same core. The direction varies between pairs. All 576 responses match the expected JSON, including all three local groups and channels. Median combined king and worker PSS just after the batch is 299.2 MiB on control and 299.0 MiB on the prototype. Each per-run memory value is the median of five samples.

The same differential test driver produces the upstream 10,514-case trace byte for byte on both loom widths, covering 5,419 products and 5,095 expected exits. Its SHA-256 is `6c6ba3431c88493feb6ece2d722dc7b1388c27223db53c91d78216240c5315ae`. The 32-bit checkpoint replay reproduces upstream's canonical lengths and SHA-256 hashes for effects and cores for all 17 events (4,074,710,338 bytes), with zero shortcut hits. A separate cold audit selects events with at least 100,000 shortcut hits, up to two such events, and always includes the bootstrap and final event. Only event 16 qualifies, with 340,894,341 hits. Its complete 24,861,721-byte product, the bootstrap core and the 239,705,535-byte final event product all match upstream by length and SHA-256. The other cold events receive the normal replay mug checks.

## What happens to a newly constructed gate

A Nock gate contains two cells, `[battery [sample context]]`. The constructor probe samples roughly one in 4,096 executions of the fused literal-battery constructor. Starting from the actual construction site, it follows the stack position of the result through up to 80 bytecode instructions, including the continuation in the caller when the constructor returns. It stops when an operation can observe the value or when it cannot establish what happens next. The table counts cases where the first identified use on normal return replaces the sample. An intervening call can still bail.

| Workload | Constructions sampled | First identified use replaces sample | Of those, cross an intervening call |
|---|---:|---:|---:|
| Ship replay | 299,102 | 282,860 (94.6%) | 182,161 (64.4%) |
| Hoon | 24,395 | 14,389 (59.0%) | 3,093 (21.5%) |
| Prelude | 25,150 | 15,084 (60.0%) | 3,107 (20.6%) |

The delayed-construction prototype carries the battery and context separately across an interpreted factory return, releases the default sample and builds the two cells at the caller's proven sample edit. It admits one pending gate per interpreter invocation, requires an unhinted return and examines at most 32 instructions per candidate. During Hoon evaluation, 12,455,011 gates take this path, and all reach the designated edit. The Prelude count is 13,053,715. Both products equal the reference bytes.

An independent probe observes actual sample edits followed immediately by a kick. Hoon can already edit both gate cells in place in 15,140 of 16,612 such samples (91.1%). In Prelude the corresponding count is 15,740 of 17,234 (91.3%), and in replay it is 375,710 of 472,922 (79.4%). Delaying allocation until the edit still pays to construct two cells. Across all these Hoon kicks, 5,970 resolve to native jets. A separate sample restricted to gates the prototype actually delays finds that **all 2,968 Hoon consumers and all 3,120 Prelude consumers enter interpreted arms**. The populations answer different questions.

## Which changes pay for themselves

Each percentage is the change in median evaluation time against the branch control in the same series. Positive values mean the prototype takes longer. Each trial starts fresh processes on distinct physical performance cores, rotates the cores between rounds and alternates workload order. The gate and slot caches share a four-way comparison. The road and delayed-gate experiments each have their own paired control. Every timed compiler output equals the complete reference jam byte for byte. The reference SHA-256 digests also match the pinned upstream develop compiler runs. [Upstream results](develop-bench-samples.jsonl)

| Prototype | Hoon | Prelude | Rounds per workload |
|---|---:|---:|---:|
| Separate interpreters for north and south roads | −0.2% | +0.3% | 2 |
| 16-entry cache for constructed gates | +9.5% | +8.8% | 4 |
| Recognize the entire factory and cache its result | +4.4% | +4.6% | 4 |
| Cache long slot reads anchored in a senior noun | +2.8% | +1.3% | 4 |
| Delay gate construction until the caller sets the sample | +6.2% | +6.3% | 4 |

The first gate cache records 28,278,221 hits in 99,988,384 Hoon constructions. Recognizing the entire factory records 21,947,142 hits in 60,460,679 calls. The end-to-end measurements include the cost of looking up and retaining these nouns. The slot cache sees 120,387,858 long reads with the chosen axis prefix and only four hits under its senior-anchor lifetime rule. Compiler peak RSS varies by less than 0.2 MiB between each prototype and its paired control. These experiments do not measure retained memory on a long-lived ship.

Two full-replay spot checks include bootstrap and the final save, using fresh copies of the same event log. The factory cache takes 279.2 s against its control at 273.3 s. Delayed construction takes 282.8 s against its control at 272.4 s. All four runs reach event 143 with the normal replay mug checks enabled. Each comparison has one observation per build.

## Carry the fields into the receiving arm

The next experiment is to compile both ends of a Nock call together. At a factory return, the caller's bytecode can specify which stack entry represents the gate, where its real sample arrives and which arm consumes it. The VM can then resolve slots in the receiving arm from the battery, sample and context. If the callee only reads those fields, the gate's two wrapper cells need never exist. A consumer that returns, stores, registers or otherwise needs the whole gate receives the ordinary noun. A compile-time plan also removes the prototype's repeated bytecode scan. When an arm conditionally returns one of its input fields, that fact can propagate back to the caller. Allocations can then disappear on both sides of the call. Skipping evaluation of an operand additionally requires proving that it cannot bail or produce an observable hint.

The receiving arms suggest where to start. Of 2,968 sampled Hoon consumers on the delayed path, 1,414 begin by projecting a slot with `FABK` or `FASK`. Another 1,244 begin with the same four instructions, `LIT0 SNOL LIBK/0 SWAP`, the prefix of a constructor. The latter can capture the first gate as the context of a second gate. Following that nested use can determine whether either wrapper escapes. The existing optional field-entry experiment already decodes a guarded projection prefix in the receiving arm. A plan spanning the factory return, intervening calls and this prefix can reuse that analysis. [Existing field-entry experiment](nock-field-benchmarks.md) · [Partial escape analysis and scalar replacement](https://ssw.jku.at/Research/Papers/Stadler14/Stadler2014-CGO-PEA.pdf)

The decisive measurement is how many wrapper cells are eliminated through the receiving arm, followed by elapsed time on the same compiler, replay and live app workloads. Observable hints, jet registration and dispatch, bail paths and noun ownership across roads constrain where the VM must reconstruct the core. Full canonical replay products and both loom widths are the correctness checks for a runtime change. The timing experiments use the 32-bit loom.

The machine is a shared Linux i7-12700K. All builds use Zig 0.15.2, ReleaseFast, x86-64 musl, the public compiler fixture and the same evaluation timer and phase counters. The enabled feature macros have the `U3_` prefix and suffixes `DECISION_BENCH`, `OWNER_BENCH`, `REUSE_BENCH`, `SPINE_BENCH`, `DIRECT_EDIT_BENCH`, `CUE_RANK`, `NOUN_SHAPES`, `GATE_SAMPLE`, `LIST_SHARING`, `CUE_VIEW` and `JAM_CLASSES`. The gate-cache, slot-cache and delayed-construction timing builds omit their hit counters. The guarded union timing build increments one counter per shortcut. [Compiler procedure and fixtures](develop-benchmarks.md) · [Replay driver](bench-develop-replay.py)

## Repeat the CPU profile

On Linux, the sampling driver launches the static Vere binary as its child. It requires Python 3 and binutils (`nm` and `readelf`). Choose a physical core and a disposable copy of the frozen input pier. The phase markers below include replay of events 6–143 and skip bootstrap and checkpoint saving. JSON output contains the sample count per symbol. As a cross-check, the standalone driver takes 3,473 in-window Hoon samples and reproduces the full reference jam. [Sampling driver](profile-nock-cpu.py)

```sh
taskset -c 8 python3 tests/profile-nock-cpu.py \
  --start 'playback starting' --end 'playback complete' \
  --output /tmp/vere-replay-cpu.json /path/to/urbit play -y -f /path/to/pier-copy
```
