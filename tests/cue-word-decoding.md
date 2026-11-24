# Reading a noun descent in one word

A jam stream visits a cell before either of its children. Consecutive cell tags therefore describe a left descent, and the first atom or backreference ends that descent. In the preorder shape sequence, a cell adds one outstanding child slot and a leaf consumes one. The decoder works directly on the interleaved structure and payload bits in jam.

The bit reader loads up to eight bytes, compares the low bits with the repeating cell tag, and counts the matching pairs. It then uses the leaf’s unary prefix to recover its width. When the payload fits, the reader returns the entire descent, its leaf value, and the advanced cursor from one window. A wider atom leaves the cursor at its payload for the ordinary copy. Every open cell keeps its own original bit position, so a backreference can resolve only to a completed noun. Short final windows use bounded loads and the scalar reader handles incomplete headers. [Bit reader](../pkg/ur/bitstream.c) · [Noun decoders](../pkg/noun/serial.c)

Navarro and Sadakane develop navigable tree representations from balanced-parenthesis sequences. Vigna shows how to answer parenthesis queries inside a machine word. The adaptation here recognizes a run of jam tags and decodes its terminal leaf. It reads the existing wire format using a fixed-size window. [Fully-Functional Static and Dynamic Succinct Trees](https://arxiv.org/abs/0905.0768) · [Broadword Implementation of Parenthesis Queries](https://arxiv.org/abs/1301.5468)

## Live Dojo

Three fake zods resume copies of the same checkpoint. Each trial jams its input before timing, uses a fresh prefix, and compares the entire decoded value with the input. The table gives medians in milliseconds over twelve measured trials after warm-up. The builds take turns on one physical P-core and rotate execution order. [Dojo expressions](bench-cue-words.dojo) · [Trial data](cue-word-samples.jsonl)

| Timed operation | Upstream develop | Branch before | Candidate | Less time vs branch | Less time vs develop |
| --- | ---: | ---: | ---: | ---: | ---: |
| Cue 80,000 integers | 16.044 | 1.542 | 1.320 | 14.4% | 91.8% |
| Cue 80,000 adjacent pairs | 38.397 | 3.065 | 2.637 | 14.0% | 93.1% |
| Cue 80,000 references to a shared record | 8.024 | 1.692 | 1.342 | 20.7% | 83.3% |
| Map cue over 20,000 separate messages | 2.440 | 1.057 | 1.053 | 0.4% | 56.8% |
| Cue a one-million-byte payload with a prefix | 0.217 | 0.218 | 0.218 | 0.2% | −0.2% |

The candidate wins 12/12 integer-list trials, 11/12 adjacent-pair trials, and 12/12 shared-record trials. A second six-trial series runs the builds concurrently and rotates their cores. The corresponding branch reductions are 14.6%, 18.1%, 18.3%, and 5.5% on the first four workloads. The large atom takes 2.4% longer in that series. Its work consists almost entirely of copying the payload.

After twelve rounds, summed king and worker RSS is 214.4 MiB on develop, 216.5 MiB on the branch control, and 216.0 MiB on the candidate. Summed PSS is 212.0, 213.7, and 213.3 MiB respectively. These are post-workload process snapshots. The cold-reader peaks appear below.

## Stored event corpus

The corpus contains 143 records totaling 240,504,745 bytes. Of 3,768,580 cell openings, 37.8% belong to runs of two or more. Of the 2,316,757 atom tokens and 1,451,966 backreferences, 94.2% fit completely within an eight-byte load from the byte containing the leaf tag. The shape count treats a backreference as a leaf on the wire.

On an Intel Core i7-12700K, one warm-up and six measured trials rotate the three binaries across physical P-cores. Each trial uses a fresh process. The Nock gate and reusable pier decoder check every complete product against the original jam length and SHA-256 outside the timer. The field reader checks the two projected products against full upstream cue at axes 2 and 14, including index construction inside the timer. Six of six paired trials favor the candidate on full decode, and five of six on projection. [Corpus procedure](nock-cue.md) · [Driver](bench-cue-projection.py)

| All 143 records, ms | Upstream develop | Branch before | Candidate | Less time vs branch | Less time vs develop |
| --- | ---: | ---: | ---: | ---: | ---: |
| Registered Nock cue gate | 2,531.93 | 512.10 | 477.82 | 6.7% | 81.1% |
| Pier decoder | 2,061.62 | 411.33 | 361.83 | 12.0% | 82.4% |
| Two event fields | 2,147.26 | 239.01 | 192.96 | 19.3% | 91.0% |

Event 17 accounts for 199 MB of input. On the other 142 records, the candidate reduces branch time by 8.4% at the Nock gate, 14.0% at the pier decoder, and 25.8% on field projection. Peak process RSS for the branch and candidate rounds to the same 959 MiB, 656 MiB, and 314 MiB respectively. Corpus RSS includes the input buffer and verification.

A separate six-trial comparison at `389a55ff37` isolates descent batching. Its middle build uses the same word-based leaf reader and short-window handling, returning exactly one opening at a time. Full descent batching reduces the middle build’s median from 155.08 to 145.19 ms on field projection (6.4%) and from 454.60 to 444.33 ms on Nock cue (2.3%). The single-tag build also improves on the branch control, which measures 183.03 ms for field projection and 478.43 ms for Nock cue in the same rounds. These comparisons share one set of rounds within each row.

## Checkpoint replay

Events 233–249 run from independent copies of the same stopped checkpoint. The timer includes event processing and disk I/O. Each process checks the event range, completion and mugs. The table gives six-trial median wall times for the final build. [Replay driver](bench-develop-replay.py)

| Schedule | Develop | Branch control | Candidate | Time vs branch | Candidate faster in |
| --- | ---: | ---: | ---: | ---: | ---: |
| Concurrent, three physical cores | 15.26 s | 14.86 s | 15.39 s | +3.6% | 4/6 trials |
| Sequential, one physical core | 5.21 s | 4.81 s | 5.20 s | +8.1% | 3/6 trials |

## Correctness and reproduction

The token differential checks every 16-bit prefix at eight starting offsets and three buffer lengths against the scalar tag, length, and bit readers. It checks the decoded fields, error class, next byte pointer, bit cursor and remaining length. Directed cases cover long runs, long unary prefixes, physical buffer ends and truncated input. The focused token test passes AddressSanitizer, LeakSanitizer and UndefinedBehaviorSanitizer on x86-64. It also passes on native 32-bit x86 and under QEMU on AArch64 and big-endian s390x. The full ur suite passes its assertions and reports the same 410 bytes in 44 existing leaked allocations on both the branch control and the candidate. [Differential tests](../pkg/ur/tests.c)

The full decoders also check every one- and two-byte input (65,792 streams) against the independent ur arena. For the atom entry point, each integer uses its minimal byte length. The suite checks exact values, Nock bail classes and reopening the same external decoder and view. The pier reader releases pending frames on failure. The scalar arena uses the effective value to select direct atom representation when a declared payload includes leading zeros. [End-to-end short input tests](../pkg/noun/serial_tests.c)

The serializer, Nock and decision suites run with memory and CPU debugging on both 32- and 64-bit looms. Both loom widths also match upstream on every complete corpus product and two-field projection. Both produce the same reference-checking trace as upstream for 5,419 values and 5,095 expected exits, with SHA-256 `6c6ba3431c88493feb6ece2d722dc7b1388c27223db53c91d78216240c5315ae`.

The upstream and candidate audit manifests match byte for byte for the bootstrap core, 138 cold-replay event products and 17 checkpoint event products. The manifests cover 33,279,069,693 bytes of canonical jam and record the length and SHA-256 of each product.

Build upstream `697fc95ddd`, the branch control `21b3755306`, and the candidate `88b71b7083` with Zig 0.15.2, ReleaseFast, on a 32-bit loom. The two branch builds enable `U3_DECISION_BENCH`, `U3_OWNER_BENCH`, `U3_REUSE_BENCH`, `U3_SPINE_BENCH`, `U3_DIRECT_EDIT_BENCH`, `U3_CUE_RANK`, `U3_NOUN_SHAPES`, `U3_GATE_SAMPLE`, `U3_LIST_SHARING`, `U3_CUE_VIEW`, and `U3_JAM_CLASSES`. The word reader runs directly through both ordinary cue entry points. Build the same `cue-bench` driver against all three libraries using the upstream harness procedure. [Upstream harness](nock-field-benchmarks.md)

```bash
for mode in nock xeno view; do
  upstream=$mode
  if [[ $mode == view ]]; then upstream=project; fi
  python3 tests/bench-cue-projection.py "$develop" "$branch" "$candidate" \
    "$ivory" "$inputs"/event-*.jam --modes "$upstream" "$mode" "$mode" \
    --cores 2 6 10 --rounds 6
done
```

For checkpoint wall time, run the following on a stopped fixture.

```bash
python3 tests/bench-develop-replay.py "$develop_urbit" "$branch_urbit" "$pier" \
  --candidate "$candidate_urbit" --cores 2 6 10 --rounds 6 --first 233 --last 249
```

For the sequential replay series, use `--serial --cores 4` in place of the three cores.

Each corpus command checks the ordered products across builds. In the sample file, trial zero is the warm-up for the corpus, ablation and Dojo series. Replay timing includes every listed trial. [Individual samples](cue-word-samples.jsonl)
