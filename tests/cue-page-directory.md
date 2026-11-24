# A sparse bitmap directory for cue

Across 143 ship events, the directory cuts the median time to read the timestamp and event tag from 175.99 ms to 99.24 ms. The timer includes building and validating the index. The other 142 records take 39.0% less time when the 199 MB event is excluded. [Samples](cue-page-directory-samples.jsonl)

## How the lookup works

Jam visits each cell before its children. An atom payload can separate adjoining tree nodes by millions of bits, while a backreference names an earlier node by its exact bit position. In event 17, 285,805 occupied 512-bit pages form 15,825 runs. Only 5,059 of its 1,330,864 backreferences land in the dense prefix where the page number directly gives the packed page slot.

The directory keeps that prefix directly addressable. It numbers subsequent pages relative to the first occupied page after the gap. Each six-bit digit selects a bit in a 64-child node. A missing bit rejects the lookup. Counting the set bits below a present bit gives the child's offset from the node's base. Jam supplies starts in increasing order, so children can be appended to contiguous arrays at each level, and a new root can cover a larger range as the input arrives. Event 17 uses four levels and 8,688 interior nodes. Its occupied pages and directory entries total 26.30 MiB, compared with 37.07 MiB for the branch page entries. These sizes exclude unused capacity and noun storage. On x86-64 each leaf page occupies 96 bytes, down from 136, by keeping one absolute noun ordinal and eight 16-bit within-page offsets. A successful lookup yields a noun slot. The decoder accepts the reference only after the target noun is complete. [Implementation](../pkg/noun/cue_pages.inc)

The tree-navigation idea here is rank on the positions of preorder nodes. The specific lookup technique has precedent in Asai and Ohara's Poptrie, which selects packed children with bitmap population counts for IP routing. Vere uses the physical offsets from jam and incrementally indexes only occupied pages. [Poptrie paper](https://conferences.sigcomm.org/sigcomm/2015/pdf/papers/p57.pdf)

## Real event records

One warm-up precedes six trials. All three binaries take turns on the same physical P-core in rotating order. A fresh process reads the same 143 records in sequence each time. The full decoders re-encode and hash every result outside the timer. The field reader compares its ordered results to full upstream cue followed by axis selection. [Benchmark driver](bench-cue-projection.py)

| Median elapsed, ms | Upstream develop | Branch before | Directory | Time vs branch | Faster trials vs branch |
| --- | ---: | ---: | ---: | ---: | ---: |
| Timestamp and event tag, axes 2 and 14 | 2,371.96 | 175.99 | 99.24 | 43.6% less | 6/6 |
| Reusable pier decoder | 2,407.53 | 393.88 | 323.19 | 17.9% less | 5/6 |
| Registered Nock `+cue` gate | 3,281.86 | 517.33 | 433.22 | 16.3% less | 6/6 |

| Other 142 records, ms | Upstream develop | Branch before | Directory | Time vs branch |
| --- | ---: | ---: | ---: | ---: |
| Timestamp and event tag, axes 2 and 14 | 92.87 | 7.99 | 4.88 | 39.0% less |
| Reusable pier decoder | 89.33 | 23.02 | 19.76 | 14.2% less |
| Registered Nock `+cue` gate | 75.58 | 34.76 | 30.89 | 11.2% less |

| Median peak process RSS, MiB | Upstream develop | Branch before | Directory | RSS vs branch |
| --- | ---: | ---: | ---: | ---: |
| Timestamp and event tag, axes 2 and 14 | 1,372.6 | 314.4 | 303.5 | 3.5% less |
| Reusable pier decoder | 1,372.6 | 656.3 | 645.4 | 1.7% less |
| Registered Nock `+cue` gate | 1,105.5 | 958.8 | 947.2 | 1.2% less |

RSS includes the input buffers, decoded results and canonical re-encoding. The comparisons use the same physical cores of a shared i7-12700K. Independent builds and replays run elsewhere on the host during portions of the measurements.

To isolate the lookup itself, a second build keeps the compact pages and constructs the identical directory. For a sparse-page lookup it runs lower-bound binary search on the packed page numbers. Six single-core rounds compare it with the directory lookup and the branch control, rotating execution order and checking the same products after every run.

| All 143 records, ms | Branch before | Binary search on compact pages | Directory lookup | Less time than binary search |
| --- | ---: | ---: | ---: | ---: |
| Timestamp and event tag, axes 2 and 14 | 152.90 | 143.91 | 95.49 | 33.6% |
| Reusable pier decoder | 337.84 | 327.02 | 270.44 | 17.3% |

## Pill and compiler inputs

The reusable pier decoder reads each fixture, then checks its canonical jam length and SHA-256. These are decode times for the shipped nouns. [Compiler inputs at the pinned benchmark revision](https://github.com/urbit/benchmark/tree/ae0e498e19beba4ab585299adcbcce0427594e87/desk/bar)

| Median elapsed, ms | Upstream develop | Branch before | Directory | Time vs branch | Faster trials vs branch |
| --- | ---: | ---: | ---: | ---: | ---: |
| ivory.jam | 128.59 | 29.55 | 22.08 | 25.3% less | 6/6 |
| urbit-v4.6.pill | 4,441.19 | 852.09 | 604.57 | 29.0% less | 6/6 |
| hoon.noun | 27.68 | 8.65 | 6.38 | 26.3% less | 6/6 |
| prelude.noun | 27.04 | 8.77 | 6.41 | 26.9% less | 6/6 |

## Live Dojo and checkpoint replay

Three fake zods resume independent copies of the same checkpoint. The live workloads rotate execution order on one physical P-core, use a fresh prefix each round, prepare jam before timing and check the entire decoded noun. Twelve measured rounds follow one warm-up. The sparse workload uses `=wscatter [wblob (weld wpairs (flop wpairs))]`, placing a million-byte atom before 80,000 pairs followed by the same pairs in reverse order. [Dojo expressions and timing form](bench-cue-words.dojo)

| Live cue, ms | Upstream develop | Branch before | Directory | Time vs branch | Faster trials vs branch |
| --- | ---: | ---: | ---: | ---: | ---: |
| Large atom followed by repeated pairs | 82.487 | 7.098 | 4.954 | 30.2% less | 12/12 |
| 80,000 adjacent pairs | 61.416 | 3.056 | 2.777 | 9.1% less | 11/12 |
| 80,000 integers | 23.869 | 1.465 | 1.527 | 4.2% more | 5/12 |
| 80,000 references to one shared record | 9.218 | 1.464 | 1.520 | 3.8% more | 4/12 |
| 20,000 separate messages | 2.501 | 1.055 | 1.098 | 4.0% more | 4/12 |
| One-million-byte atom with prefix | 0.305 | 0.354 | 0.335 | 5.2% less | 7/12 |

Checkpoint replay times the entire process on events 233–249 in six rounds with rotating order on one P-core. Each run starts with a separate copy of the stopped checkpoint and checks its event range and completion. One disk series overlaps builds and audits. A second disk series and a tmpfs series follow with the same inputs, execution order and core. The fresh eval series runs `urbit eval` with `(add 40 2)`, checks the printed `42`, and includes startup and exit. [Replay driver](bench-develop-replay.py)

| Median full-process wall time | Upstream develop | Branch before | Directory | Time vs branch | Faster trials vs branch |
| --- | ---: | ---: | ---: | ---: | ---: |
| Checkpoint replay, disk under load, seconds | 7.96 | 8.70 | 11.40 | 31.0% more | 2/6 |
| Checkpoint replay, repeat on disk, seconds | 5.96 | 6.35 | 6.45 | 1.6% more | 3/6 |
| Checkpoint replay, tmpfs, seconds | 1.47 | 1.45 | 1.41 | 3.1% less | 3/6 |
| Fresh eval, ms | 623.93 | 505.06 | 511.71 | 1.3% more | 6/12 |

Post-workload live memory across the king and worker is 199.9 MiB RSS and 196.5 MiB PSS on the branch and 199.4 MiB RSS and 195.6 MiB PSS on the candidate. Fresh eval timings are host-sensitive. Two additional sets in the samples share a core with compilations or replay for part of the run.

## Correctness and reproduction

Both 32- and 64-bit looms match the upstream 10,514-case Nock trace (5,419 products and 5,095 expected exits), all 143 full decode products, and both projected fields per record. The serializer tests cover all 65,792 one- and two-byte streams and exercise sparse offsets across all ten possible directory heights, missing children, dense prefixes and index reuse. The serializer also passes in the default build and with the rank reader and indexed view enabled individually. A separate randomized comparison with sorted offsets passes ASan and UBSan on x86-64, runs on native 32-bit x86, and matches under QEMU on AArch64 and big-endian s390x. [Serializer tests](../pkg/noun/serial_tests.c)

The cold bootstrap core and all 155 event products from cold and checkpoint replay match upstream by canonical jam length and SHA-256 across 33,279,069,693 bytes. Upstream also resumes a checkpoint written by the directory build and verifies the persisted sparse-pair noun and its complete jam/cue round trip.

Use upstream `697fc95ddd`, the branch control `925b080fba` (same runtime as `88b71b7083`), and candidate `7f45ca502b`. The 32-bit loom ReleaseFast builds use Zig 0.15.2 and the same branch flags as the word-reader comparison. The directory runs inside the existing `U3_CUE_RANK` and `U3_CUE_VIEW` configurations. The upstream field reader decodes the entire noun. [Build flags and corpus setup](cue-word-decoding.md)

```bash
for mode in view xeno nock; do
  up=$mode
  if [[ $mode == view ]]; then up=project; fi
  python3 tests/bench-cue-projection.py "$develop" "$branch" "$candidate" \
    "$ivory" "$inputs"/event-*.jam --modes "$up" "$mode" "$mode" \
    --serial --cores 0 --rounds 6
done
```

The fixture series uses the same command in `xeno` mode with the four named inputs. Use `tests/bench-develop-replay.py` with `--candidate`, `--serial --cores 6 --rounds 6 --first 233 --last 249` on a stopped checkpoint for the replay series. Trial zero is the warm-up for the decoder, Dojo and fresh eval measurements. Every replay trial counts. [Individual samples](cue-page-directory-samples.jsonl)
