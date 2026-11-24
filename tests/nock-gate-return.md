# Returning a gate as fields

Returning a Nock gate as its battery, sample and context removes two wrapper cells whenever the receiving arm can use those fields directly. On the frozen ship log this eliminates 187,552,792 cell constructions during events 6–143. The branch control finishes compilation and the populated app workload sooner. The implementation is available as an experimental patch. [Gate-return patch](bench-gate-return.patch) · [Measurements](nock-gate-return-samples.jsonl)

| Workload | Develop | Branch | Gate return | Time change vs branch |
|---|---:|---:|---:|---:|
| Compile Hoon | 23.26 s | 16.93 s | 18.29 s | +8.0% |
| Compile Prelude | 24.74 s | 18.25 s | 18.96 s | +3.9% |
| Cold replay, pair 1 |  | 302.25 s | 309.11 s | +2.3% |
| Cold replay, pair 2 |  | 293.36 s | 313.29 s | +6.8% |
| 96 populated app reads |  | 1.740 s | 1.871 s | +7.5% |

The compiler comparison uses three rounds on rotating physical cores with fresh processes and the same timed Nock evaluation on each build. All outputs equal the complete upstream jam bytes. The two replay pairs use fresh copies of the frozen log and exchange cores 0 and 10. Each run includes bootstrap, normal event mug checks and the final save. Times are wall seconds on a shared i7-12700K. The pinned upstream commit is `697fc95ddd` and the branch control is `af8b22b03a` with runtime `6e828eb179`.

The HTTP comparison uses six alternating pairs on the same core. A private, resumed fakezod serves 32 cycles of authenticated reads of `groups/v3/groups`, `groups/v1/groups` and `channels/v2/channels`. The fixture contains three groups and their chat channels. All 1,152 responses match the expected content, with one common digest for the full group and channel metadata across builds. Combined king and worker PSS after the batch is 299.6 MiB on the branch and 299.3 MiB with gate return. Each run contributes the median of five samples. Hoon compiler peak RSS differs by 4 KiB between branch and candidate in the three-way compiler comparison.

## Where the cells disappear

| Evaluation phase | Gates deferred | Interpreted entries | Native entries | Entries across another head kick | Wrapper cells avoided |
|---|---:|---:|---:|---:|---:|
| Compile Hoon | 26,871,300 | 1,266,090 | 7,815,052 | 496,163 | 18,162,284 |
| Cold events 6–143 | 273,317,208 | 11,918,930 | 81,857,466 | 1,722,085 | 187,552,792 |
| Checkpoint events 233–249 | 2 | 0 | 0 | 0 | 0 |

The compiler follows the gate's stack position from a head kick to an axis-6 edit and the subsequent call. One byte in the call-site alignment gap records that distance. A second byte in the program header identifies a factory that constructs a literal battery and default sample. A caller below 16 MiB of bytecode can also encode the edit distance in its saved instruction pointer so that an arm with an arbitrary prefix can forward its terminal constructor. The ordinary operand stack owns the context. The literal table owns the battery, and the interpreter reads its slot after any intervening call. A single pending gate is allowed per interpreter invocation.

At the consuming kick, a battery and registration check guards entry into an interpreted arm or one of nine native jets. The former reads its projections from the constituent fields. The latter uses its existing noun primitive. An arm that needs the entire gate receives the materialized noun. A failed sample takes the normal Nock exit path. Native entry requires a live, trusted jet and uses the ordinary path under memory debugging. Tracing and CPU profiling use ordinary dispatch. The approach draws on work on returning constructed products and on allocating an object only along the paths that need it. [Constructed product result analysis for Haskell](https://www.microsoft.com/en-us/research/wp-content/uploads/2016/07/cpr.pdf) · [Partial escape analysis and scalar replacement](https://ssw.jku.at/Research/Papers/Stadler14/Stadler2014-CGO-PEA.pdf)

The count of cells avoided is twice the sum of successful interpreted and native entries. The PSS measurements above capture the memory retained by the ship. The terminal-constructor extension defers 4,762,413 additional Hoon gates. Comparing the counter builds shows the same number of successful interpreted and native entries with and without it. Gate counters use phase differences from dedicated counting builds.

| Change | Hoon time vs same-series control | Prelude time vs same-series control |
|---|---:|---:|
| Interpreted arms | +1.2% | +0.8% |
| Interpreted arms and nine jets | +2.1% | +2.1% |
| Direct execution of the factory alone | +0.2% | +2.5% |
| Complete factory, field return and fused sample load | +4.4% | +4.6% |
| Also forward from a terminal constructor | +5.9% | +8.5% |

Each row uses four compiler rounds. The first two variants share a three-way series and the last three share a four-way series. In a separate Hoon CPU profile, allocation in `u3a_celloc` accounts for 2.5% of branch samples and 2.1% of gate-return samples. Decoding instruction arguments, selecting a sample and validating its registered parent together account for 2.5% of gate-return samples. These are exclusive symbol samples. Inlined work falls under the interpreter. The existing branch already edits most eligible sample cells in place. [Branch CPU profile and in-place edit counts](nock-workload-profile.md)

## Shared prefixes in subject reads

A branch-control probe counts pointer steps in positive, direct fragment instructions with axes through 65,535. It also records two adjacent reads from the same live subject when a stack swap separates them or when a test consumes the first field and branches to the second read. It counts the leading directions shared by their two axes. The totals describe adjacent pairs, so a run of three reads can contribute twice to the common-prefix column.

| Workload | Fragment pointer steps | Common steps across a swap | Common steps across a taken or untaken test | Sum / total steps |
|---|---:|---:|---:|---:|
| Hoon | 1,667,266,038 | 73,361,722 | 283,400,704 | 21.4% |
| Prelude | 1,747,247,087 | 75,584,966 | 297,301,461 | 21.3% |
| Cold replay | 28,460,445,398 | 725,019,543 | 6,828,912,285 | 26.5% |

A separate prototype keeps a borrowed ancestor in the interpreter while evaluating these adjacent reads. The experiment handles swaps and short cell, zero and one tests. Its four paired compiler rounds give +2.8% elapsed time for Hoon and +0.3% for Prelude. One cold replay pair takes 286.59 s on the branch and 290.04 s on the prototype. The common-step count covers more patterns than this implementation and is an opportunity estimate. A compiler that selects a common ancestor when emitting the bytecode could eliminate the repeated run-time pattern checks. [Shared-prefix prototype](bench-nock-shared-prefix.patch)

## Product and checkpoint checks

Both gate-return loom widths reproduce the upstream decision trace over 10,514 cases and the new trace over 360 targeted cases (272 products and 88 exits). The targeted tests exercise both road orientations, a changing battery, an escaping gate, arbitrary producer work and calls that compute or fail to compute the sample. The checked run also enables the existing field-entry optimization, and the debug build enables CPU and memory debugging. A counter assertion checks that the optimized receiver runs across an intervening head kick.

The 32-bit gate-return replay matches upstream's canonical length and SHA-256 for the bootstrap core, all 138 cold event products and all 17 checkpoint event products. This covers 33,279,069,693 bytes across 156 entries. A copy of the populated checkpoint written by the candidate opens under develop and serves all 96 reference HTTP responses. The samples file contains both manifest hashes and the differential trace hashes.

## Build the experiments

Apply either patch to `af8b22b03a`. The measurements use Zig 0.15.2 in ReleaseFast on x86-64 Linux musl. Alongside the branch flags listed in the profile, gate return uses `-Dcopt=-DU3_GATE_RETURN -Dcopt=-DU3_GATE_RETURN_NATIVE -Dcopt=-DU3_GATE_RETURN_FUSE -Dcopt=-DU3_GATE_RETURN_SUFFIX`. Omit the suffix flag for complete factories only. Enable `U3_GATE_FACTORY_ONLY` alongside the fusion flag to time direct factory construction alone. `U3_GATE_RETURN_STATS` enables the phase-report function and the focused-test hit check. Run `U3_GATE_RETURN_CHECK=1 U3_CASE_TRACE=1 path/to/decision-test` on either loom width. The independent shared-prefix patch uses `-Dcopt=-DU3_PREFIX_PAIR` with the same branch flags. To collect the slot-walk counts, build the branch with the independent flag `U3_PROJECTION_PROBE` and call `u3n_axis_pair_report` at the evaluation phase boundaries. Both report functions take a phase string such as `before` or `after`. [Baseline build flags](nock-workload-profile.md) · [Compiler measurement hook](bench-desk-eval.patch) · [CPU sampler](profile-nock-cpu.py)
