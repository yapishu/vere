# A rank index for cue on Vere 4.6

Fresh fakezods reach the first Dojo prompt in 54.9 seconds with the rank index enabled, compared with 72.8 seconds with the previous optimized runtime. Across all 143 event records in a frozen fakezod log, the registered Nock `+cue` gate takes 83.3% less time. Every resulting noun re-encodes to the exact input bytes.

## How the decoder works

Jam writes atoms and cell openings in preorder. Every atom or cell starts at a particular bit offset. A backreference can name a completed value at one of those offsets. Vere's Nock gate maps these offsets to nouns with a loom hash table. The pier decoder uses a separate native hash table.

With `U3_CUE_RANK`, the decoder sets one bit per atom or cell start. It appends one noun slot per set bit, in input order. At a backreference it finds the 64-bit word containing the offset, checks the bit, then adds the number of earlier set bits in that word to its base slot. A cell reserves its slot at the opening tag and fills it after both children decode. Open cells, atom payload bits and backreference tokens cannot supply a noun.

The decoder allocates pages only for occupied 512-bit spans. Consecutively numbered pages are directly addressable. Across gaps it can find a page by binary search. The table borrows each completed noun from the result under construction. A successful backreference acquires one reference. A protected Nock call allocates overflow storage on its road. A reusable pier decoder keeps its external buffer capacity between calls and resets the logical index after every input.

This uses the rank operation familiar from succinct tree navigation on the existing jam stream. Atom payloads, shared subtrees, the noun layout and the jam format retain their usual representation. [Succinct binary trees](https://arxiv.org/abs/1410.4963)

## Whole-runtime measurements

Every start uses a new private pier, the same official release pill and `urbit -F zod`. Timing ends when the PTY receives the first Dojo prompt. The processes run concurrently on different physical performance cores and swap cores each round. Separate four-pair comparisons use stock Vere 4.6 and the previous optimized build. All other rows in this table use the previous optimized build as baseline.

| Workload | Baseline → candidate | Less elapsed time | Faster pairs |
|---|---:|---:|---:|
| Fresh fakezod, stock baseline | 69.95 s → 55.94 s | 20.0% | 4/4 |
| Fresh fakezod, previous optimized baseline | 72.79 s → 54.94 s | 24.5% | 4/4 |
| Cold replay through event 143 | 376.61 s → 369.88 s | 1.8% | 2/2 |
| Fresh urbit eval, add 40 and 2 | 381.52 ms → 339.29 ms | 11.1% | 12/12 |
| Fresh urbit eval, parse the Arvo CSV library | 450.88 ms → 407.11 ms | 9.7% | 12/12 |
| Running Dojo, decode a gate context and check its mug | 173.50 µs → 74.50 µs | 57.1% | 8/8 |

The CLI timer includes process startup, evaluation and output. The parser reads the 1,453-byte library source and returns mug 1,294,273,084. The live test changes the payload every round, prepares its jam before timing, and checks the complete decoded noun's mug against the original inside the timed expression. Eight rounds follow one excluded warm-up in each ship. The host is one shared i7-12700K running Linux. The fresh-pier trials share the filesystem cache and use disk-backed scratch. Bootstrap spends appreciable time decoding the release pill. Cold replay also spends minutes compiling and processing events. [Arvo CSV input](https://github.com/urbit/urbit/blob/5a187fededc4582a34fcd6055c67bb63e0917b94/pkg/arvo/lib/csv.hoon)

## Decode measurements

Each worker loads the release ivory kernel, then visits every log record in chronological order. The event corpus comprises 240,504,745 jam bytes. Nock timing surrounds the protected call to the registered `cue` gate, including promotion of its result. Pier timing surrounds `u3s_cue_xeno_with` on a reusable home-road decoder. File I/O, canonical re-encoding and hashing are outside both timers. The driver verifies the SHA-256 and byte length emitted by each process against the corresponding input.

| All 143 records | Baseline → candidate | Less elapsed time | Faster pairs |
|---|---:|---:|---:|
| Nock +cue, stock baseline | 2567.50 ms → 482.56 ms | 81.2% | 4/4 |
| Pier decoder, stock baseline | 1773.47 ms → 397.09 ms | 77.6% | 4/4 |
| Nock +cue, previous optimized baseline | 3431.36 ms → 574.23 ms | 83.3% | 6/6 |
| Pier decoder, previous optimized baseline | 2396.99 ms → 463.22 ms | 80.7% | 6/6 |

Peak resident memory for the entire pier-decoder benchmark process, including validation, changes from 1354 to 677 MiB. For the Nock benchmark process it changes from 1100 to 1024 MiB.

Each row below is a separate eight-pair comparison of the previous optimized build and the candidate. Each call starts in a fresh process with its kernel loaded before timing. The corpus totals above include the smaller records as well.

| Registered gate input | Baseline → candidate | Less elapsed time | Faster pairs |
|---|---:|---:|---:|
| Release ivory pill, 1.53 MB | 179.67 ms → 42.96 ms | 76.1% | 8/8 |
| Event 3, 0.54 MB | 53.47 ms → 14.66 ms | 72.6% | 8/8 |
| Event 16, 4.23 MB | 3.30 ms → 2.87 ms | 13.2% | 7/8 |
| Event 17, 198.99 MB | 2639.88 ms → 464.49 ms | 82.4% | 8/8 |
| Event 72, 21.78 MB | 17.00 ms → 16.27 ms | 4.3% | 4/8 |

## Checks and reproduction

The serializer suite checks 80 offset targets, a sparse stream spanning empty pages, reuse after a previous input, and 512 independently generated trees with backreferences. Both decoder entry points must match each expected product or exit. The release controls and the candidate pass. The development checkout passes with memory debugging on 32-bit and 64-bit looms. Both loom widths and both entry points also reproduce the canonical jam of each of the 143 stored event records.

The separate execution audit matches stock at the bootstrap core and the product of event 143. It also matches all 17 products from events 233–249 in a saved Dojo checkpoint. These checks compare jam length and SHA-256. The Nock differential suite passes all 10,255 cases on both widths.

The tag used for timing is `vere-v4.6` at `8ddc4b786979574dbfcb655e3db1b634f658d0de`, built with Zig 0.15.2, ReleaseFast and a 32-bit loom. The previous optimized build enables `U3_DECISION_BENCH`, `U3_OWNER_BENCH`, `U3_REUSE_BENCH`, `U3_SPINE_BENCH` and `U3_DIRECT_EDIT_BENCH`. The candidate adds `U3_CUE_RANK`. Each table reports the percentage reduction between the medians. Individual sample pairs are available for all tables. [Paired samples](nock-cue-samples.csv)

On this branch, build both configurations in separate install directories. Set `bench` to a disk-backed output directory. The benchmark accepts a raw ivory jam and one or more raw event jams. A Vere LMDB event value starts with a four-byte mug followed by its jam.

```bash
flags=(-Doptimize=ReleaseFast -Dcopt=-DU3_DECISION_BENCH -Dcopt=-DU3_OWNER_BENCH
       -Dcopt=-DU3_REUSE_BENCH -Dcopt=-DU3_SPINE_BENCH -Dcopt=-DU3_DIRECT_EDIT_BENCH)
zig build cue-bench "${flags[@]}" -p "$bench/base"
zig build cue-bench serial-test "${flags[@]}" -Dcopt=-DU3_CUE_RANK -p "$bench/rank"
for impl in base rank; do
  "$bench/$impl/bin/cue-bench" nock "$ivory" "$event"
  "$bench/$impl/bin/cue-bench" xeno "$ivory" "$event"
done
```
