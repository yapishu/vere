# Vere runtime benchmarks by commit

Review branch `review/noun-runtime` through [9d47dd576c](https://github.com/yapishu/vere/commit/9d47dd576caaceb26116044fc1523f7422edb4dc) · upstream [develop baf98ea9c3](https://github.com/urbit/vere/commit/baf98ea9c31cfd4f45586d41db02cf4156c11b7b) · 27 November 2026

On an x86-64 CPU with SHA instructions, Clay page hashing takes **74.8–75.0% less time** and the complete Ames packet round-trip test takes **20.6–20.9% less time** than the preceding review, across two live fake ~zod boots.

Calling `+sham` on the noun behind the largest salted hash in cold replay takes **51.1 ms** in a live Dojo, down from **55.5 ms** on the preceding review and **65.9 ms** on develop. Reading its 20.8 MiB message in place also saves **19.3 MiB** of peak RSS across the full replay.

On two live Kelvin 135 boots, deterministic draws using inputs from successful Eyre logins take **23.7–24.2% less time** than the preceding review. Generating an 8 MiB atom reduces peak process RSS from **37.2 to 8.9 MiB** on a 32-bit loom and **49.2 to 8.8 MiB** on a 64-bit loom.

Tagging 12,251 prebuilt singleton lists takes **22.3–24.9% less time** and tagging singleton sets takes **19.5–21.3% less time** than on the preceding review across two live Dojo boots. Inside the Hoon compiler, 52,152 calls to the captured set gate take **28.7% less time**, saving a few milliseconds per compile.

On 24 complete Arvo file revisions, two source-annotated Clay map gates take **30.1–36.6% less time** across two live Dojo boots. During three desk changes, time inside all 147 map calls falls by **16.8–19.4%** across two replay series.

Finding added and removed paths between complete Arvo and Vere revisions takes **14.8–31.7% less time** than on the preceding review across eight live Dojo workloads and two independent boots. Combined king and worker PSS after the comparisons stays within **0.7 MiB** of the preceding review.

In two live Kelvin 135 Dojos, batches of map deletions captured from cold replay take **27.5–29.2% less time** when the key exists and **43.7–44.1% less time** when it is absent, against the preceding review. Across 806 repeated file removals, the map and set jets each return the original root every time. Ship PSS after the file workloads is within **0.8 MiB** of the preceding review.

Reusing a private core for prepared native calls cuts the time to sort 8,192 atoms by **22.5–23.6%** and to fold over the Lull source by **27.7–27.8%** against the preceding review. Resident ship PSS after the workloads rises by **2.0–2.7 MiB**.

At `136deb1539`, the review stack reduces cold replay of the Kelvin 135 fake ~zod through event 143 from **331.21 to 114.06 seconds** and self-hosted Hoon compilation from **12.720 to 4.717 seconds**. Across 30 real application revisions, the two `+loss` commits at that revision cut time in the subsequence operation by **20.8%** and time in the complete Clay text diff by **4.8%**. The same two commits measured 1.0% slower on cold replay.

[Hardware SHA-256](#change-9d47dd576c) · [Hashing atom storage](#change-c7aab99a51) · [Deterministic bit generation](#change-7f16135a2d) · [Short-lived gates](#change-0c171e9479) · [Prepared noun constructors](#change-78c2194073) · [Map and set difference](#change-0aebc87353) · [Map and set deletion](#change-52bac63838) · [Prepared native gate calls](#change-56b5327a55) · [Replay and compilers at `136deb1539`](#tip-totals) · [Boot, memory and public programs](#broader-snapshot) · [Real moon](#real-moon) · [Commit index](#commit-index) · [Measurement notes](#measurement-notes)

<a id="tip-totals"></a>

## Replay and compilers at `136deb1539`

Each timing below has three trials per executable. Compiler rows use the recorded Dojo evaluation timer. Replay rows use whole-command wall time. The last column uses the medians from this comparison. The preceding review already contains the bounded edit frontier.

| Workload | Develop `baf98ea9c3` | Previous review `d5f8811878` | Review `136deb1539` | Less time vs develop | Change vs previous review |
|---|---:|---:|---:|---:|---:|
| Cold replay through event 143 | 331.210 s | 112.890 s | 114.060 s | 65.6% | 1.0% longer |
| Checkpoint replay through event 249 | 1.810 s | 1.270 s | 1.270 s | 29.8% | level |
| Compile Hoon | 12.720 s | 4.946 s | 4.717 s | 62.9% | 4.6% less |
| Compile Prelude | 9.728 s | 2.909 s | 3.016 s | 69.0% | 3.7% longer |
| Compile Zuse | 4.045 s | 1.538 s | 1.563 s | 61.4% | 1.6% longer |

A breakpoint recorded zero calls to the changed jet during all three compiles and the short checkpoint replay. The compiler variation in the last column therefore supplies no evidence for a compiler gain from these two commits. The cold trials rotated across three physical cores while other jobs ran on the host.

In the same campaign, peak RSS for a mostly distinct 160,000-item native diff was **31.89 MiB on develop, 17.88 MiB on previous review and 16.59 MiB at the tip**. The live ships after all 49 revision pairs held **287.48, 286.52 and 287.27 MiB PSS** in the first boot and **285.20, 284.79 and 285.26 MiB** in the second. The working memory of one diff and the resident size of a whole ship are separately measured.

**Product checks.** Both loom widths reproduced the ordered canonical jam for 791,348 `+loss` input pairs, including ties and colliding mugs. Eight native test executables passed on both widths. The audits matched develop’s canonical length and SHA-256 for the bootstrap core, 138 cold-replay products, 17 checkpoint products and 10 recorded Dojo products, totaling 35.68 GB per width. Develop reopened the candidate’s checkpoint and reproduced its stored product at event 295.

<a id="broader-snapshot"></a>

## Boot, resident memory, serialization and public Nock programs

This broader Kelvin 135 comparison pins review at [8629d8e655](https://github.com/yapishu/vere/commit/8629d8e655c925204df56a6015231032394918a3) and develop at `baf98ea9c3`. It supplies the boot and memory figures for that revision. A fresh fake ship boots from the same 207 MiB local pill and runs to a checked Dojo reply. Cold replay reconstructs from a log without a checkpoint. Restart opens a saved checkpoint. The OS file cache was warm. Full replay has one timed pair in this series.

**Elapsed time**

| Workload | Upstream develop | Review branch | Change | Runs per build |
|---|---:|---:|---:|---:|
| Fresh ~zod from a local pill to a checked Dojo reply | 43.48 s | 25.58 s | 41.2% less | 5 |
| Restart saved ~zod to a checked reply | 2.13 s | 1.37 s | 35.4% less | 3 |
| Replay the event log through event 143, without a checkpoint | 342.03 s | 163.22 s | 52.3% less | 1 |
| Replay events 233–249 from a checkpoint | 1.96 s | 1.57 s | 20.1% less | 8 |
| Compile Hoon from parsed source | 12.67 s | 5.78 s | 54.3% less | 3 |
| Compile Lull from parsed source | 10.29 s | 5.14 s | 50.1% less | 3 |
| Parse Lull from source text | 293.0 ms | 226.7 ms | 22.6% less | 9 |
| Export checkpoint (cram) | 12.31 s | 3.71 s | 69.9% less | 3 |
| Import checkpoint (queu) | 6.11 s | 2.18 s | 64.3% less | 3 |
| Deduplicate checkpoint (meld) | 5.28 s | 5.10 s | 3.4% less | 3 |
| Hoon +jam across 143 ship event records | 3.597 s | 0.460 s | 87.2% less | 3 |
| Hoon +cue across the same records | 3.817 s | 0.294 s | 92.3% less | 3 |
| Encode the same records for the event log, including mugs | 3.535 s | 0.476 s | 86.5% less | 3 |

**Memory**

| Measurement | Upstream develop | Review branch | Change | Runs per build |
|---|---:|---:|---:|---:|
| Fresh boot, peak king + worker PSS | 2,463.2 MiB | 1,861.6 MiB | 24.4% less | 5 |
| Idle after fresh boot, king + worker PSS | 696.5 MiB | 534.3 MiB | 23.3% less | 5 |
| Idle after checkpoint restart, king + worker PSS | 215.3 MiB | 215.9 MiB | 0.3% more | 3 |
| Full replay, peak RSS | 1,145.2 MiB | 835.9 MiB | 27.0% less | 1 |
| Checkpoint export, peak RSS | 4,403.5 MiB | 848.0 MiB | 80.7% less | 3 |
| Checkpoint import, peak RSS | 3,396.4 MiB | 664.5 MiB | 80.4% less | 3 |
| Checkpoint deduplication, peak RSS | 766.1 MiB | 765.9 MiB | 0.0% less | 3 |

**Public Nock programs in a live ship**

| Program × calls per timed batch | Upstream develop | Review branch | Change | Runs per build |
|---|---:|---:|---:|---:|
| ackermann × 1 | 634.44 ms | 25.77 ms | 95.9% less | 7 |
| add × 2 | 47.73 ms | 0.35 ms | 99.3% less | 7 |
| bigslot × 64 | 16.41 ms | 1.28 ms | 92.2% less | 7 |
| dec × 8,000 | 41.00 ms | 2.64 ms | 93.6% less | 7 |
| factorial × 512 | 88.98 ms | 30.54 ms | 65.7% less | 7 |
| fibonacci × 256 | 39.34 ms | 17.57 ms | 55.3% less | 7 |
| mmul × 16 | 15.67 ms | 5.38 ms | 65.7% less | 7 |

<a id="real-moon"></a>

## Real moon at an earlier review revision

A saved comparison of the same moon pier pins review at [783265084f](https://github.com/yapishu/vere/commit/783265084f58ee9361c9b4b094168dfbcee3067e) and develop at `baf98ea9c3`. The full window covers events 94,999 through 355,119, with one full-epoch sample per compared executable. Process wall time fell **36.4%**, while peak RSS rose **2.5%**. The post-roll ext4 snapshot row uses the subsequent open. The complete epoch, shorter event window and snapshot operation have different starting states.

| Workload | Develop | Review | Change vs develop |
|---|---:|---:|---:|
| Complete epoch, process wall | 1,541.76 s | 980.51 s | 36.4% less |
| Complete epoch, event loop | 1,532.82 s | 975.67 s | 36.3% less |
| Complete epoch, peak RSS | 1,278.04 MiB | 1,310.15 MiB | 2.5% more |
| 16,384 events, process wall | 71.31 s | 46.38 s | 35.0% less |
| 16,384 events, event loop | 63.05 s | 41.26 s | 34.6% less |
| 16,384 events, peak RSS | 896.12 MiB | 920.96 MiB | 2.8% more |
| Same snapshot load afterward, ext4 | 8.41 s | 6.10 s | 27.5% less |
| Clean restart to checked Dojo reply, ext4 | 2.86 s | 1.07 s | 62.7% less |

<a id="measurement-notes"></a>

## Measurement notes

- **Incremental** compares a candidate with the preceding review configuration in its saved campaign. **Multiple commits** means the reported result covers all listed changes together. Intermediate build results appear where the report supplies them.
- **Historical component controls** measure techniques before consolidation into the first review commits. **Cumulative** compares a whole review tree with the named upstream revision. Early campaigns used develop `697fc95ddd`. The merge brought in `baf98ea9c3`.
- Reported percentages retain each campaign’s statistic. Some use the median of paired reductions, which can differ from the percentage change between the two displayed medians. Keep each percentage with its own baseline. Adding percentages across commits or multiplying results from different fixtures gives an invalid branch total.
- Peak RSS covers one process over its lifetime unless the table specifies otherwise. PSS apportions shared pages and the ship measurements often sum king and worker. Allocation counters, loom reservation and retained noun sizes measure other quantities.
- The ledger reproduces selected saved tables, with the original build labels, units and sign conventions. Workloads range from individual native calls through whole replay. Each comparison names its baseline.

<a id="commit-index"></a>

## Commit index

The index follows the review stack in first-parent order. Commits sharing a measurement link to the same entry.

| Commit | Change | Measurement |
|---|---|---|
| [696a08825a](https://github.com/yapishu/vere/commit/696a08825aed2fc0f2aacad7000092964e7331e2) | index jam reads and encode shared subtrees once | [Historical component controls](#change-696a08825a) |
| [f19b814606](https://github.com/yapishu/vere/commit/f19b81460633720681ad9127cbe971b1c1a0381f) | share comparisons and build canonical collections in bulk | [Historical component controls](#change-f19b814606) |
| [8dc2cd5bf5](https://github.com/yapishu/vere/commit/8dc2cd5bf5df6ee9672cd3c5f0ab2ae20c4249a6) | share subject paths and evaluate eligible folds over shared nouns | [Historical component and cumulative controls](#change-8dc2cd5bf5) |
| [6438085868](https://github.com/yapishu/vere/commit/6438085868268f98b8581170ca83355c38519929) | reuse gate-builder results on the current road | [Review integration](#change-6438085868) |
| [6539f8c340](https://github.com/yapishu/vere/commit/6539f8c3400e1f737a904002b3491c9ed1caaa5e) | match nested patterns directly on jammed nouns | [Multiple commits](#change-6539f8c340) |
| [7b3ed7d66e](https://github.com/yapishu/vere/commit/7b3ed7d66e0447dbfc0176bd93ee2cdfc726913e) | include jam search in the standard binary | [Multiple commits](#change-6539f8c340) |
| [e5713418e5](https://github.com/yapishu/vere/commit/e5713418e53abe406acae750427efa25456d0522) | cite published techniques beside their implementations | [Documentation only](#change-e5713418e5) |
| [08b2c5a143](https://github.com/yapishu/vere/commit/08b2c5a143038793a50cfc062ff63b6a3411e518) | preserve shared branches when uniting noun maps | [Multiple commits, distinct operations](#change-08b2c5a143) |
| [41c63206f0](https://github.com/yapishu/vere/commit/41c63206f0aeb975e9e3505e0b80a415fa023793) | index matching positions directly in longest common subsequence | [Multiple commits, distinct operations](#change-08b2c5a143) |
| [ed501841e1](https://github.com/yapishu/vere/commit/ed501841e135b47cf9146caa17f38fd1a1064977) | copy ordinary JSON string bytes in runs | [Multiple commits, distinct operations](#change-08b2c5a143) |
| [e89ef1269b](https://github.com/yapishu/vere/commit/e89ef1269b9e85360a62fa0bbfc60289613ec8b6) | stream unions of comparable noun maps in key order | [Focused operation](#change-e89ef1269b) |
| [6d26b65b43](https://github.com/yapishu/vere/commit/6d26b65b43c0789197183f1b3452bfca17aad098) | reconstruct dense longest common subsequences in bit stripes | [Focused variant](#change-6d26b65b43) |
| [e07a7bdb9b](https://github.com/yapishu/vere/commit/e07a7bdb9b52dfc03228bfb9948efe0268bf7a8f) | resolve structural wings directly in k137 fond | [Incremental](#change-e07a7bdb9b) |
| [8c48ac70c1](https://github.com/yapishu/vere/commit/8c48ac70c1d4ce5033854e2088993f0877e07041) | resolve k137 names across forks and completed holds | [Incremental](#change-8c48ac70c1) |
| [603faab90f](https://github.com/yapishu/vere/commit/603faab90fbe33a00a37ddab6077725b1f48f0df) | split noun maps and sets along a single search path | [Incremental](#change-603faab90f) |
| [322ced7dd8](https://github.com/yapishu/vere/commit/322ced7dd8306988ee66ceda3b9dfe5132ba5d78) | intersect ordered noun maps and sets in one pass | [Incremental](#change-322ced7dd8) |
| [caca1a40cf](https://github.com/yapishu/vere/commit/caca1a40cfabfb831f24a3a87e6ff261e95dc273) | remember expensive equalities between ancestor nouns | [Multiple commits](#change-caca1a40cf) |
| [159dde26aa](https://github.com/yapishu/vere/commit/159dde26aa0c44e78ee79c32887161c18eaa7a82) | compare a bound value directly with a context field | [Multiple commits](#change-caca1a40cf) |
| [d1e1003f99](https://github.com/yapishu/vere/commit/d1e1003f99e0d4e101175acf82c2f65e75c34e9d) | track completed comparisons by their two cell handles | [Multiple commits](#change-d1e1003f99) |
| [3b100e8e1f](https://github.com/yapishu/vere/commit/3b100e8e1fc8bb15d9a0388c898c5e42189ffe1b) | reuse proven ancestor subtrees during equality | [Multiple commits](#change-d1e1003f99) |
| [529457f1a7](https://github.com/yapishu/vere/commit/529457f1a71d84f8f07c2594f20f734c9169222b) | Merge upstream develop into review/noun-runtime | [Merge and cumulative comparison](#change-529457f1a7) |
| [f0ab6c67e8](https://github.com/yapishu/vere/commit/f0ab6c67e8798df9de5d003559f9e0829ed9c9d7) | reuse registered parent locations within a child road | [Incremental](#change-f0ab6c67e8) |
| [8f4a9e8641](https://github.com/yapishu/vere/commit/8f4a9e8641fd2b8ab646d7ba8cc62840a1b16a11) | specialize short field lists in native jet wrappers | [Incremental](#change-8f4a9e8641) |
| [3b0f67d360](https://github.com/yapishu/vere/commit/3b0f67d360aee67a6f205a4b7d1631008b0d4fd4) | skip repeated registry misses for a locked battery | [Incremental](#change-3b0f67d360) |
| [bdfc084e4d](https://github.com/yapishu/vere/commit/bdfc084e4d6154a56e10e05cda39de18bc89d1a8) | carry key and heap order through map and set validation | [Incremental](#change-bdfc084e4d) |
| [79827ef5ed](https://github.com/yapishu/vere/commit/79827ef5edbb42d855e87f0454730ba20291f7a0) | project map keys while checking tree order | [Incremental](#change-79827ef5ed) |
| [caaf13c9fc](https://github.com/yapishu/vere/commit/caaf13c9fc063a5826603f4f5ac6892c619878dc) | carry type pairs through structural nest checks | [Incremental](#change-caaf13c9fc) |
| [e90b8c5efb](https://github.com/yapishu/vere/commit/e90b8c5efb54253daa05db6be8a2f2cdf5731183) | filter hashcons probes with spare slot bits | [Incremental](#change-e90b8c5efb) |
| [6194f5f2c2](https://github.com/yapishu/vere/commit/6194f5f2c2f57d57604ce8887f7c4eff8f58e345) | retain hashcons fingerprints on wide looms | [Incremental](#change-6194f5f2c2) |
| [aa4bb8c2c5](https://github.com/yapishu/vere/commit/aa4bb8c2c52bb133432d36c6904f089bd7253501) | carry treap ranks and share branches through point inserts | [Incremental](#change-aa4bb8c2c5) |
| [76e0b84755](https://github.com/yapishu/vere/commit/76e0b84755e36040b47e26c5fd15f7144dce0d91) | carry search hashes and borrow native map values | [Incremental](#change-76e0b84755) |
| [42715575b4](https://github.com/yapishu/vere/commit/42715575b403900f79b870cf4043b9bbe398ab2f) | refurbish faces along structural type paths | [Incremental](#change-42715575b4) |
| [1a78049be3](https://github.com/yapishu/vere/commit/1a78049be3f12e8bab981b8271a842e965ba79f2) | resolve structural wings in Kelvin 135 | [Multiple commits](#change-1a78049be3) |
| [4247fa8a74](https://github.com/yapishu/vere/commit/4247fa8a7409b75323daf1fbe24cf56622510e35) | build an arm axis only when the search finds its name | [Multiple commits](#change-1a78049be3) |
| [25b8c4aae9](https://github.com/yapishu/vere/commit/25b8c4aae992cff21ef82c85c99d50701bb9ea0c) | refurbish recursive types through completed hold expansions | [Incremental](#change-25b8c4aae9) |
| [c553aef0c2](https://github.com/yapishu/vere/commit/c553aef0c27b733910595776a84d29ecace9f1bc) | edit the type at a resolved wing for single-arm calls | [Incremental](#change-c553aef0c2) |
| [a4fe1e6b6b](https://github.com/yapishu/vere/commit/a4fe1e6b6bfaf8a0f7e7dfc569594bff1ecc2784) | project type axes through forks and return natural wing matches | [Incremental](#change-a4fe1e6b6b) |
| [45a6ba3fa2](https://github.com/yapishu/vere/commit/45a6ba3fa2ab3ba70fb8a19e0cebb8c57244acf9) | deduplicate checkpoint cells by their canonical children | [Incremental](#change-45a6ba3fa2) |
| [c480f3d8d8](https://github.com/yapishu/vere/commit/c480f3d8d8a4e78de5c7fe762e4e724dd267c46d) | copy full loom words when exporting indirect atoms | [Multiple commits, including a correctness fix](#change-c480f3d8d8) |
| [33bfbb2f82](https://github.com/yapishu/vere/commit/33bfbb2f8219285d593c82ecd9ec4b14312bda6a) | index jam positions by cell ID for dense traversals | [Multiple commits, including a correctness fix](#change-c480f3d8d8) |
| [e958e6a74c](https://github.com/yapishu/vere/commit/e958e6a74c059728d3428a9bcba3ea570534adf0) | intern cells by their canonical child handles | [Multiple commits, including an ownership fix](#change-e958e6a74c) |
| [b9f0ab3163](https://github.com/yapishu/vere/commit/b9f0ab3163c5210e732dca6f9d2ecfc7b6e10a26) | release duplicate atom buffers during interning | [Multiple commits, including an ownership fix](#change-e958e6a74c) |
| [64f3b3be29](https://github.com/yapishu/vere/commit/64f3b3be29fa1f6f1cfa2597900d445eb6157595) | filter dictionary entries by backreference targets | [Multiple commits, distinct paths](#change-64f3b3be29) |
| [a70aee1c00](https://github.com/yapishu/vere/commit/a70aee1c0041eee7731aaea2374963dfa2aa7b5f) | cue event records directly from the log buffer | [Multiple commits, distinct paths](#change-64f3b3be29) |
| [d15af74f1b](https://github.com/yapishu/vere/commit/d15af74f1b8c75c402a0fe8b20ec6736bad76aa0) | filter jam class probes with spare hash table bits | [Multiple commits](#change-d15af74f1b) |
| [832e82e86c](https://github.com/yapishu/vere/commit/832e82e86cd3b22b547597037404a03b729e6f54) | pack canonical jam directly into its destination | [Multiple commits](#change-d15af74f1b) |
| [b0d1d5a303](https://github.com/yapishu/vere/commit/b0d1d5a303d2057931604d8f77f3026033adf442) | identify adjacent parent classes by their postorder IDs | [Incremental](#change-b0d1d5a303) |
| [20dfb283a2](https://github.com/yapishu/vere/commit/20dfb283a22944a6cdf13d5a6106c23ce5dd35c4) | infer adjacent parent handles during cell interning | [Multiple commits with an intermediate build](#change-20dfb283a2) |
| [0d31704e9c](https://github.com/yapishu/vere/commit/0d31704e9c481debbac3f22424fae5fd5abfe62d) | grow the three cell arrays independently | [Multiple commits with an intermediate build](#change-20dfb283a2) |
| [8ec3b987b1](https://github.com/yapishu/vere/commit/8ec3b987b1b22b6c9fe0169f03c7aaf0bcd56c01) | reuse arena mugs when rebuilding the loom | [Multiple commits with an intermediate build](#change-20dfb283a2) |
| [81d8dc1c7b](https://github.com/yapishu/vere/commit/81d8dc1c7ba40ac20ada33ed46c34984652bddaa) | preserve integer width when interning atom bytes | [Multiple commits with an intermediate build](#change-20dfb283a2) |
| [c1e8742808](https://github.com/yapishu/vere/commit/c1e8742808cf951a2eedb63161c2b29e547e54aa) | discard unused loom pages when replacing the home road | [Multiple commits with intermediate builds](#change-c1e8742808) |
| [f41e4d8954](https://github.com/yapishu/vere/commit/f41e4d895477936d5aab373339a2b7f90bdb4869) | write the rock before rebuilding the loom | [Multiple commits with intermediate builds](#change-c1e8742808) |
| [ca7f073484](https://github.com/yapishu/vere/commit/ca7f0734842b8e52519e054062387851044a4874) | release evacuated atom pages during checkpoint export | [Multiple commits with intermediate builds](#change-c1e8742808) |
| [5c28ff0672](https://github.com/yapishu/vere/commit/5c28ff06723b1b2adc7cac29dfda646cea4400cf) | check that a failed rock write restores shared atoms | [Multiple commits with intermediate builds](#change-c1e8742808) |
| [d3fccfaa24](https://github.com/yapishu/vere/commit/d3fccfaa24c1aa92f6fe0ec0990c52d38f1d4051) | split and concatenate byte strings in atom buffers | [Incremental](#change-d3fccfaa24) |
| [8629d8e655](https://github.com/yapishu/vere/commit/8629d8e655c925204df56a6015231032394918a3) | build byte lists in batches from the road cell cache | [Incremental](#change-8629d8e655) |
| [6702faad73](https://github.com/yapishu/vere/commit/6702faad73a79a8c68b8584b7428f57329c8b4c4) | reverse atom bytes directly in swp and rev | [Incremental](#change-6702faad73) |
| [487f06c938](https://github.com/yapishu/vere/commit/487f06c9388bddbba66e9cf62ef508edcb8612bb) | split newline-terminated cords with the shared byte scanner | [Incremental](#change-487f06c938) |
| [ad0fcb22d4](https://github.com/yapishu/vere/commit/ad0fcb22d46b552e2c9249365c9b5da0e9c53d81) | prefetch hash-cons buckets while walking checkpoint cells | [Incremental](#change-ad0fcb22d4) |
| [93fc95a579](https://github.com/yapishu/vere/commit/93fc95a579c558d6a1ae61a325e0e56b8032b19f) | build checkpoint hash-cons tables in home-bucket order | [Incremental](#change-93fc95a579) |
| [e4a3c6800a](https://github.com/yapishu/vere/commit/e4a3c6800afc16fd074cd39fc7348bc3b666fd66) | compare the first differing noun fields for aor and dor | [Multiple commits with a registration-only control](#change-e4a3c6800a) |
| [60c01815cd](https://github.com/yapishu/vere/commit/60c01815cd1d010518fde21eea049fab937bc1f7) | sort verified noun orders with normalized prefixes | [Multiple commits with a registration-only control](#change-e4a3c6800a) |
| [d9a65c37d2](https://github.com/yapishu/vere/commit/d9a65c37d23cfc6f8af7e7c89518fa73259efb63) | compare equal shared noun graphs in a single walk | [Multiple commits with a registration-only control](#change-e4a3c6800a) |
| [d1bab7c2e9](https://github.com/yapishu/vere/commit/d1bab7c2e9a34fba7d5b560b513aed10546f8f9a) | compile axis edits in a compressed binary trie | [Incremental](#change-d1bab7c2e9) |
| [a56b375a03](https://github.com/yapishu/vere/commit/a56b375a0320e4389e80a462cf0c7e27f84974d1) | build private JSON object maps by treap splitting | [Incremental](#change-a56b375a03) |
| [6de113961a](https://github.com/yapishu/vere/commit/6de113961a19eaf2873d3bbdaf9ac5eeb88f1cd8) | scan buffered ASCII strings and whitespace in runs | [Incremental](#change-6de113961a) |
| [783265084f](https://github.com/yapishu/vere/commit/783265084f58ee9361c9b4b094168dfbcee3067e) | advance the cache clock through occupied HAMT children | [Incremental](#change-783265084f) |
| [091cd3074c](https://github.com/yapishu/vere/commit/091cd3074c57dce5835240fb4c5c02ea5cdf29e5) | reap inherited programs only when a jet site changes | [Incremental](#change-091cd3074c) |
| [09b3f06281](https://github.com/yapishu/vere/commit/09b3f062811c3cb43bb13c91877dba953c1da4f1) | merge dense batches into populated maps and sets | [Incremental](#change-09b3f06281) |
| [e402da877a](https://github.com/yapishu/vere/commit/e402da877aefc62970777aca019682a672c76501) | refurbish recursive list types in one pass | [Incremental](#change-e402da877a) |
| [4942a7dd88](https://github.com/yapishu/vere/commit/4942a7dd88b550fb5238227631410fa46563acb9) | share field reads when mapping records with turn | [Incremental](#change-4942a7dd88) |
| [cc0ddf6b53](https://github.com/yapishu/vere/commit/cc0ddf6b5371203dc514b5ba63bd046fd913273a) | batch independent reads when flattening large maps and sets | [Incremental](#change-cc0ddf6b53) |
| [030e24f609](https://github.com/yapishu/vere/commit/030e24f60944239fc4eb9528b43663a1839e146d) | construct terminal protected cue results on the receiving road | [Multiple commits with a parser-only control](#change-030e24f609) |
| [6b1dea57d2](https://github.com/yapishu/vere/commit/6b1dea57d284c925e30d525f85d3ce938423de48) | specialize the cue reader for each constructor and close cells inline | [Multiple commits with a parser-only control](#change-030e24f609) |
| [d6eee3e8bb](https://github.com/yapishu/vere/commit/d6eee3e8bb79a24ccfd270acd1395c4f2b8e9d87) | scan plain JSON string spans and write the exact output atom | [Incremental](#change-d6eee3e8bb) |
| [3421967e39](https://github.com/yapishu/vere/commit/3421967e396930a4da3b8feae6cc14cb3d2e8b45) | combine cell mugs directly from their two words | [Incremental](#change-3421967e39) |
| [4d116b0a7d](https://github.com/yapishu/vere/commit/4d116b0a7d62697eba2067bc4d38b44fa51748d0) | carry the mugged memo key from lookup to save | [Incremental](#change-4d116b0a7d) |
| [16525ae06c](https://github.com/yapishu/vere/commit/16525ae06c4848a5d638c54694a3252f8a53aba4) | compose noun axes directly in the result words | [Incremental](#change-16525ae06c) |
| [de20d1d452](https://github.com/yapishu/vere/commit/de20d1d452fa57671c5725a8b22185f2f2034df7) | materialize field axes only when a name resolves | [Incremental](#change-de20d1d452) |
| [3ba5984dea](https://github.com/yapishu/vere/commit/3ba5984dea65c33f29384f2e034d23dd48a923dc) | move transient memo lookups past cheap decisions | [Incremental](#change-3ba5984dea) |
| [cdff69c001](https://github.com/yapishu/vere/commit/cdff69c0015084d46b442f962ed77876092b98dc) | reclaim transient memo entries in groups | [Incremental](#change-cdff69c001) |
| [e833fce569](https://github.com/yapishu/vere/commit/e833fce56986cbf6d7f280ec8b984ec33f2d865b) | use memo dependencies to set CLOCK insertion priority | [Incremental](#change-e833fce569) |
| [4f095fdad7](https://github.com/yapishu/vere/commit/4f095fdad7171b3cdb5aff18bc7c081fe8109778) | grow packed hash trie nodes in place | [Incremental](#change-4f095fdad7) |
| [d5f8811878](https://github.com/yapishu/vere/commit/d5f88118788c2856a452fe81ed2c5bf636550a29) | find canonical loss along bounded edit frontiers | [Incremental](#change-d5f8811878) |
| [c45cfd8d75](https://github.com/yapishu/vere/commit/c45cfd8d752db1a63486c055eb7068f19452e218) | count both loss inputs while validating their spines | [Multiple commits](#change-c45cfd8d75) |
| [136deb1539](https://github.com/yapishu/vere/commit/136deb15399dc0a52024ab31b00cb98ae9968362) | index loss occurrence chains by integer position | [Multiple commits](#change-c45cfd8d75) |
| [56b5327a55](https://github.com/yapishu/vere/commit/56b5327a5511c2087cdc8783dc5ae9b242686f6b) | reuse private cores across prepared native calls | [Incremental](#change-56b5327a55) |
| [52bac63838](https://github.com/yapishu/vere/commit/52bac63838d4036cc0793f2837f7f126a980a2f2) | rebuild only the treap path changed by deletion | [Incremental](#change-52bac63838) |
| [0aebc87353](https://github.com/yapishu/vere/commit/0aebc873537ea5dc79c04a734e902550c88f9a17) | pass treap partitions directly through difference | [Incremental](#change-0aebc87353) |
| [78c2194073](https://github.com/yapishu/vere/commit/78c2194073d5c2c2e16c52738f42eb2407ddb974) | prepare one-hole constructors for repeated gate calls | [Incremental](#change-78c2194073) |
| [0c171e9479](https://github.com/yapishu/vere/commit/0c171e94798b61f9e66b9050b4615465d448ab67) | prepare short-lived gates without building their fallback | [Incremental](#change-0c171e9479) |
| [7f16135a2d](https://github.com/yapishu/vere/commit/7f16135a2d7c37e92245aaf0a52b5172e2537aff) | write og digest blocks directly into the result atom | [Incremental](#change-7f16135a2d) |
| [c7aab99a51](https://github.com/yapishu/vere/commit/c7aab99a513337f5fa8472a877054008e9fc4948) | hash shax and shas messages directly from the atom | [Incremental](#change-c7aab99a51) |
| [9d47dd576c](https://github.com/yapishu/vere/commit/9d47dd576caaceb26116044fc1523f7422edb4dc) | use Nettle SHA-256 instructions on capable CPUs | [Incremental](#change-9d47dd576c) |

## Measurements by change

<a id="change-696a08825a"></a>

### Index jam reads and encode shared subtrees once

**Historical component controls.** The exact-size encoder cut Hoon jam across 143 events from 3,306.1 to 1,185.5 ms. In separate comparisons, word decoding cut the pier decoder from 411.33 to 361.83 ms and the page directory cut a read of axes 2 and 14 from 175.99 to 99.24 ms. Each comparison has its own starting build.

- [696a08825a](https://github.com/yapishu/vere/commit/696a08825aed2fc0f2aacad7000092964e7331e2) — noun: index jam reads and encode shared subtrees once

**Real event records**

| Workload | Upstream develop | Branch | Candidate | Less vs branch | Less vs develop |
|---|---:|---:|---:|---:|---:|
| Hoon slab, 143 records (ms) | 3,012.9 | 3,306.1 | 1,185.5 | +64.1% | +60.7% |
| Hoon slab, other 142 (ms) | 112.5 | 108.6 | 46.6 | +57.1% | +58.6% |
| Hoon slab, process peak RSS (MiB) | 1,707.4 | 1,108.5 | 820.0 | +26.0% | +52.0% |
| IPC buffer, 143 records (ms) | 2,774.6 | 3,058.6 | 1,165.7 | +61.9% | +58.0% |
| IPC buffer, other 142 (ms) | 114.2 | 115.8 | 50.7 | +56.2% | +55.6% |
| IPC buffer, process peak RSS (MiB) | 1,455.4 | 856.6 | 819.9 | +4.3% | +43.7% |

**Inside a running ship**

| Workload | Upstream develop | Branch | Candidate | Less vs branch | Less vs develop |
|---|---:|---:|---:|---:|---:|
| List of 80,000 consecutive atoms (ms) | 25.069 | 24.590 | 10.819 | +56.0% | +56.8% |
| 80,000 distinct pairs (ms) | 51.151 | 51.008 | 26.889 | +47.3% | +47.4% |
| 80,000 records with 1,984 possible values (ms) | 16.447 | 15.639 | 12.591 | +19.5% | +23.4% |
| 80,000 references to one record (ms) | 12.384 | 12.727 | 7.298 | +42.7% | +41.1% |
| One MiB atom inside a pair (ms) | 0.247 | 0.259 | 0.226 | +12.7% | +8.5% |
| 20,000 separate two-atom messages (ms) | 12.901 | 11.680 | 6.824 | +41.6% | +47.1% |

**Compiler, replay and memory**

| Workload | Upstream develop | Branch | Candidate | Less vs branch | Less vs develop |
|---|---:|---:|---:|---:|---:|
| Hoon compile (s) | 23.11 | 16.93 | 17.31 | -2.2% | +25.1% |
| Prelude compile (s) | 24.20 | 17.41 | 17.66 | -1.4% | +27.0% |
| Checkpoint replay, first series (s) | 11.97 | 11.52 | 12.60 | -9.4% | -5.3% |
| Checkpoint replay, second series (s) | 8.36 | 8.32 | 7.98 | +4.1% | +4.6% |
| Cold replay, one observation (s) | 439.84 | 337.32 | 332.82 | +1.3% | +24.3% |
| Resumed ship PSS (MiB) | 274.6 | 276.1 | 275.6 | +0.2% | -0.4% |
| Worker peak RSS (MiB) | 228.5 | 229.9 | 229.7 | +0.1% | -0.5% |

**Stored event corpus**

| All 143 records, ms | Upstream develop | Branch before | Candidate | Less time vs branch | Less time vs develop |
| --- | ---: | ---: | ---: | ---: | ---: |
| Registered Nock cue gate | 2,531.93 | 512.10 | 477.82 | 6.7% | 81.1% |
| Pier decoder | 2,061.62 | 411.33 | 361.83 | 12.0% | 82.4% |
| Two event fields | 2,147.26 | 239.01 | 192.96 | 19.3% | 91.0% |

**Real event records**

| Median elapsed, ms | Upstream develop | Branch before | Directory | Time vs branch | Faster trials vs branch |
| --- | ---: | ---: | ---: | ---: | ---: |
| Timestamp and event tag, axes 2 and 14 | 2,371.96 | 175.99 | 99.24 | 43.6% less | 6/6 |
| Reusable pier decoder | 2,407.53 | 393.88 | 323.19 | 17.9% less | 5/6 |
| Registered Nock `+cue` gate | 3,281.86 | 517.33 | 433.22 | 16.3% less | 6/6 |

**Real event records**

| Median peak process RSS, MiB | Upstream develop | Branch before | Directory | RSS vs branch |
| --- | ---: | ---: | ---: | ---: |
| Timestamp and event tag, axes 2 and 14 | 1,372.6 | 314.4 | 303.5 | 3.5% less |
| Reusable pier decoder | 1,372.6 | 656.3 | 645.4 | 1.7% less |
| Registered Nock `+cue` gate | 1,105.5 | 958.8 | 947.2 | 1.2% less |

<a id="change-f19b814606"></a>

### Share comparisons and build canonical collections in bulk

**Historical component controls.** The bulk builder cut construction of a set of 20,000 atoms from 14.188 to 0.620 ms. Comparing equal lists of 120,000 atoms fell from 26.555 to 3.178 ms against the parent of the equality change. The report also counts peak allocated bytes for the collection jobs.

- [f19b814606](https://github.com/yapishu/vere/commit/f19b81460633720681ad9127cbe971b1c1a0381f) — noun: share comparisons and build canonical collections in bulk

**Live ship operations**

| Workload, ms | Develop | Starting branch | Final | Less time vs develop |
| --- | ---: | ---: | ---: | ---: |
| Build a set of 20,000 atoms | 13.633 | 14.188 | 0.62 | 95.5% |
| Build a map of 20,000 pairs | 16.064 | 17.102 | 0.755 | 95.3% |
| Build a map from 20,000 pairs with 257 keys | 7.622 | 8.289 | 0.689 | 91.0% |
| Extract 20,000 atom keys | 15.059 | 14.46 | 2.119 | 85.9% |
| Extract 20,000 two-atom keys | 13.191 | 12.521 | 2.288 | 82.7% |
| Extract 256 keys | 0.103 | 0.096 | 0.024 | 76.7% |
| Build an eight-pair map | 0.007 | 0.008 | 0.006 | 14.3% |
| Build from 20,000 copies of one pair | 0.574 | 0.631 | 0.099 | 82.8% |

**Live ship operations**

| Workload, ms | Develop | Parent of equality change | Final | Less time vs develop |
| --- | ---: | ---: | ---: | ---: |
| Equal lists of 120,000 atoms | 25.805 | 26.555 | 3.178 | 87.7% |
| Same 120,000 atoms with one extra tail element | 3.448 | 3.966 | 2.138 | 38.0% |
| Equal lists of 120,000 two-atom records | 71.951 | 65.794 | 14.255 | 80.2% |
| Equal lists of 256 atoms | 0.003 | 0.003 | 0.003 | 0.0% |

**Memory**

| Operation | Develop peak bytes | Starting branch peak bytes | Final peak bytes |
| --- | ---: | ---: | ---: |
| Build a set of 20,000 atoms | 1,589,072 | 1,589,072 | 966,480 |
| Build a map of 20,000 pairs | 1,589,072 | 1,589,072 | 966,480 |
| Build a map from 20,000 pairs with 257 keys | 999,248 | 999,248 | 327,680 |
| Extract 20,000 atom keys | 638,800 | 638,800 | 524,112 |
| Equal lists of 120,000 atoms | 4,911,792 | 4,911,792 | 512 |
| Equal lists of 120,000 two-atom records | 8,926,288 | 8,926,288 | 2,304,032 |

<a id="change-8dc2cd5bf5"></a>

### Share subject paths and evaluate eligible folds over shared nouns

**Historical component and cumulative controls.** The finite-fold experiment cut whole cold replay by 27.8% and 28.0% in two pairs that exchanged cores. Peak RSS grew 0.8 MiB. A separate subject-path experiment cut full replay by 1.2% and took 7.8% longer on 96 group and channel reads. The signed review tree also has an end-to-end comparison covering the first three commits together. Its HTTP row used an earlier integrated executable.

- [8dc2cd5bf5](https://github.com/yapishu/vere/commit/8dc2cd5bf5df6ee9672cd3c5f0ab2ae20c4249a6) — nock: share subject paths and evaluate eligible folds over shared nouns

**Evaluating two-state folds on shared noun trees**

| Cold replay | Control CPU | Fold CPU | Branch control | Fold | Less time | Peak RSS, control / fold |
|---|---:|---:|---:|---:|---:|---:|
| 1 | 12 | 4 | 308.01 s | 222.48 s | 27.8% | 1,085.1 / 1,085.9 MiB |
| 2 | 4 | 12 | 295.60 s | 212.95 s | 28.0% | 1,085.1 / 1,085.9 MiB |

**Other workloads**

| Workload | Branch control | Fold |
|---|---:|---:|
| Compile Hoon | 17.74 s | 17.45 s |
| Compile Prelude | 18.39 s | 18.08 s |
| Synthetic shared tree, 16 direct merges | 120.57 ms | 0.30 ms |
| Synthetic shared tree, 16 curried merges | 266.22 ms | 0.83 ms |
| Synthetic tree, merge outside the two-state domain | 100.25 ms | 99.52 ms |

**Compiled paths through a Nock subject**

| Workload | Upstream develop | Branch control | Compiled projections | Less time vs. branch |
|---|---:|---:|---:|---:|
| Compile Hoon | 23.23 s | 17.09 s | 17.17 s | -0.4% |
| Compile Prelude | 23.77 s | 17.12 s | 17.21 s | -0.5% |
| Cold replay through event 143 | 443.90 s | 280.68 s | 277.37 s | +1.2% |
| 96 populated group and channel reads | 1.848 s | 1.354 s | 1.460 s | -7.8% |

**What sharing saves**

| Envelope axis | Tag, branch | Tag, candidate | Less time | Nested heads, branch | Nested heads, candidate | Less time |
|---|---:|---:|---:|---:|---:|---:|
| 6 | 12.1 ns | 11.5 ns | 5.3% | 20.4 ns | 12.9 ns | 36.6% |
| 30 | 14.9 ns | 13.4 ns | 10.2% | 22.1 ns | 14.4 ns | 34.9% |
| 510 | 20.7 ns | 17.9 ns | 13.7% | 30.9 ns | 18.3 ns | 40.7% |

**Vere runtime benchmarks**

| Workload | Upstream develop | Optimized | Change in elapsed time |
|---|---:|---:|---:|
| Cold replay through event 143 | 405.63 s | 193.99 s | 52.2% lower |
| Compile Hoon | 22.55 s | 15.99 s | 29.1% lower |
| Compile Prelude | 23.26 s | 16.40 s | 29.5% lower |
| Resume and replay events 233–249 | 1.310 s | 1.356 s | 3.5% higher |
| 96 authenticated group and channel reads | 1.845 s | 1.411 s | 23.5% lower |

<a id="change-6438085868"></a>

### Reuse gate-builder results on the current road

**Review integration.** The saved integration run measures four compiler pairs. The table uses median wall seconds and median process peak RSS. The ordinary review binary and the integrated gate-builder binary produced the same recorded compiler digests.

- [6438085868](https://github.com/yapishu/vere/commit/6438085868268f98b8581170ca83355c38519929) — nock: reuse gate-builder results on the current road

**Gate-builder integration**

| Workload | Pairs | Review wall, s | Integrated wall, s | Less wall time | Review peak RSS, MiB | Integrated peak RSS, MiB |
|---|---:|---:|---:|---:|---:|---:|
| Hoon | 4 | 17.040 | 16.190 | 5.0% | 118.21 | 94.16 |
| Prelude | 4 | 17.225 | 16.855 | 2.1% | 115.52 | 90.99 |

<a id="change-6539f8c340"></a>

### Searching compressed event logs for nested patterns

**Multiple commits.** The first change implements structural matching over jam. The second puts the search in the standard binary. Seven patterns over 143 log records took 446.81 ms with ordinary cue plus matching and 100.63 ms with integrated search. The packaging change has no independent timing.

- [6539f8c340](https://github.com/yapishu/vere/commit/6539f8c3400e1f737a904002b3491c9ed1caaa5e) — noun: match nested patterns directly on jammed nouns
- [7b3ed7d66e](https://github.com/yapishu/vere/commit/7b3ed7d66e0447dbfc0176bd93ee2cdfc726913e) — vere: include jam search in the standard binary

Stack parent before this group is [6438085868](https://github.com/yapishu/vere/commit/6438085868268f98b8581170ca83355c38519929).

**Integrated jam search**

| Event corpus, seven patterns | Trials per build | Review cue and matcher, ms | Integrated direct search, ms | Less wall time | Cue / direct peak RSS, MiB |
|---|---:|---:|---:|---:|---:|
| Events 1–143 | 8 | 446.81 | 100.63 | 77.5% | 533.24 / 141.37 |
| Same epoch without the largest event | 8 | 38.51 | 7.13 | 81.5% | 63.60 / 9.18 |
| Events 144–249 | 8 | 4.06 | 1.63 | 59.9% | 1.38 / 0.38 |

<a id="change-e5713418e5"></a>

### Cite published techniques beside their implementations

**Documentation only.** Adds references beside the implementations. No runtime performance claim.

- [e5713418e5](https://github.com/yapishu/vere/commit/e5713418e53abe406acae750427efa25456d0522) — noun: cite published techniques beside their implementations

<a id="change-08b2c5a143"></a>

### Shared map unions, indexed subsequences and JSON string runs

**Multiple commits, distinct operations.** In commit order, a one-edit map union took 77.6% less time, a Vere source diff took 74.9% less, and JSON encoding of 49 posts took 46.2% less. These operation timings use a build containing all three jets. Interleaved map union took 2.9% longer. The map driver retaining 16 versions fell from 33.42 to 4.02 MiB peak RSS.

- [08b2c5a143](https://github.com/yapishu/vere/commit/08b2c5a143038793a50cfc062ff63b6a3411e518) — jets: preserve shared branches when uniting noun maps
- [41c63206f0](https://github.com/yapishu/vere/commit/41c63206f0aeb975e9e3505e0b80a415fa023793) — jets: index matching positions directly in longest common subsequence
- [ed501841e1](https://github.com/yapishu/vere/commit/ed501841e135b47cf9146caa17f38fd1a1064977) — jets: copy ordinary JSON string bytes in runs

**Native operations in median CPU milliseconds per call**

| Workload | Develop | Review | Candidate | Less vs develop | Less vs review |
|---|---:|---:|---:|---:|---:|
| map / same | 0.8188 | 0.8103 | 0.1795 | +78.1% | +77.8% |
| map / one-edit | 0.8106 | 0.7973 | 0.1786 | +78.0% | +77.6% |
| map / separate | 0.9452 | 0.9435 | 0.3965 | +58.0% | +58.0% |
| map / interleaved | 45.4041 | 45.9080 | 47.2217 | -4.0% | -2.9% |
| map / small | 0.0800 | 0.0905 | 0.0804 | -0.5% | +11.2% |
| lcs / vere-source | 12.2760 | 12.3427 | 3.1038 | +74.7% | +74.9% |
| lcs / repeated | 10.5647 | 10.5218 | 4.4394 | +58.0% | +57.8% |
| lcs / sparse-edits | 11.7189 | 11.7239 | 0.7563 | +93.5% | +93.5% |
| json / 49-posts | 0.0593 | 0.0609 | 0.0328 | +44.8% | +46.2% |
| json / 8-posts | 0.0099 | 0.0101 | 0.0054 | +45.3% | +46.2% |
| json / groups | 0.0076 | 0.0079 | 0.0046 | +39.9% | +41.6% |
| json / plain | 0.7003 | 0.6973 | 0.0990 | +85.9% | +85.8% |
| json / sparse-escapes | 0.7264 | 0.7342 | 0.1103 | +84.8% | +85.0% |
| json / dense-escapes | 0.7761 | 0.7528 | 0.7240 | +6.7% | +3.8% |

**Peak process RSS in MiB**

| Workload | Develop | Review | Candidate | Less vs develop | Less vs review |
|---|---:|---:|---:|---:|---:|
| maps | 33.69 | 33.42 | 4.02 | +88.1% | +88.0% |
| lcs | 23.97 | 23.89 | 13.24 | +44.8% | +44.6% |

**Whole workloads**

| Workload | Develop | Review | Candidate | Less vs develop | Less vs review |
|---|---:|---:|---:|---:|---:|
| Compile hoon (s) | 22.17 | 14.95 | 14.77 | +33.4% | +1.2% |
| Compile prelude (s) | 22.88 | 15.25 | 15.35 | +32.9% | -0.6% |
| Authenticated HTTP, 49 posts (ms) | 19.92 | 16.25 | 15.77 | +20.8% | +2.9% |
| Authenticated HTTP, Eight posts (ms) | 15.02 | 11.79 | 11.59 | +22.8% | +1.7% |
| Authenticated HTTP, Group list (ms) | 19.96 | 14.81 | 14.60 | +26.8% | +1.4% |

<a id="change-e89ef1269b"></a>

### Stream unions of comparable noun maps in key order

**Focused operation.** Streaming map union cut the measured unions from events 14 and 16 by 83.0% and 92.1%. Fully interleaved input benefited most in the shape tests. The benchmark records eligible calls so the times can be interpreted in proportion to their frequency.

- [e89ef1269b](https://github.com/yapishu/vere/commit/e89ef1269b9e85360a62fa0bbfc60289613ec8b6) — jets: stream unions of comparable noun maps in key order

**How often map union streams**

| Event | Left keys | Right keys | Overlap | Side switches among unshared keys | Review ms | Streaming ms | Paired reduction |
|---|---|---|---|---|---|---|---|
| 14 | 624 | 129 | 28 | 178 | 0.2015 | 0.0344 | 83.0% |
| 16 | 490 | 803 | 76 | 521 | 0.9164 | 0.0716 | 92.1% |

**How often map union streams**

| Map fixture | Review ms | Streaming candidate ms | Paired reduction | Pair range |
|---|---|---|---|---|
| interleaved | 46.9571 | 3.9827 | 91.9% | +89.0% to +93.0% |
| half-overlap | 33.5249 | 33.2553 | 1.5% | -0.1% to +40.5% |
| one-edit | 0.2633 | 0.2465 | 7.1% | +3.0% to +15.8% |
| small | 0.0805 | 0.0787 | 0.8% | -9.2% to +13.0% |
| skew-384 | 1.3071 | 1.2961 | 0.9% | -0.2% to +4.6% |
| vere-files | 0.6181 | 0.0701 | 88.5% | +87.6% to +88.8% |

<a id="change-6d26b65b43"></a>

### Reconstruct dense longest common subsequences in bit stripes

**Focused variant.** The dense subsequence algorithm with the overlap check cut the Vere source diff by 57.2%. Clay source took 1.0% longer and disjoint repeated values took 0.6% longer. Use the overlap-check column for the integrated algorithm.

- [6d26b65b43](https://github.com/yapishu/vere/commit/6d26b65b43c0789197183f1b3452bfca17aad098) — jets: reconstruct dense longest common subsequences in bit stripes

**Thread CPU for the dense subsequence jet. Absolute milliseconds refer to the overlap-check comparison**

| Fixture | Research vs review | With overlap check vs review | Review ms¹ | With check ms¹ |
|---|---:|---:|---:|---:|
| Clay source diff | -0.3% | -1.0% | 3.1433 | 3.1764 |
| Vere source diff | +59.4% | +57.2% | 3.0110 | 1.2893 |
| Disjoint repeated values | -15.5% | -0.6% | 0.2650 | 0.2670 |
| Sparse edits | +0.0% | -0.4% | 0.7584 | 0.7595 |
| Repeated symbols | +89.6% | +89.0% | 4.3182 | 0.4757 |

<a id="change-e07a7bdb9b"></a>

### Resolve structural wings directly in k137 fond

**Incremental.** Kelvin 137 structural wing resolution cut Hoon and Prelude compiler evaluation by paired medians of 10.3% and 12.0%. The ranges in the table show run-to-run spread.

- [e07a7bdb9b](https://github.com/yapishu/vere/commit/e07a7bdb9b52dfc03228bfb9948efe0268bf7a8f) — jets: resolve structural wings directly in k137 fond

Stack parent before this change is [6d26b65b43](https://github.com/yapishu/vere/commit/6d26b65b43c0789197183f1b3452bfca17aad098).

**Timing**

| Workload | Pairs | Review s | Research s | Paired reduction | Pair range | Review peak MiB | Research peak MiB |
|---|---:|---:|---:|---:|---:|---:|---:|
| Hoon | 9 | 15.697 | 14.646 | 10.3% | 0.2% to 27.3% | 94.25 | 94.36 |
| Prelude | 9 | 16.469 | 14.290 | 12.0% | 9.0% to 22.7% | 91.09 | 91.19 |
| Clay | 2 | 168.115 | 160.975 | 4.2% | 1.6% to 6.8% | 418.39 | 418.59 |

**Timing**

| Compiler series | Workload | Pairs | Paired reduction | Pair range |
|---|---|---:|---:|---:|
| Alternating | hoon | 5 | 9.7% | 0.2% to 17.0% |
| Alternating | prelude | 5 | 12.0% | 9.0% to 18.9% |
| Synchronized | hoon | 4 | 11.8% | 3.4% to 27.3% |
| Synchronized | prelude | 4 | 12.5% | 11.8% to 22.7% |

<a id="change-8c48ac70c1"></a>

### Resolve k137 names across forks and completed holds

**Incremental.** Resolving names across forks and completed holds cut Hoon time by 7.7% and Prelude by 8.8% on Kelvin 137. The complete Clay desk-install event saw a much smaller change.

- [8c48ac70c1](https://github.com/yapishu/vere/commit/8c48ac70c1d4ce5033854e2088993f0877e07041) — jets: resolve k137 names across forks and completed holds

Stack parent before this change is [e07a7bdb9b](https://github.com/yapishu/vere/commit/e07a7bdb9b52dfc03228bfb9948efe0268bf7a8f).

**Review and candidate**

| Workload | Pairs | Control s | Candidate s | Paired reduction | Pair range | Control peak MiB | Candidate peak MiB |
|---|---:|---:|---:|---:|---:|---:|---:|
| hoon | 10 | 14.285 | 13.362 | 7.7% | -6.9% to 18.6% | 94.29 | 94.60 |
| prelude | 10 | 14.705 | 13.541 | 8.8% | -6.3% to 34.9% | 91.12 | 91.43 |
| Clay event 16 | 2 | 168.885 | 167.105 | 1.0% | 0.1% to 2.0% | 418.89 | 419.25 |

<a id="change-603faab90f"></a>

### Split noun maps and sets along a single search path

**Incremental.** Splitting the Arvo path map took 41.6% less time and splitting the corresponding set took 38.7% less. These are path-list operations.

- [603faab90f](https://github.com/yapishu/vere/commit/603faab90fbe33a00a37ddab6077725b1f48f0df) — jets: split noun maps and sets along a single search path

Stack parent before this change is [8c48ac70c1](https://github.com/yapishu/vere/commit/8c48ac70c1d4ce5033854e2088993f0877e07041).

**Direct splits for noun maps and sets**

| Keys | Manifest operation | Review µs | Candidate µs | Less time | Paired range |
| --- | --- | ---: | ---: | ---: | ---: |
| Path list | Arvo, map-split | 0.880 | 0.515 | 41.6% | 39.7–42.2% |
| Path list | Arvo, set-split | 0.837 | 0.511 | 38.7% | 37.7–40.0% |
| Path list | Arvo removals, map-difference | 207.879 | 107.245 | 48.2% | 47.0–49.7% |
| Path list | Arvo removals, set-difference | 194.378 | 109.101 | 44.1% | 43.1–45.1% |
| Path list | Arvo, map-split | 0.786 | 0.457 | 41.6% | 41.0–43.2% |
| Path list | Arvo, set-split | 0.750 | 0.445 | 40.6% | 34.8–42.6% |
| Path list | Arvo additions, map-difference | 209.993 | 92.227 | 56.0% | 55.3–56.8% |
| Path list | Arvo additions, set-difference | 198.497 | 94.976 | 52.4% | 51.5–53.0% |
| Path list | Vere, map-split | 0.962 | 0.592 | 38.6% | 37.7–40.5% |
| Path list | Vere, set-split | 0.941 | 0.594 | 37.1% | 34.9–38.3% |
| Path list | Vere removals, map-difference | 86.725 | 23.825 | 72.5% | 71.6–73.0% |
| Path list | Vere removals, set-difference | 53.027 | 23.375 | 55.5% | 52.5–57.3% |
| Flat cord | Arvo, map-split | 0.949 | 0.568 | 40.3% | 37.8–41.4% |
| Flat cord | Arvo, set-split | 0.900 | 0.538 | 39.9% | 38.0–41.5% |
| Flat cord | Arvo removals, map-difference | 177.166 | 97.976 | 45.0% | 44.0–46.9% |
| Flat cord | Arvo removals, set-difference | 164.177 | 95.456 | 41.7% | 40.1–46.1% |
| Flat cord | Vere, map-split | 0.993 | 0.600 | 39.4% | 37.8–39.9% |
| Flat cord | Vere, set-split | 0.949 | 0.580 | 38.8% | 37.1–39.6% |
| Flat cord | Vere removals, map-difference | 144.033 | 38.250 | 73.5% | 72.7–74.2% |
| Flat cord | Vere removals, set-difference | 75.442 | 31.404 | 59.0% | 53.4–61.4% |
| Flat cord | Arvo, map-split | 0.865 | 0.511 | 41.4% | 39.8–51.4% |
| Flat cord | Arvo, set-split | 0.813 | 0.485 | 40.3% | 39.5–40.6% |
| Flat cord | Arvo additions, map-difference | 166.749 | 80.974 | 51.5% | 49.5–54.6% |
| Flat cord | Arvo additions, set-difference | 155.938 | 79.124 | 49.4% | 47.7–50.2% |
| Flat cord | Vere, map-split | 0.972 | 0.589 | 39.6% | 37.1–39.8% |
| Flat cord | Vere, set-split | 0.933 | 0.576 | 38.6% | 36.6–42.9% |
| Flat cord | Vere additions, map-difference | 131.819 | 42.608 | 67.9% | 66.8–69.0% |
| Flat cord | Vere additions, set-difference | 69.345 | 34.104 | 50.3% | 40.7–54.2% |

<a id="change-322ced7dd8"></a>

### Intersect ordered noun maps and sets in one pass

**Incremental.** A bulk-edit path intersection benefited inside a running ship. The entire first commit event measured 193.521 versus 193.739 ms. The isolated intersection and full-event timings show how much of the operation was on the critical path.

- [322ced7dd8](https://github.com/yapishu/vere/commit/322ced7dd8306988ee66ceda3b9dfe5132ba5d78) — jets: intersect ordered noun maps and sets in one pass

Stack parent before this change is [603faab90f](https://github.com/yapishu/vere/commit/603faab90fbe33a00a37ddab6077725b1f48f0df).

**Inside a running ship**

| Workload | Review µs | Candidate µs | Less time, paired | Paired range |
| --- | ---: | ---: | ---: | ---: |
| First commit, 321 × 5 paths | 6.018 | 4.527 | +24.9% | +22.0 to +30.8% |
| Bulk edit, 337 × 326 paths | 84.236 | 24.574 | +70.8% | +69.7 to +72.2% |
| Single edit, 1 × 390 paths | 1.254 | 0.463 | +61.2% | +22.6 to +80.3% |
| Bulk edit, ancillary 6 × 6 map | 0.453 | 0.766 | -69.4% | -194.8 to -11.9% |

**Inside a running ship**

| Workload | Review ms | Candidate ms | Less time, paired | Paired range |
| --- | ---: | ---: | ---: | ---: |
| Complete first commit event | 193.521 | 193.739 | +0.1% | -3.7 to +3.1% |
| Complete bulk commit event | 202.289 | 201.755 | +0.1% | -1.5 to +2.3% |
| Complete single-edit event | 38.298 | 37.540 | +1.8% | -0.4 to +5.9% |

**Revision manifests**

| Workload | Review µs | Candidate µs | Less time, paired | Paired range |
| --- | ---: | ---: | ---: | ---: |
| Arvo, 455 upserts × 671 old paths | 372.056 | 26.049 | +93.0% | +92.3 to +93.6% |
| Vere, 65 upserts × 1,306 old paths | 80.840 | 54.081 | +33.1% | +29.7 to +33.9% |
| Arvo, complete maps | 553.083 | 29.476 | +94.7% | +94.2 to +94.9% |
| Arvo, complete sets | 356.372 | 28.748 | +91.9% | +90.9 to +92.4% |
| Vere, complete maps | 363.788 | 63.684 | +82.5% | +82.1 to +83.0% |
| Vere, complete sets | 167.820 | 61.744 | +63.1% | +62.6 to +63.9% |
| Vere, map and a persistent one-key edit | 69.122 | 3.470 | +95.0% | +94.7 to +95.1% |
| Arvo, map union | 57.873 | 57.726 | -0.9% | -23.7 to +7.6% |
| Vere, map union | 91.145 | 94.106 | -3.2% | -5.1 to -1.8% |

**Whole runtime and memory**

| Workload | Review s | Candidate s | Less time, paired | Paired range |
| --- | ---: | ---: | ---: | ---: |
| Hoon compile | 12.228 | 12.355 | -1.2% | -4.0 to -0.7% |
| Prelude compile | 13.004 | 13.332 | -2.1% | -2.6 to +0.1% |
| Cold replay to event 143 | 226.06 | 220.81 | +2.4% | +1.1 to +3.6% |

<a id="change-caca1a40cf"></a>

### Ancestor equality and bound-field comparisons

**Multiple commits.** Remembering expensive ancestor comparisons and comparing bound fields directly reduced normalization of a restored Lull AST from 206.083 s to 0.205 s. Resident normalization fell from 18.575 to 15.528 ms. These are distinct operations with very different starting costs.

- [caca1a40cf](https://github.com/yapishu/vere/commit/caca1a40cfabfb831f24a3a87e6ff261e95dc273) — noun: remember expensive equalities between ancestor nouns
- [159dde26aa](https://github.com/yapishu/vere/commit/159dde26aa0c44e78ee79c32887161c18eaa7a82) — nock: compare a bound value directly with a context field

Stack parent before this group is [322ced7dd8](https://github.com/yapishu/vere/commit/322ced7dd8306988ee66ceda3b9dfe5132ba5d78).

**Running ships**

| Live Dojo workload | Develop | Previous review | New review | Less time vs previous review |
|---|---:|---:|---:|---:|
| Resident mold over the full lull.hoon AST | 25.376 ms | 18.575 ms | 15.528 ms | 16.4% |
| Restored mold over a small parsed gate | 194.013 ms | 122.625 ms | 6.010 ms | 95.1% |
| Restored mold over the full lull.hoon AST | 344.798 s | 206.083 s | 0.205 s | 99.9% |

**Standalone Nock evaluation**

| Restored input | Develop | Previous review | New review | Less time vs previous review |
|---|---:|---:|---:|---:|
| mold-lull | 38,240.4 ms | 24,733.4 ms | 125.9 ms | 99.5% |
| mold-map | 1,971.0 ms | 1,346.7 ms | 16.3 ms | 98.8% |

**Whole workloads**

| Workload | Trials per build | Previous review median | New review median | Median paired time reduction |
|---|---:|---:|---:|---:|
| Compile Hoon | 6 | 12.85 s | 12.99 s | 0.9% |
| Compile Prelude | 6 | 12.81 s | 12.80 s | 0.8% |
| Clay events 285–297, worker time | 8 | 523.18 ms | 527.83 ms | 0.6% |
| Cold replay to 143, whole process | 2 | 210.16 s | 207.24 s | 1.4% |

<a id="change-d1e1003f99"></a>

### Completed cell pairs and equality of ancestor subtrees

**Multiple commits.** Tracking completed cell pairs and reusing proven ancestor subtrees reduced the restored Lull parser from 23,948.94 to 681.62 ms against the pre-merge review control. Upstream also changed equality during this period. The next entry gives the like-for-like post-merge result.

- [d1e1003f99](https://github.com/yapishu/vere/commit/d1e1003f99e0d4e101175acf82c2f65e75c34e9d) — noun: track completed comparisons by their two cell handles
- [3b100e8e1f](https://github.com/yapishu/vere/commit/3b100e8e1fc8bb15d9a0388c898c5e42189ffe1b) — noun: reuse proven ancestor subtrees during equality

Stack parent before this group is [159dde26aa](https://github.com/yapishu/vere/commit/159dde26aa0c44e78ee79c32887161c18eaa7a82).

**Earlier review baseline**

| Workload | Previous review | Equality commits | Reduction |
|---|---:|---:|---:|
| Resident parser | 310.51 ms | 312.53 ms | -0.7% |
| Restored parser, lull.hoon | 23,948.94 ms | 681.62 ms | +97.2% |
| Restored parser, small input | 416.54 ms | 79.61 ms | +80.9% |
| Restored mold, lull AST | 227.66 ms | 23.52 ms | +89.7% |
| Standalone lull parser peak RSS | 479.0 MiB | 62.3 MiB | +87.0% |

<a id="change-529457f1a7"></a>

### Merge upstream develop into review/noun-runtime

**Merge and cumulative comparison.** The second parent brings in the newer develop baseline. Against that upstream tree the merged branch took 283.67 versus 309.97 ms for the restored Lull parser and 17.22 versus 22.89 ms for the restored mold. These reductions cover the stack through the merge.

- [529457f1a7](https://github.com/yapishu/vere/commit/529457f1a71d84f8f07c2594f20f734c9169222b) — Merge upstream develop into review/noun-runtime

**Merged review against current develop**

| Workload | Current develop | Merged review | Reduction |
|---|---:|---:|---:|
| Resident parser, before restored calls | 314.10 ms | 284.36 ms | +9.5% |
| Resident mold on the lull AST | 22.76 ms | 16.60 ms | +27.1% |
| Restored mold on the lull AST | 22.89 ms | 17.22 ms | +24.8% |
| Restored parser, small input | 3.88 ms | 3.06 ms | +21.0% |
| Restored parser, lull.hoon | 309.97 ms | 283.67 ms | +8.5% |

**Merged review against current develop**

| Workload | Current develop | Merged review | Reduction |
|---|---:|---:|---:|
| Standalone parser, lull.hoon | 851.56 ms | 662.13 ms | +22.2% |
| Standalone parser, lib/map.hoon | 75.09 ms | 56.35 ms | +24.9% |
| Standalone parser, small input | 32.89 ms | 27.17 ms | +17.4% |
| Compile Hoon | 20.49 s | 14.94 s | +27.1% |
| Compile Prelude | 20.76 s | 13.83 s | +33.4% |
| Cold replay through event 143 | 342.64 s | 211.84 s | +38.2% |

**Merged review against current develop**

| Workload | Current develop | Merged review | Reduction |
|---|---:|---:|---:|
| Standalone lull parser peak RSS | 121.8 MiB | 60.9 MiB | +50.0% |
| Live ship PSS after the full restored parse | 192.6 MiB | 204.7 MiB | -6.3% |
| Compiler peak RSS, Hoon | 113.2 MiB | 91.8 MiB | +18.9% |
| Cold replay peak RSS | 1,145.0 MiB | 1,021.2 MiB | +10.8% |

<a id="change-f0ab6c67e8"></a>

### Reuse registered parent locations within a child road

**Incremental.** Reusing registered jet parents in child roads cut the resident parser from 286.38 to 228.52 ms across both live series. The Hoon compiler gained 3.0%.

- [f0ab6c67e8](https://github.com/yapishu/vere/commit/f0ab6c67e8798df9de5d003559f9e0829ed9c9d7) — jets: reuse registered parent locations within a child road

Stack parent before this change is [529457f1a7](https://github.com/yapishu/vere/commit/529457f1a71d84f8f07c2594f20f734c9169222b).

**Live Dojo, both series**

| Workload | Develop | Previous review | Candidate | Less time vs review | Less time vs develop |
|---|---:|---:|---:|---:|---:|
| resident | 303.01 ms | 286.38 ms | 228.52 ms | +20.2% | +24.6% |
| restored-lull | 302.69 ms | 286.49 ms | 227.40 ms | +20.6% | +24.9% |
| resident-mold | 21.94 ms | 16.62 ms | 15.81 ms | +4.8% | +27.9% |

**Standalone evaluation**

| Workload | Develop | Previous review | Candidate | Less time vs review | Less time vs develop |
|---|---:|---:|---:|---:|---:|
| compile-hoon | 18.55 s | 12.05 s | 11.69 s | +3.0% | +37.0% |
| compile-prelude | 19.12 s | 12.11 s | 11.74 s | +3.1% | +38.6% |

<a id="change-8f4a9e8641"></a>

### Specialize short field lists in native jet wrappers

**Incremental.** Specializing short wrapper field lists cut 100,000 bit-mask operations by 19.0%, parsing Hoon source by 8.4%, and whole Hoon compilation by 2.3% in this series.

- [8f4a9e8641](https://github.com/yapishu/vere/commit/8f4a9e8641fd2b8ab646d7ba8cc62840a1b16a11) — noun: specialize short field lists in native jet wrappers

Stack parent before this change is [f0ab6c67e8](https://github.com/yapishu/vere/commit/f0ab6c67e8798df9de5d003559f9e0829ed9c9d7).

**Running ships**

| Workload | Develop | Previous review | Candidate | Less time vs review | Less time vs develop |
|---|---:|---:|---:|---:|---:|
| 100,000 bit masks | 13.070 ms | 11.053 ms | 8.955 ms | +19.0% | +31.5% |
| 100,000 set membership queries | 28.104 ms | 23.281 ms | 21.041 ms | +9.6% | +25.1% |
| Parse sys/lull.hoon | 304.528 ms | 235.902 ms | 223.246 ms | +5.4% | +26.7% |
| Format 4,096 ship names | 28.236 ms | 27.723 ms | 26.192 ms | +5.5% | +7.2% |
| 100,000 bit slices | 12.236 ms | 10.163 ms | 10.299 ms | -1.3% | +15.8% |

**Compiler and parser**

| Workload | Develop | Previous review | Candidate | Less time vs review | Less time vs develop |
|---|---:|---:|---:|---:|---:|
| Compile Hoon | 18.402 s | 11.587 s | 11.319 s | +2.3% | +38.5% |
| Compile Prelude | 19.381 s | 11.695 s | 11.420 s | +2.4% | +41.1% |

**Compiler and parser**

| Workload | Develop | Previous review | Candidate | Less time vs review | Less time vs develop |
|---|---:|---:|---:|---:|---:|
| Parse sys/hoon.hoon | 3077.184 ms | 1626.426 ms | 1490.049 ms | +8.4% | +51.6% |
| Parse sys/lull.hoon | 781.238 ms | 535.035 ms | 512.148 ms | +4.3% | +34.4% |
| Small parser workload | 8.880 ms | 8.652 ms | 8.817 ms | -1.9% | +0.7% |

<a id="change-3b0f67d360"></a>

### Skip repeated registry misses for a locked battery

**Incremental.** Skipping repeated registry misses on locked batteries cut Hoon compile from 11.97 to 11.22 s. The live dispatch tests and ship-memory table show its effect under other call patterns.

- [3b0f67d360](https://github.com/yapishu/vere/commit/3b0f67d360aee67a6f205a4b7d1631008b0d4fd4) — jets: skip repeated registry misses for a locked battery

Stack parent before this change is [8f4a9e8641](https://github.com/yapishu/vere/commit/8f4a9e8641fd2b8ab646d7ba8cc62840a1b16a11).

**Live Dojo**

| Workload | develop | review | clean | Less time vs review | Less time vs develop |
|---|---:|---:|---:|---:|---:|
| resident | 319.19 ms | 221.90 ms | 209.71 ms | +5.5% | +34.3% |
| restored-lull | 316.42 ms | 233.53 ms | 211.29 ms | +9.5% | +33.2% |
| resident-mold | 22.22 ms | 16.69 ms | 15.24 ms | +8.7% | +31.4% |
| tree-dispatch | 345.96 ms | 246.12 ms | 199.98 ms | +18.7% | +42.2% |
| build-fold | 71.69 ms | 56.50 ms | 47.56 ms | +15.8% | +33.7% |
| map-fold | 36.41 ms | 28.64 ms | 23.40 ms | +18.3% | +35.7% |
| sum | 56.74 ms | 44.83 ms | 39.00 ms | +13.0% | +31.3% |

**Standalone compilers and parsers**

| Workload | develop | review | clean | Less time vs review | Less time vs develop |
|---|---:|---:|---:|---:|---:|
| compile-hoon | 19.44 s | 11.97 s | 11.22 s | +6.3% | +42.3% |
| compile-prelude | 20.18 s | 11.63 s | 11.36 s | +2.4% | +43.7% |

**Ship memory in MiB, summed across king and worker**

| Stage | Measure | Develop | Review | Candidate | Change vs review |
|---|---|---:|---:|---:|---:|
| prepared | Rss | 192.96 | 205.91 | 206.25 | +0.34 |
| prepared | Pss | 189.01 | 202.13 | 202.47 | +0.34 |
| prepared | Pss_Anon | 90.60 | 94.31 | 94.62 | +0.32 |
| after-work | Rss | 196.77 | 208.37 | 208.72 | +0.35 |
| after-work | Pss | 192.82 | 204.59 | 204.95 | +0.35 |
| after-work | Pss_Anon | 93.29 | 95.91 | 96.26 | +0.35 |

<a id="change-bdfc084e4d"></a>

### Carry key and heap order through map and set validation

**Incremental.** Carrying key and heap order cut validation of a 120,000-key map by 36.4%. Against upstream validator functions, validation of 24 captured resident Clay trees fell from 108.1 to 39.5 µs per batch (63.5%). Decoding, validating and releasing them saved 9.6%.

- [bdfc084e4d](https://github.com/yapishu/vere/commit/bdfc084e4d6154a56e10e05cda39de18bc89d1a8) — jets: carry key and heap order through map and set validation

Stack parent before this change is [3b0f67d360](https://github.com/yapishu/vere/commit/3b0f67d360aee67a6f205a4b7d1631008b0d4fd4).

**Live Dojo medians in milliseconds**

| Workload | Develop | Previous review | Candidate | Less time vs review | Less time vs develop |
| --- | ---: | ---: | ---: | ---: | ---: |
| Validate map, 120,000 atom keys | 17.490 | 17.316 | 11.012 | 36.4% | 37.0% |
| Validate set, 120,000 atom keys | 9.356 | 8.424 | 5.665 | 32.8% | 39.5% |
| Extract keys, 120,000 atom keys | 112.992 | 24.580 | 18.169 | 26.1% | 83.9% |
| Extract keys, 20,000 cell keys | 11.085 | 2.644 | 2.123 | 19.7% | 80.8% |
| Extract keys, 256 atom keys | 0.096 | 0.025 | 0.017 | 32.0% | 82.3% |
| Build map from 120,000 pairs and extract keys | 224.028 | 26.412 | 18.523 | 29.9% | 91.7% |

**Captured Clay trees in microseconds per batch of 24, using the upstream validator as control**

| Workload | Control | Candidate | Less time |
| --- | ---: | ---: | ---: |
| Resident trees | 108.1 | 39.5 | 63.5% |
| Decode, validate and release | 602.4 | 544.3 | 9.6% |

**Whole workloads**

| Workload | Review s | Candidate s | Less time | Review peak MiB | Candidate peak MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Hoon compile | 11.126 | 11.044 | +0.7% | 91.48 | 91.52 |
| Prelude compile | 11.280 | 11.296 | -0.1% | 88.78 | 88.82 |
| Cold replay through event 143 | 205.89 | 205.89 | +0.0% | 1021.21 | 1020.83 |

<a id="change-79827ef5ed"></a>

### Project map keys while checking tree order

**Incremental.** Extracting 120,000 atom keys fell from 17.737 to 11.387 ms. The retained path-data test on Arvo saved 16.9%.

- [79827ef5ed](https://github.com/yapishu/vere/commit/79827ef5edbb42d855e87f0454730ba20291f7a0) — jets: project map keys while checking tree order

Stack parent before this change is [bdfc084e4d](https://github.com/yapishu/vere/commit/bdfc084e4d6154a56e10e05cda39de18bc89d1a8).

**Live Dojo medians in milliseconds**

| Workload | Develop | Previous review | Candidate | Less time vs review | Less time vs develop |
| --- | ---: | ---: | ---: | ---: | ---: |
| Extract 120,000 atom keys | 112.019 | 17.737 | 11.387 | +35.8% | +89.8% |
| Extract 20,000 cell keys | 10.981 | 2.111 | 1.559 | +26.1% | +85.8% |
| Extract 256 atom keys | 0.100 | 0.021 | 0.017 | +19.0% | +83.0% |
| Build a 120,000-entry map and extract keys | 228.136 | 18.443 | 16.193 | +12.2% | +92.9% |
| Extract and traverse all keys | 116.686 | 22.782 | 15.591 | +31.6% | +86.6% |
| Extract and test 50,000 memberships | 145.228 | 43.243 | 38.056 | +12.0% | +73.8% |
| Traverse a retained key set | 3.771 | 4.432 | 4.621 | -4.3% | -22.5% |
| Test 50,000 memberships on a retained set | 37.740 | 31.864 | 33.194 | -4.2% | +12.0% |
| Validate map only | 16.453 | 10.505 | 10.799 | -2.8% | +34.4% |
| Validate set only | 9.215 | 5.419 | 5.834 | -7.7% | +36.7% |

**Repository and Clay path projections in microseconds**

| Input | Previous projection | Candidate | Less time |
| --- | ---: | ---: | ---: |
| Arvo | 69.25 | 57.56 | 16.9% |
| Vere | 146.10 | 119.36 | 18.3% |
| Clay | 175.60 | 143.92 | 18.0% |

**Whole workloads**

| Workload | Previous review s | Candidate s | Review peak RSS MiB | Candidate peak RSS MiB |
| --- | ---: | ---: | ---: | ---: |
| Hoon compile | 11.457 | 11.137 | 91.49 | 91.75 |
| Prelude compile | 11.653 | 11.321 | 88.80 | 89.05 |
| Cold replay, pair 1 | 233.23 | 229.76 | 1020.80 | 1021.07 |
| Cold replay, pair 2 | 186.41 | 189.77 | 1020.80 | 1021.07 |

<a id="change-caaf13c9fc"></a>

### Carry type pairs through structural nest checks

**Incremental.** Checking a Hoon union against 117 arms took 40.8% less time. Complete Hoon compile saved 3.1%. Ship PSS after type queries rose from 171.66 to 173.49 MiB.

- [caaf13c9fc](https://github.com/yapishu/vere/commit/caaf13c9fc063a5826603f4f5ac6892c619878dc) — jets: carry type pairs through structural nest checks

Stack parent before this change is [79827ef5ed](https://github.com/yapishu/vere/commit/79827ef5edbb42d855e87f0454730ba20291f7a0).

**Structural type checks in +nest**

| Live Dojo workload | Develop | Previous review | Candidate | Less time vs review | Less time vs develop |
|---|---:|---:|---:|---:|---:|
| Hoon union against each of its 117 arms | 20.62 ms | 17.32 ms | 10.26 ms | +40.8% | +50.3% |
| Original Hoon type against the 117 arms | 20.60 ms | 17.14 ms | 10.11 ms | +41.0% | +50.9% |
| Each arm against the whole Hoon union | 10.06 ms | 8.05 ms | 7.67 ms | +4.8% | +23.8% |
| 234 queries against a 117-way tagged union | 26.24 ms | 22.94 ms | 6.21 ms | +72.9% | +76.3% |
| 234 queries against an 8-way tagged union | 2.56 ms | 2.25 ms | 0.65 ms | +71.2% | +74.6% |

**Structural type checks in +nest**

| Complete compiler evaluation | Develop | Previous review | Candidate | Less time vs review | Less time vs develop |
|---|---:|---:|---:|---:|---:|
| Hoon | 18.70 s | 11.39 s | 11.04 s | +3.1% | +40.9% |
| Prelude | 19.34 s | 11.61 s | 11.29 s | +2.7% | +41.6% |

**Structural type checks in +nest**

| Memory | Develop | Previous review | Candidate |
|---|---:|---:|---:|
| Live ship PSS after Hoon type queries | 168.02 MiB | 171.66 MiB | 173.49 MiB |
| Hoon compile peak RSS | 113.12 MiB | 91.49 MiB | 91.61 MiB |
| Prelude compile peak RSS | 110.74 MiB | 88.80 MiB | 88.92 MiB |

**Structural type checks in +nest**

| Same-core cold replay | Previous review | Candidate |
|---|---:|---:|
| Wall time | 165.55 s | 165.31 s |
| User + system CPU time | 160.10 s | 159.72 s |
| Peak RSS | 1020.92 MiB | 1015.57 MiB |

<a id="change-e90b8c5efb"></a>

### Filter hashcons probes with spare slot bits

**Incremental.** Hash table fingerprints reduced checkpoint table rebuild time by 20.2% and 16.4% in the two restore cases. Full cold replay took 179.290 versus 182.605 s, a 1.8% slowdown.

- [e90b8c5efb](https://github.com/yapishu/vere/commit/e90b8c5efb54253daa05db6be8a2f2cdf5731183) — noun: filter hashcons probes with spare slot bits

Stack parent before this change is [caaf13c9fc](https://github.com/yapishu/vere/commit/caaf13c9fc063a5826603f4f5ac6892c619878dc).

**Checkpoint restore and first event**

| Metric | Previous review | Candidate | Reduction |
|---|---|---|---|
| Table rebuild | 1,533.500 ms | 1,224.000 ms | +20.2% |
| Full command elapsed | 1.765 s | 1.455 s | +17.6% |
| Full command CPU | 1.755 s | 1.450 s | +17.4% |
| Peak RSS | 238.779 MiB | 238.691 MiB | +0.0% |
| Box comparisons | 7,185,590 | 3,704,779 | +48.4% |

**Checkpoint restore and first event**

| Metric | Previous review | Candidate | Reduction |
|---|---|---|---|
| Table rebuild | 777.500 ms | 650.000 ms | +16.4% |
| Full command elapsed | 0.815 s | 0.680 s | +16.6% |
| Full command CPU | 0.810 s | 0.680 s | +16.0% |
| Peak RSS | 181.102 MiB | 181.098 MiB | +0.0% |
| Box comparisons | 2,198,320 | 143,903 | +93.5% |

**Compilers and cold replay**

| Metric | Previous review | Candidate | Reduction |
|---|---|---|---|
| Full cold replay elapsed | 179.290 s | 182.605 s | -1.8% |
| CPU | 179.025 s | 182.420 s | -1.9% |
| Peak RSS | 1,015.625 MiB | 1,015.449 MiB | +0.0% |

<a id="change-6194f5f2c2"></a>

### Retain hashcons fingerprints on wide looms

**Incremental.** Keeping fingerprints on wide looms improved restores across multiple loom reservations. Elapsed time and memory depend on reserved size and actual high offsets. The compiler and full replay controls include regressions.

- [6194f5f2c2](https://github.com/yapishu/vere/commit/6194f5f2c2f57d57604ce8887f7c4eff8f58e345) — noun: retain hashcons fingerprints on wide looms

Stack parent before this change is [e90b8c5efb](https://github.com/yapishu/vere/commit/e90b8c5efb54253daa05db6be8a2f2cdf5731183).

**Checkpoint and first event**

| Loom | Checkpoint | Review rebuild ms | Candidate rebuild ms | Reduction | Review whole s | Candidate whole s | Reduction |
|---|---|---|---|---|---|---|---|
| 32-bit / 2 GiB | 232 | 1,738.5 | 1,647 | +5.3% | 1.985 | 1.905 | +4.0% |
| 32-bit / 2 GiB | 240 | 945.5 | 878.5 | +7.1% | 0.99 | 0.92 | +7.1% |
| 64-bit / 2 GiB | 232 | 1,678 | 1,765.5 | -5.2% | 2.355 | 2.46 | -4.5% |
| 64-bit / 2 GiB | 240 | 667.5 | 636 | +4.7% | 0.7 | 0.67 | +4.3% |
| 64-bit / 64 GiB | 232 | 1,941.5 | 1,633 | +15.9% | 2.65 | 2.3 | +13.2% |
| 64-bit / 64 GiB | 240 | 704 | 676 | +4.0% | 0.74 | 0.715 | +3.4% |
| 64-bit / 128 GiB | 232 | 1,870 | 1,552.5 | +17.0% | 2.57 | 2.225 | +13.4% |
| 64-bit / 128 GiB | 240 | 717 | 599 | +16.5% | 0.755 | 0.635 | +15.9% |

**Checkpoint and first event**

| Loom | Checkpoint | Review comparisons | Candidate comparisons | Review RSS MiB | Candidate RSS MiB |
|---|---|---|---|---|---|
| 32-bit / 2 GiB | 232 | 3,704,779 | 3,597,922 | 241.453 | 241.314 |
| 32-bit / 2 GiB | 240 | 143,903 | 76,252 | 183.047 | 182.992 |
| 64-bit / 2 GiB | 232 | 3,559,176 | 3,480,631 | 531.906 | 531.922 |
| 64-bit / 2 GiB | 240 | 75,715 | 41,739 | 318.461 | 318.457 |
| 64-bit / 64 GiB | 232 | 8,576,098 | 3,559,176 | 532.391 | 532.404 |
| 64-bit / 64 GiB | 240 | 2,192,589 | 75,715 | 318.93 | 318.926 |
| 64-bit / 128 GiB | 232 | 8,576,098 | 3,559,176 | 532.891 | 532.904 |
| 64-bit / 128 GiB | 240 | 2,192,589 | 75,715 | 319.555 | 319.551 |

**Compiler times in seconds, with cold replay time and memory labeled by row**

| Workload | Develop | Previous review | Candidate | Reduction vs review |
|---|---|---|---|---|
| hoon | 19.851 | 11.064 | 11.296 | -2.1% |
| prelude | 19.827 | 12.288 | 12.268 | +0.2% |
| Cold elapsed s | 317.91 | 174.66 | 176.68 | -1.2% |
| Cold CPU s | 317.85 | 174.64 | 176.63 | -1.1% |
| Peak RSS MiB | 1,145.211 | 1,015.477 | 1,015.453 | +0.0% |

<a id="change-aa4bb8c2c5"></a>

### Carry treap ranks and share branches through point inserts

**Incremental.** Point insertion into a fresh Arvo map took 19.8% less time on the default loom. The retained map versions used 313.0 versus 298.2 KiB. Empty set insertion took longer in the replay call breakdown.

- [aa4bb8c2c5](https://github.com/yapishu/vere/commit/aa4bb8c2c52bb133432d36c6904f089bd7253501) — jets: carry treap ranks and share branches through point inserts

Stack parent before this change is [6194f5f2c2](https://github.com/yapishu/vere/commit/6194f5f2c2f57d57604ce8887f7c4eff8f58e345).

**Point insertions on real manifests**

| Loom bits | Fixture | Tree | Put sequence | Previous ms | Candidate ms | Less time |
|---|---|---|---|---|---|---|
| 32 | arvo | map | fresh | 0.581 | 0.466 | 19.8% |
| 32 | arvo | map | update | 0.488 | 0.372 | 23.8% |
| 32 | arvo | set | fresh | 0.552 | 0.455 | 17.6% |
| 32 | arvo | set | update | 0.460 | 0.256 | 44.4% |
| 32 | vere | map | fresh | 1.319 | 1.083 | 17.9% |
| 32 | vere | map | update | 1.021 | 0.281 | 72.5% |
| 32 | vere | set | fresh | 1.263 | 1.067 | 15.5% |
| 32 | vere | set | update | 1.050 | 0.279 | 73.4% |
| 64 | arvo | map | fresh | 0.572 | 0.448 | 21.6% |
| 64 | arvo | map | update | 0.484 | 0.364 | 24.9% |
| 64 | arvo | set | fresh | 0.543 | 0.424 | 21.9% |
| 64 | arvo | set | update | 0.456 | 0.246 | 45.9% |
| 64 | vere | map | fresh | 1.327 | 1.062 | 20.0% |
| 64 | vere | map | update | 1.059 | 0.291 | 72.6% |
| 64 | vere | set | fresh | 1.341 | 1.088 | 18.9% |
| 64 | vere | set | update | 1.070 | 0.277 | 74.1% |

**Retained versions**

| Loom bits | Fixture | Tree | Previous KiB | Candidate KiB | Less live memory |
|---|---|---|---|---|---|
| 32 | arvo | map | 313.0 | 298.2 | 4.7% |
| 32 | arvo | set | 215.7 | 167.8 | 22.2% |
| 32 | vere | map | 603.5 | 364.0 | 39.7% |
| 32 | vere | set | 494.5 | 244.8 | 50.5% |
| 64 | arvo | map | 523.1 | 493.6 | 5.6% |
| 64 | arvo | set | 398.8 | 303.1 | 24.0% |
| 64 | vere | map | 1,046.4 | 567.3 | 45.8% |
| 64 | vere | set | 914.2 | 414.6 | 54.6% |

**Calls inside Vere**

| Workload | Jet | Input tree | Calls sampled per run | Previous ns/call | Candidate ns/call | Less sampled time |
|---|---|---|---|---|---|---|
| Cold replay | in | empty | 42913 | 37.3 | 50.9 | -35.8% |
| Cold replay | in | nonempty | 60931 | 384.3 | 210.4 | 45.2% |
| Cold replay | in | all | 103844 | 240.9 | 144.5 | 40.0% |
| Cold replay | by | empty | 2943 | 126.1 | 169.3 | -33.7% |
| Cold replay | by | nonempty | 97 | 642.5 | 329.8 | 49.1% |
| Cold replay | by | all | 3040 | 142.6 | 174.4 | -21.8% |
| Hoon | in | empty | 3425 | 38.7 | 50.9 | -28.1% |
| Hoon | in | nonempty | 1962 | 442.0 | 167.9 | 61.4% |
| Hoon | in | all | 5387 | 186.9 | 93.5 | 48.8% |
| Hoon | by | empty | 208 | 129.7 | 160.1 | -23.5% |
| Hoon | by | nonempty | 6 | 666.3 | 260.2 | 62.2% |
| Hoon | by | all | 214 | 141.6 | 163.0 | -15.1% |
| Prelude | in | empty | 3773 | 40.0 | 45.6 | -14.2% |
| Prelude | in | nonempty | 2019 | 458.6 | 180.7 | 60.0% |
| Prelude | in | all | 5792 | 185.8 | 93.6 | 49.6% |
| Prelude | by | empty | 209 | 101.6 | 151.2 | -48.8% |
| Prelude | by | nonempty | 10 | 530.9 | 218.0 | 60.7% |
| Prelude | by | all | 219 | 120.5 | 155.2 | -28.8% |

<a id="change-76e0b84755"></a>

### Carry search hashes and borrow native map values

**Incremental.** The set-hit workload took 13.9% less time. Map lookups in the fresh Arvo fixture saved 23.8%. The Hoon compiler took 0.7% longer.

- [76e0b84755](https://github.com/yapishu/vere/commit/76e0b84755e36040b47e26c5fd15f7144dce0d91) — jets: carry search hashes and borrow native map values

Stack parent before this change is [aa4bb8c2c5](https://github.com/yapishu/vere/commit/aa4bb8c2c52bb133432d36c6904f089bd7253501).

**Live Dojo**

| 50,000 queries | Develop ms | Review ms | Candidate ms | Less vs review | Less vs develop | Paired wins |
| --- | --- | --- | --- | --- | --- | --- |
| set-hits | 39.34 | 35.80 | 30.82 | 13.9% | 21.7% | 7/9 |
| set-misses | 44.12 | 38.70 | 34.37 | 11.2% | 22.1% | 7/9 |
| map-hits | 51.77 | 48.81 | 43.07 | 11.8% | 16.8% | 8/9 |
| map-misses | 58.49 | 54.68 | 45.34 | 17.1% | 22.5% | 9/9 |
| cell-key-map | 57.42 | 58.54 | 47.36 | 19.1% | 17.5% | 9/9 |
| one-entry-misses | 11.32 | 7.43 | 6.76 | 9.1% | 40.3% | 9/9 |

**Repository path lookups**

| Loom and keys | Paths | Operation | Review ns/query | Candidate ns/query | Less time |
| --- | --- | --- | --- | --- | --- |
| 32 fresh | arvo | Map get | 262.1 | 199.7 | 23.8% |
| 32 fresh | arvo | Map membership | 248.5 | 179.2 | 27.9% |
| 32 fresh | arvo | Set membership | 245.3 | 170.9 | 30.3% |
| 32 fresh | arvo | Consuming map get | 267.7 | 191.2 | 28.6% |
| 32 fresh | vere | Map get | 426.8 | 371.7 | 12.9% |
| 32 fresh | vere | Map membership | 405.7 | 351.8 | 13.3% |
| 32 fresh | vere | Set membership | 396.6 | 344.3 | 13.2% |
| 32 fresh | vere | Consuming map get | 431.8 | 361.8 | 16.2% |
| 32 warm | arvo | Map get | 132.4 | 60.4 | 54.4% |
| 32 warm | arvo | Map membership | 126.4 | 50.2 | 60.3% |
| 32 warm | arvo | Set membership | 117.5 | 44.6 | 62.1% |
| 32 warm | arvo | Consuming map get | 138.6 | 56.4 | 59.3% |
| 32 warm | vere | Map get | 151.6 | 96.3 | 36.5% |
| 32 warm | vere | Map membership | 142.3 | 69.7 | 51.0% |
| 32 warm | vere | Set membership | 129.8 | 62.8 | 51.6% |
| 32 warm | vere | Consuming map get | 158.9 | 82.8 | 47.9% |
| 64 warm | arvo | Map get | 130.9 | 55.8 | 57.3% |
| 64 warm | arvo | Map membership | 131.8 | 43.9 | 66.7% |
| 64 warm | arvo | Set membership | 126.5 | 46.4 | 63.4% |
| 64 warm | arvo | Consuming map get | 136.6 | 52.2 | 61.8% |
| 64 warm | vere | Map get | 154.8 | 94.3 | 39.1% |
| 64 warm | vere | Map membership | 144.4 | 73.2 | 49.3% |
| 64 warm | vere | Set membership | 138.6 | 74.1 | 46.6% |
| 64 warm | vere | Consuming map get | 161.0 | 83.4 | 48.2% |

**Compiler and replay**

| Compile | Develop s | Review s | Candidate s | Less vs review | Less vs develop | Peak MiB, develop/review/candidate |
| --- | --- | --- | --- | --- | --- | --- |
| hoon | 19.22 | 10.94 | 11.01 | -0.7% | 42.7% | 113.1 / 91.6 / 91.7 |
| prelude | 19.63 | 11.59 | 11.46 | 1.1% | 41.6% | 110.8 / 88.8 / 88.8 |

<a id="change-42715575b4"></a>

### Refurbish faces along structural type paths

**Incremental.** Refurbishing faces on 1,024 distinct paired record types fell from 8.367 to 0.393 ms. Inclusive time for the measured calls inside cold replay fell 3.3%.

- [42715575b4](https://github.com/yapishu/vere/commit/42715575b403900f79b870cf4043b9bbe398ab2f) — jets: refurbish faces along structural type paths

Stack parent before this change is [76e0b84755](https://github.com/yapishu/vere/commit/76e0b84755e36040b47e26c5fd15f7144dce0d91).

**Ordinary Dojo and complete compiler jobs**

| Workload | Develop | Previous review | Candidate | Less time vs review |
|---|---:|---:|---:|---:|
| 1,024 distinct paired record types | 11.261 ms | 8.367 ms | 0.393 ms | +95.3% |
| 1,024 overlapping field-name stacks | 6.533 ms | 4.885 ms | 0.360 ms | +92.6% |
| Parse the complete Lull source | 311.038 ms | 221.557 ms | 210.390 ms | +5.0% |
| Compile Hoon | 20.995 s | 11.611 s | 11.517 s | +0.8% |
| Compile Prelude | 21.504 s | 11.732 s | 11.973 s | -2.1% |

**Actual calls inside compilation and replay**

| Enclosing job | All calls | Review time inside redo | Candidate time inside redo | Less time |
|---|---:|---:|---:|---:|
| Cold | 573,200 | 30.829 s | 29.824 s | +3.3% |
| Hoon | 44,686 | 3.972 s | 3.894 s | +2.0% |
| Prelude | 48,010 | 4.462 s | 4.352 s | +2.5% |

<a id="change-1a78049be3"></a>

### Kelvin 135 wing resolution and native arm search

**Multiple commits.** Together the Kelvin 135 field resolver and deferred arm-axis construction cut self-hosted Hoon compilation from 8.44 to 6.50 s and cold replay from 188.90 to 163.11 s. Searching for 512 absent names fell from 274 to 176 µs.

- [1a78049be3](https://github.com/yapishu/vere/commit/1a78049be3f12e8bab981b8271a842e965ba79f2) — jets: resolve structural wings in Kelvin 135
- [4247fa8a74](https://github.com/yapishu/vere/commit/4247fa8a7409b75323daf1fbe24cf56622510e35) — jets: build an arm axis only when the search finds its name

Stack parent before this group is [42715575b4](https://github.com/yapishu/vere/commit/42715575b403900f79b870cf4043b9bbe398ab2f).

**Ship workloads**

| Workload | Upstream develop | Previous review | Review | Less time vs previous review | Less time vs develop |
|---|---:|---:|---:|---:|---:|
| Compile the ship’s Hoon source | 11.08 s | 8.44 s | 6.50 s | 22.9% | 41.3% |
| Compile the ship’s Lull source | 8.74 s | 6.98 s | 5.28 s | 24.4% | 39.6% |
| Compile the CSV library with a fresh subject | 52.33 ms | 40.89 ms | 28.71 ms | 29.8% | 45.1% |
| Search eight chapters for 512 absent names | 263.5 µs | 274.0 µs | 176.0 µs | 35.8% | 33.2% |
| Resolve 13 compiler field names | 83.0 µs | 67.5 µs | 22.0 µs | 67.4% | 73.5% |
| Cold bootstrap and replay through event 143 | 325.58 s | 188.90 s | 163.11 s | 13.7% | 49.9% |

<a id="change-25b8c4aae9"></a>

### Refurbish recursive types through completed hold expansions

**Incremental.** Reusing completed hold expansions cut compiler evaluation of a running ship’s Hoon source from 6.390 to 5.687 s. The Kelvin 137 fixture supplies a separate comparison.

- [25b8c4aae9](https://github.com/yapishu/vere/commit/25b8c4aae992cff21ef82c85c99d50701bb9ea0c) — jets: refurbish recursive types through completed hold expansions

Stack parent before this change is [4247fa8a74](https://github.com/yapishu/vere/commit/4247fa8a7409b75323daf1fbe24cf56622510e35).

**Recursive type refurbishment in Vere**

| Workload | Develop | Previous review | Candidate | Less time vs previous | Less time vs develop |
|---|---:|---:|---:|---:|---:|
| Compile the running ship’s Hoon source | 11.106 s | 6.390 s | 5.687 s | 11.0% | 48.8% |
| Compile the running ship’s Lull source | 8.856 s | 5.364 s | 5.072 s | 5.5% | 42.7% |
| Name fields of 1,024 two-variant types | 6.278 ms | 4.502 ms | 0.571 ms | 87.3% | 90.9% |
| Expand and name 1,024 deferred types | 11.559 ms | 7.954 ms | 1.385 ms | 82.6% | 88.0% |
| Name 1,024 ordinary records | 11.256 ms | 0.386 ms | 0.390 ms | -1.0% | 96.5% |
| Join 1,024 overlapping name stacks | 6.523 ms | 0.363 ms | 0.363 ms | 0.0% | 94.4% |
| Cold bootstrap and replay through event 143 | 316.77 s | 149.20 s | 147.63 s | 1.1% | 53.4% |

**Shipped Kelvin 137 compiler inputs**

| Compiler | Previous review | Candidate | Less time |
|---|---:|---:|---:|
| Hoon | 10.721 s | 10.083 s | 5.9% |
| Prelude | 10.848 s | 10.017 s | 7.7% |

<a id="change-c553aef0c2"></a>

### Edit the type at a resolved wing for single-arm calls

**Incremental.** Editing at a resolved wing cut the live Hoon compile from 5.862 to 5.347 s, an 8.8% reduction.

- [c553aef0c2](https://github.com/yapishu/vere/commit/c553aef0c27b733910595776a84d29ecace9f1bc) — jets: edit the type at a resolved wing for single-arm calls

Stack parent before this change is [25b8c4aae9](https://github.com/yapishu/vere/commit/25b8c4aae992cff21ef82c85c99d50701bb9ea0c).

**Editing an arm's sample type in Vere**

| Workload | Develop | Previous review | Candidate | Less time vs review | Less time vs develop |
|---|---:|---:|---:|---:|---:|
| Compile Hoon in Dojo | 10.843 s | 5.862 s | 5.347 s | 8.8% | 50.7% |
| Compile Lull in Dojo | 8.814 s | 5.198 s | 4.499 s | 13.4% | 49.0% |
| Edit 1,024 gate samples | 5.1035 ms | 3.1970 ms | 0.3330 ms | 89.6% | 93.5% |
| Resolve 1,024 anonymous limbs | 0.6115 ms | 0.5365 ms | 0.2125 ms | 60.4% | 65.2% |
| Cold boot and replay through event 143 | 316.19 s | 144.45 s | 136.71 s | 5.4% | 56.8% |

<a id="change-a4fe1e6b6b"></a>

### Project type axes through forks and return natural wing matches

**Incremental.** Projecting 127 axes of the add gate type fell from 118.5 to 31.5 µs. The whole Hoon compiler fixture improved by 2.8%.

- [a4fe1e6b6b](https://github.com/yapishu/vere/commit/a4fe1e6b6bfaf8a0f7e7dfc569594bff1ecc2784) — jets: project type axes through forks and return natural wing matches

Stack parent before this change is [c553aef0c2](https://github.com/yapishu/vere/commit/c553aef0c27b733910595776a84d29ecace9f1bc).

**Compiler type projections and wing lookup**

| Workload | Develop | Previous review | Candidate | Less time vs review |
|---|---:|---:|---:|---:|
| 127 axes of the add gate type | 151.5 µs | 118.5 µs | 31.5 µs | 73.4% |
| 256 record projections | 148.0 µs | 109.0 µs | 42.0 µs | 61.5% |
| 256 four-way union projections | 676.0 µs | 519.0 µs | 163.5 µs | 68.5% |
| 256 named record fields | 439.0 µs | 86.5 µs | 57.0 µs | 34.1% |
| Live self-hosted Hoon | 10.898 s | 4.990 s | 4.844 s | 2.9% |
| Live Lull compile | 8.578 s | 4.372 s | 4.358 s | 0.3% |
| Cold replay through event 143 | 317.12 s | 140.59 s | 138.45 s | 1.5% |

**Compiler fixtures**

| Fixture | Previous review | Candidate | Change from review | Peak RSS, previous / candidate |
|---|---:|---:|---:|---:|
| Hoon | 9.491 s | 9.224 s | 2.8% less time | 91.57 / 92.11 MiB |
| Prelude | 9.645 s | 9.445 s | 2.1% less time | 88.72 / 89.27 MiB |

<a id="change-45a6ba3fa2"></a>

### Deduplicate checkpoint cells by their canonical children

**Incremental.** Deduplicating cells by canonical children cut event-232 melt from 3.50 to 2.82 s. The report also measures meld and peak process RSS.

- [45a6ba3fa2](https://github.com/yapishu/vere/commit/45a6ba3fa2ab3ba70fb8a19e0cebb8c57244acf9) — vere: deduplicate checkpoint cells by their canonical children

Stack parent before this change is [a4fe1e6b6b](https://github.com/yapishu/vere/commit/a4fe1e6b6bfaf8a0f7e7dfc569594bff1ecc2784).

**Release measurements**

| Event | Command | Develop | Prior review | Signed | Time saved vs review | Time saved vs develop | RSS develop / review / signed |
|---:|---|---:|---:|---:|---:|---:|---:|
| 232 | melt | 3.73 s | 3.50 s | 2.82 s | 19.4% | 24.4% | 607.93 / 606.50 / 606.45 |
| 232 | meld | 5.17 s | 4.80 s | 4.14 s | 13.8% | 19.9% | 821.34 / 818.63 / 818.54 |
| 249 | melt | 3.10 s | 2.98 s | 2.32 s | 22.1% | 25.2% | 606.66 / 606.71 / 606.75 |
| 249 | meld | 4.48 s | 4.26 s | 3.54 s | 16.9% | 21.0% | 766.06 / 766.10 / 765.96 |

<a id="change-c480f3d8d8"></a>

### Full-width atom export and dense jam position indexing

**Multiple commits, including a correctness fix.** The first commit preserves full loom words when exporting indirect atoms. The second indexes dense jam positions by cell ID. Event-232 cram used about 2.20 GiB versus 4.30 GiB before. The individual effect of the full-word fix is correctness on the wider loom.

- [c480f3d8d8](https://github.com/yapishu/vere/commit/c480f3d8d8a4e78de5c7fe762e4e724dd267c46d) — noun: copy full loom words when exporting indirect atoms
- [33bfbb2f82](https://github.com/yapishu/vere/commit/33bfbb2f8219285d593c82ecd9ec4b14312bda6a) — ur: index jam positions by cell ID for dense traversals

Stack parent before this group is [45a6ba3fa2](https://github.com/yapishu/vere/commit/45a6ba3fa2ab3ba70fb8a19e0cebb8c57244acf9).

**Whole-command timings**

| Fixture | Build | Runs | Wall | User CPU | Peak RSS |
|---|---|---:|---:|---:|---:|
| 232 | develop | 3 | 9.47 s | 6.80 s | 4.300 GiB |
| 232 | review | 3 | 9.14 s | 6.55 s | 4.299 GiB |
| 232 | signed | 3 | 6.50 s | 4.90 s | 2.201 GiB |
| 249 | develop | 3 | 8.81 s | 6.32 s | 4.300 GiB |
| 249 | review | 3 | 8.68 s | 6.13 s | 4.300 GiB |
| 249 | signed | 3 | 6.11 s | 4.56 s | 2.202 GiB |
| wide | fixed64 | 4 | 9.76 s | 7.05 s | 4.203 GiB |
| wide | signed64 | 4 | 7.09 s | 5.43 s | 2.119 GiB |

**Encoder and sparse inputs**

| Input | Invocation | Review | Candidate | Less time |
|---|---|---:|---:|---:|
| 232 rock | One-shot | 4290.9 ms | 750.9 ms | 82.5% |
| 232 rock | First on handle | 4126.1 ms | 681.6 ms | 83.5% |
| 232 rock | Second on handle | 2015.1 ms | 662.3 ms | 67.1% |
| Hoon result | One-shot | 27.8 ms | 8.7 ms | 68.7% |
| Hoon result | First on handle | 29.1 ms | 8.4 ms | 71.2% |
| Hoon result | Second on handle | 17.8 ms | 7.9 ms | 55.7% |

<a id="change-e958e6a74c"></a>

### Arena interning and ownership of duplicate atom buffers

**Multiple commits, including an ownership fix.** Canonical child-handle interning reduced event-232 checkpoint export wall time by 31.7% and peak RSS by 26.0% across this series. The second commit releases allocations when the arena already contains the same atom.

- [e958e6a74c](https://github.com/yapishu/vere/commit/e958e6a74c059728d3428a9bcba3ea570534adf0) — ur: intern cells by their canonical child handles
- [b9f0ab3163](https://github.com/yapishu/vere/commit/b9f0ab3163c5210e732dca6f9d2ecfc7b6e10a26) — ur: release duplicate atom buffers during interning

Stack parent before this group is [33bfbb2f82](https://github.com/yapishu/vere/commit/33bfbb2f8219285d593c82ecd9ec4b14312bda6a).

**Complete rock export**

| Fixture | Build | Runs | Wall | User CPU | Peak RSS |
|---|---|---:|---:|---:|---:|
| Event 232 | develop | 3 | 9.25 s | 6.75 s | 4.300 GiB |
| Event 232 | review | 3 | 6.44 s | 4.85 s | 2.201 GiB |
| Event 232 | release | 3 | 4.40 s | 3.30 s | 1.628 GiB |
| Event 249 | develop | 3 | 8.46 s | 6.03 s | 4.300 GiB |
| Event 249 | review | 3 | 5.95 s | 4.46 s | 2.201 GiB |
| Event 249 | release | 3 | 3.96 s | 2.91 s | 1.629 GiB |
| Event 232, wide | review64 | 4 | 6.83 s | 5.21 s | 2.119 GiB |
| Event 232, wide | release64 | 4 | 4.90 s | 3.69 s | 1.531 GiB |

**Complete rock export**

| Fixture | Less wall time vs previous review | Less peak RSS vs previous review |
|---|---:|---:|
| 232 | 31.7% | 26.0% |
| 249 | 33.4% | 26.0% |
| wide | 28.2% | 27.8% |

**Cell construction and lookup**

| Table | Build from empty | Repeat lookups |
|---|---:|---:|
| Published bucket table | 3.063 s | 1.842 s |
| Linear probing by mug | 1.893 s | 1.391 s |
| Mug plus fingerprint | 1.354 s | 1.041 s |
| Ordered handles | 1.305 s | 0.458 s |
| Ordered handles plus fingerprint, rotation mixer | 1.031 s | 0.227 s |
| Ordered handles plus fingerprint, production mixer | 1.021 s | 0.226 s |

**Independent off-loom decoder**

| Input | Previous review | Candidate | Less time |
|---|---:|---:|---:|
| rock | 9876.6 ms | 6501.1 ms | 34.2% |

<a id="change-64f3b3be29"></a>

### Backreference filtering and decoding from the event log buffer

**Multiple commits, distinct paths.** Filtering backreference targets cut off-loom event-232 rock decode from 5.7820 to 1.8200 s and peak RSS from 4,339.1 to 853.0 MiB. The direct log-buffer change also affects disk sifting. Its end-to-end result measures the integrated build.

- [64f3b3be29](https://github.com/yapishu/vere/commit/64f3b3be29fa1f6f1cfa2597900d445eb6157595) — cue: filter dictionary entries by backreference targets
- [a70aee1c00](https://github.com/yapishu/vere/commit/a70aee1c0041eee7731aaea2374963dfa2aa7b5f) — disk: cue event records directly from the log buffer

Stack parent before this group is [b9f0ab3163](https://github.com/yapishu/vere/commit/b9f0ab3163c5210e732dca6f9d2ecfc7b6e10a26).

**Backreference filtering and borrowed event records**

| Workload | Develop seconds | Previous review seconds | Release seconds | Less time vs review | Previous review peak MiB | Release peak MiB |
|---|---:|---:|---:|---:|---:|---:|
| Off-loom ur_cue, event-232 rock | 7.7608 | 5.7820 | 1.8200 | 68.5% | 4339.1 | 853.0 |
| Off-loom ur_cue, 143 events | 2.9477 | 2.4327 | 0.8003 | 67.1% | 1783.4 | 564.4 |
| u3_disk_sift, 143 LMDB records | — | 0.3086 | 0.2544 | 17.6% | 724.6 | 534.8 |
| Reusable u3s_cue_xeno_with, 143 events | — | 0.2761 | 0.2707 | 2.0% | 493.5 | 493.3 |
| One-shot u3s_cue_xeno, 143 events | — | 0.2812 | 0.2627 | 6.6% | 493.5 | 471.8 |
| u3s_cue_atom, 143 events | — | 0.2488 | 0.2482 | 0.2% | 685.1 | 684.9 |
| 1M two-byte messages, reusable pier decoder | — | 0.0544 | 0.0544 | -0.1% | — | — |
| 1M two-byte messages, reusable ur decoder | — | 0.0609 | 0.0533 | 12.3% | — | — |
| Complete event-232 rock import | 4.9900 | 2.3100 | 2.2900 | 0.9% | 773.4 | 684.9 |
| Complete checkpoint replay, events 233–249 | 1.5300 | 1.3100 | 1.3000 | 0.8% | 239.0 | 238.9 |

**Synthetic input shapes**

| Mode | Shape | Previous review ms | Release ms | Previous review peak MiB | Release peak MiB |
|---|---|---:|---:|---:|---:|
| xeno | unique | 7.71 | 7.79 | 20.9 | 20.9 |
| xeno | hot | 13.51 | 13.42 | 20.9 | 20.9 |
| xeno | spread | 20.26 | 20.09 | 20.9 | 20.9 |
| xeno | blob | 1.76 | 1.68 | 20.9 | 20.9 |
| once | unique | 7.76 | 7.86 | 20.9 | 20.9 |
| once | hot | 13.55 | 12.58 | 20.9 | 20.9 |
| once | spread | 20.37 | 23.10 | 20.9 | 20.9 |
| once | blob | 1.79 | 1.74 | 20.9 | 20.9 |

<a id="change-d15af74f1b"></a>

### Jam probe fingerprints and direct event-record output

**Multiple commits.** Jam probe fingerprints and packing into the destination reduced event-record encoding from 796.4 to 542.9 ms across 143 records. Persisting the events includes additional work and saved 7.4%.

- [d15af74f1b](https://github.com/yapishu/vere/commit/d15af74f1b8c75c402a0fe8b20ec6736bad76aa0) — noun: filter jam class probes with spare hash table bits
- [832e82e86c](https://github.com/yapishu/vere/commit/832e82e86cd3b22b547597037404a03b729e6f54) — noun: pack canonical jam directly into its destination

Stack parent before this group is [a70aee1c00](https://github.com/yapishu/vere/commit/a70aee1c0041eee7731aaea2374963dfa2aa7b5f).

**Jam class lookup and direct event serialization**

| Workload | Develop | Previous review | Release | Less vs review | Less vs develop |
|---|---:|---:|---:|---:|---:|
| Event record encoding, ms | 2581.5 | 796.4 | 542.9 | 31.8% | 79.0% |
| Other 142 records excluding the largest, ms | 81.33 | 47.65 | 38.24 | 19.7% | 53.0% |
| Hoon jam, ms | 2495.5 | 730.6 | 536.1 | 26.6% | 78.5% |
| IPC jam, ms | 2339.5 | 754.1 | 568.9 | 24.6% | 75.7% |
| 200,000 two-atom event records, ms | 31.89 | 17.16 | 10.75 | 37.3% | 66.3% |
| Two LMDB write batches, encoding and commit, s | 4.559 | 2.734 | 2.531 | 7.4% | 44.5% |
| LMDB transactions alone, s | 1.981 | 1.925 | 1.988 | -3.3% | -0.3% |
| Peak event encoder RSS, MiB | 1193.9 | 965.3 | 775.9 | 19.6% | 35.0% |
| Peak Hoon encoder RSS, MiB | 1030.4 | 801.8 | 802.2 | -0.0% | 22.1% |
| Peak IPC encoder RSS, MiB | 1045.6 | 775.5 | 775.9 | -0.0% | 25.8% |
| Peak full persist harness RSS, MiB | 1489.4 | 1260.7 | 1071.2 | 15.0% | 28.1% |
| Small interactive log records, µs/record | 0.991 | 0.463 | 0.322 | 30.6% | 67.6% |
| Complete replay of events 233–249, s | 1.49 | 1.31 | 1.29 | 1.5% | 13.4% |
| Complete replay peak RSS, MiB | 249.0 | 238.7 | 238.9 | -0.1% | 4.1% |

<a id="change-b0d1d5a303"></a>

### Identify adjacent parent classes by their postorder IDs

**Incremental.** Inferring adjacent parent classes cut encoding of the same 143 records from 560.6 to 368.0 ms, a further 34.4% against this run’s preceding review.

- [b0d1d5a303](https://github.com/yapishu/vere/commit/b0d1d5a303d2057931604d8f77f3026033adf442) — jam: identify adjacent parent classes by their postorder IDs

Stack parent before this change is [832e82e86c](https://github.com/yapishu/vere/commit/832e82e86cd3b22b547597037404a03b729e6f54).

**Postorder parent classes in jam**

| Workload | Develop | Previous review | New review | Less vs review | Less vs develop |
|---|---:|---:|---:|---:|---:|
| 143 event records, ms | 2832.6 | 560.6 | 368.0 | 34.4% | 87.0% |
| Other 142 records excluding the largest, ms | 81.65 | 38.97 | 34.64 | 11.1% | 57.6% |
| Hoon jam, ms | 2702.1 | 564.1 | 367.8 | 34.8% | 86.4% |
| IPC jam, ms | 2538.0 | 571.6 | 379.5 | 33.6% | 85.0% |
| 200,000 two-atom messages, ms | 31.96 | 10.76 | 10.23 | 4.9% | 68.0% |
| Encode and commit two LMDB batches, s | 4.497 | 2.109 | 2.151 | -2.0% | 52.2% |
| LMDB transactions alone, s | 1.871 | 1.542 | 1.788 | -15.9% | 4.5% |
| Event encoder peak RSS, MiB | 1193.8 | 775.6 | 775.7 | -0.0% | 35.0% |
| Hoon encoder peak RSS, MiB | 1030.3 | 801.9 | 802.0 | -0.0% | 22.2% |
| IPC encoder peak RSS, MiB | 1045.6 | 775.6 | 775.7 | -0.0% | 25.8% |
| Full persistence harness peak RSS, MiB | 1489.2 | 1070.9 | 1071.0 | -0.0% | 28.1% |
| Interactive records, µs per record | 0.965 | 0.300 | 0.287 | 4.2% | 70.2% |
| Complete replay of events 233–249, s | 1.57 | 1.31 | 1.32 | -0.8% | 15.9% |
| Complete replay peak RSS, MiB | 249.0 | 238.7 | 238.9 | -0.1% | 4.1% |

**Synthetic shapes**

| Workload | Develop | Previous review | New review | Less vs review | Less vs develop |
|---|---:|---:|---:|---:|---:|
| deep (ms) | — | 9.36 | 5.66 | 39.5% | — |
| distinct (ms) | 63.44 | 15.70 | 12.70 | 19.1% | 80.0% |
| duplicates (ms) | 54.11 | 15.37 | 10.50 | 31.7% | 80.6% |
| fanout (ms) | 79.52 | 21.51 | 19.18 | 10.8% | 75.9% |
| balanced (ms) | 43.18 | 12.14 | 8.68 | 28.5% | 79.9% |
| dag (ms) | — | 14.92 | 10.30 | 31.0% | — |

<a id="change-20dfb283a2"></a>

### Arena cell storage, reuse of mugs and integer width

**Multiple commits with an intermediate build.** On event 232, adjacent parent handles and independent array growth took export from 4.36 to 3.70 s. Reusing arena mugs took it to 3.37 s. Group peak RSS fell from 1,666.8 to 1,555.1 MiB. The last commit preserves integer width.

- [20dfb283a2](https://github.com/yapishu/vere/commit/20dfb283a22944a6cdf13d5a6106c23ce5dd35c4) — ur: infer adjacent parent handles during cell interning
- [0d31704e9c](https://github.com/yapishu/vere/commit/0d31704e9c481debbac3f22424fae5fd5abfe62d) — ur: grow the three cell arrays independently
- [8ec3b987b1](https://github.com/yapishu/vere/commit/8ec3b987b1b22b6c9fe0169f03c7aaf0bcd56c01) — noun: reuse arena mugs when rebuilding the loom
- [81d8dc1c7b](https://github.com/yapishu/vere/commit/81d8dc1c7ba40ac20ada33ed46c34984652bddaa) — ur: preserve integer width when interning atom bytes

Stack parent before this group is [b0d1d5a303](https://github.com/yapishu/vere/commit/b0d1d5a303d2057931604d8f77f3026033adf442).

**Checkpoint export**

| Checkpoint | Trials per build | Previous review | New review | Less wall time | Previous / new peak RSS |
|---|---:|---:|---:|---:|---:|
| Event 232, 32-bit | 5 | 4.360 s | 3.370 s | 22.7% | 1,666.8 / 1,555.1 MiB |
| Event 232, 64-bit | 4 | 4.875 s | 3.865 s | 20.7% | 1,567.7 / 1,455.9 MiB |
| Event 249, 32-bit | 4 | 3.830 s | 2.900 s | 24.3% | 1,667.6 / 1,556.0 MiB |

**Off-loom ur library**

| Workload | Previous review | New review | Less time | Previous / new peak RSS |
|---|---:|---:|---:|---:|
| 143 event records | 0.909 s | 0.533 s | 41.3% | 577.5 / 521.1 MiB |
| Entire rock | 2.041 s | 1.074 s | 47.4% | 853.0 / 734.0 MiB |
| 2M distinct two-atom pairs | 239.144 ms | 184.866 ms | 22.7% | 79.8 / 80.3 MiB |
| 2M draws from a bounded pair set | 243.014 ms | 222.459 ms | 8.5% | 79.8 / 80.3 MiB |
| 2M-cell list | 216.094 ms | 42.177 ms | 80.5% | 79.8 / 39.9 MiB |
| 2M-cell list with shared heads | 216.845 ms | 42.445 ms | 80.4% | 79.8 / 39.9 MiB |
| 2M mixed cell constructions | 223.886 ms | 74.085 ms | 66.9% | 79.8 / 43.8 MiB |
| 20,000 independent one-cell arenas | 3.821 ms | 4.180 ms | -9.4% | 21.5 / 21.5 MiB |

<a id="change-c1e8742808"></a>

### Peak memory during checkpoint export

**Multiple commits with intermediate builds.** The event-232 peak in exploratory stages was 1,555.0 MiB initially, 1,191.9 after discarding the empty loom, 998.0 after writing the rock before rebuilding, and 848.1 after releasing evacuated atom pages. The final five-run comparison measured 1,554.9 to 848.0 MiB with the same 3.720 s elapsed. The fourth commit tests restoration after a failed write.

- [c1e8742808](https://github.com/yapishu/vere/commit/c1e8742808cf951a2eedb63161c2b29e547e54aa) — noun: discard unused loom pages when replacing the home road
- [f41e4d8954](https://github.com/yapishu/vere/commit/f41e4d895477936d5aab373339a2b7f90bdb4869) — noun: write the rock before rebuilding the loom
- [ca7f073484](https://github.com/yapishu/vere/commit/ca7f0734842b8e52519e054062387851044a4874) — noun: release evacuated atom pages during checkpoint export
- [5c28ff0672](https://github.com/yapishu/vere/commit/5c28ff06723b1b2adc7cac29dfda646cea4400cf) — vere: check that a failed rock write restores shared atoms

Stack parent before this group is [81d8dc1c7b](https://github.com/yapishu/vere/commit/81d8dc1c7ba40ac20ada33ed46c34984652bddaa).

**Full commands**

| Checkpoint and command | Trials per build | Previous / new peak RSS | Less peak RSS | Previous / new wall time |
|---|---:|---:|---:|---:|
| Event 232, 32-bit cram | 5 | 1,554.9 / 848.0 MiB | 45.5% | 3.720 / 3.720 s |
| Event 232, 64-bit cram | 4 | 1,455.7 / 993.5 MiB | 31.8% | 4.895 / 4.930 s |
| Event 249, 32-bit cram | 4 | 1,555.8 / 848.1 MiB | 45.5% | 3.655 / 3.575 s |
| Event 232, 32-bit cram with --no-demand | 3 | 1,584.9 / 1,046.9 MiB | 33.9% | 4.400 / 4.530 s |
| Event 232, 32-bit queu | 3 | 684.7 / 663.8 MiB | 3.1% | 2.720 / 2.790 s |
| Event 232, 32-bit queu with --no-demand | 3 | 1,067.0 / 813.9 MiB | 23.7% | 2.730 / 2.790 s |

<a id="change-d3fccfaa24"></a>

### Split and concatenate byte strings in atom buffers

**Incremental.** Splitting lines in atom buffers fell from 258.0 to 193.5 µs. The source’s boot, replay and serialization totals cover the entire stack through this point.

- [d3fccfaa24](https://github.com/yapishu/vere/commit/d3fccfaa24c1aa92f6fe0ec0990c52d38f1d4051) — jets: split and concatenate byte strings in atom buffers

Stack parent before this change is [5c28ff0672](https://github.com/yapishu/vere/commit/5c28ff06723b1b2adc7cac29dfda646cea4400cf).

**Contribution of the byte-string change**

| Operation on the 164,363-byte Lull source | Develop | Previous review | New review | Reduction vs previous review |
|---|---:|---:|---:|---:|
| Split lines | 237.0 µs | 258.0 µs | 193.5 µs | 25.0% |
| Join lines | 763.5 µs | 660.5 µs | 542.0 µs | 17.9% |
| Tape to cord | 2,041.5 µs | 2,311.5 µs | 1,796.5 µs | 22.3% |
| Line tape round trips | 3,248.5 µs | 2,632.5 µs | 2,247.0 µs | 14.6% |

<a id="change-8629d8e655"></a>

### Build byte lists in batches from the road cell cache

**Incremental.** Batching cell allocation for byte lists cut conversion of Lull to a tape from 397 to 248 µs. The broader cumulative benchmark for this revision appears above.

- [8629d8e655](https://github.com/yapishu/vere/commit/8629d8e655c925204df56a6015231032394918a3) — noun: build byte lists in batches from the road cell cache

Stack parent before this change is [d3fccfaa24](https://github.com/yapishu/vere/commit/d3fccfaa24c1aa92f6fe0ec0990c52d38f1d4051).

**Kelvin 135 byte-list construction**

| Workload | Develop, µs | Previous review, µs | Candidate, µs | Less time vs previous |
|---|---:|---:|---:|---:|
| trip Lull | 378.0 | 397.0 | 248.0 | 37.5% |
| trip CSV | 5.0 | 6.0 | 5.0 | 16.7% |
| trip individual lines | 526.0 | 556.0 | 378.0 | 32.0% |
| line round trips | 3,046.0 | 2,251.0 | 2,126.0 | 5.6% |
| trip tiny cords | 186.0 | 191.0 | 175.0 | 8.4% |
| scow numbers | 1,606.0 | 1,373.0 | 1,220.0 | 11.1% |
| parse Lull | 292,987.0 | 229,123.0 | 226,696.0 | 1.1% |

<a id="change-6702faad73"></a>

### Reverse atom bytes directly in swp and rev

**Incremental.** Direct byte reversal cut the Lull workload from 592 to 31 µs, a 94.8% reduction.

- [6702faad73](https://github.com/yapishu/vere/commit/6702faad73a79a8c68b8584b7428f57329c8b4c4) — jets: reverse atom bytes directly in swp and rev

Stack parent before this change is [8629d8e655](https://github.com/yapishu/vere/commit/8629d8e655c925204df56a6015231032394918a3).

**Byte reversal in Vere on Kelvin 135**

| Workload (median µs) | Develop | Previous review | Review | Less time vs previous review |
|---|---:|---:|---:|---:|
| reverse Lull | 553 | 592 | 31 | 94.8% |
| reverse fixed width Lull | 564 | 637 | 32 | 95.0% |
| SHA-256 standard file digest | 900 | 948 | 392 | 58.6% |
| SHA-512 standard file digest | 765 | 816 | 251 | 69.2% |
| length-aware SHA-256 file digest | 930 | 981 | 401 | 59.1% |
| line digests | 5,456 | 5,239 | 4,083 | 22.1% |
| cord to SHA-256 standard integer | 1,452 | 1,543 | 406 | 73.7% |
| double SHA-256 on 32-byte messages | 19,199 | 18,215 | 15,037 | 17.4% |
| Base64 encode 4 KiB | 1,517 | 1,519 | 1,497 | 1.4% |
| direct shax control | 364 | 363 | 361 | 0.6% |
| reverse zero-padded addresses | 1,097 | 1,052 | 839 | 20.2% |

<a id="change-487f06c938"></a>

### Split newline-terminated cords with the shared byte scanner

**Incremental.** The shared scanner cut splitting Lull source into lines from 406 to 186 µs. The native table includes memory on large inputs.

- [487f06c938](https://github.com/yapishu/vere/commit/487f06c9388bddbba66e9cf62ef508edcb8612bb) — jets: split newline-terminated cords with the shared byte scanner

Stack parent before this change is [6702faad73](https://github.com/yapishu/vere/commit/6702faad73a79a8c68b8584b7428f57329c8b4c4).

**Unix line splitting on Kelvin 135**

| Workload | Develop (µs) | Previous review (µs) | Review (µs) | Less time vs previous review |
|---|---:|---:|---:|---:|
| split Lull source | 409 | 406 | 186 | 54.2% |
| split Hoon source | 1,148 | 1,135 | 532 | 53.1% |
| split Zuse source | 582 | 579 | 261 | 54.9% |
| split and rejoin Lull | 1,817 | 1,625 | 1,385 | 14.8% |
| split 4,096 seven-byte lines | 113 | 110 | 54 | 50.9% |
| split 4,096 empty lines | 27 | 27 | 15 | 44.4% |
| split 32 8-KiB lines | 348 | 342 | 54 | 84.2% |
| rejoin Lull control | 1,341 | 1,190 | 1,175 | 1.3% |

**Unix line splitting on Kelvin 135**

| Native workload | Previous review (ms) | Review (ms) | Previous peak RSS (MiB) | Review peak RSS (MiB) |
|---|---:|---:|---:|---:|
| 32 MiB, LF every 8,192 bytes | 55.15 | 15.20 | 81.55 | 81.55 |
| 4 MiB, LF every 8 bytes | 19.55 | 12.28 | 29.52 | 29.52 |
| 1 MiB, LF every 1 bytes | 10.08 | 6.30 | 18.58 | 18.58 |

<a id="change-ad0fcb22d4"></a>

### Prefetch hash-cons buckets while walking checkpoint cells

**Incremental.** Prefetching checkpoint hash-cons buckets cut a caught-up restart from 0.928 to 0.854 s over nine quiet rounds.

- [ad0fcb22d4](https://github.com/yapishu/vere/commit/ad0fcb22d46b552e2c9249365c9b5da0e9c53d81) — noun: prefetch hash-cons buckets while walking checkpoint cells

Stack parent before this change is [487f06c938](https://github.com/yapishu/vere/commit/487f06c9388bddbba66e9cf62ef508edcb8612bb).

**Checkpoint hash-cons table rebuild on Kelvin 135**

| Median | Develop | Previous review | Review head | Reduction vs previous review |
|---|---:|---:|---:|---:|
| Caught-up restart, 9 quiet rounds | 1.097 s | 0.928 s | 0.854 s | 8.0% |
| Rebuild within that restart | 556.00 ms | 460.00 ms | 338.00 ms | 26.5% |
| Memory at checked reply | 225.28 MiB | 218.38 MiB | 218.95 MiB | -0.3% |
| Saved checkpoint, events 233–249, 8 quiet rounds | 1.420 s | 1.215 s | 1.050 s | 13.6% |
| Peak RSS for those 17 events | 249.02 MiB | 238.67 MiB | 238.89 MiB | -0.1% |
| Caught-up restart, 9 rounds alongside other suites | 1.325 s | 1.099 s | 0.917 s | 16.5% |
| Rebuild within that restart | 732.00 ms | 607.00 ms | 430.00 ms | 29.2% |
| Memory at checked reply | 225.29 MiB | 218.34 MiB | 218.62 MiB | -0.1% |
| Launch at 233, catch up and answer in Dojo, 9 rounds | 1.458 s | 1.349 s | 1.165 s | 13.6% |
| Replay events 234–249, 12 rounds | 0.805 s | 0.800 s | 0.650 s | 18.8% |
| Peak RSS for those 16 events | 197.22 MiB | 190.15 MiB | 190.36 MiB | -0.1% |
| Saved checkpoint, events 233–249, 8 rounds | 1.845 s | 1.550 s | 1.275 s | 17.7% |
| Caught-up restart, 64-bit loom, 7 rounds | — | 1.250 s | 1.068 s | 14.6% |
| Memory at that 64-bit reply | — | 379.16 MiB | 379.55 MiB | -0.1% |
| Replay events 234–249, 64-bit loom, 9 rounds | — | 0.890 s | 0.720 s | 19.1% |
| Peak RSS for that 64-bit replay | — | 334.04 MiB | 334.48 MiB | -0.1% |
| Caught-up restart, two-build confirmation, 8 quiet rounds | — | 0.927 s | 0.853 s | 8.0% |
| Rebuild within that two-build confirmation | — | 463.50 ms | 339.50 ms | 26.8% |
| Event-233 checkpoint, two-build confirmation, 8 quiet rounds | — | 0.660 s | 0.540 s | 18.2% |

<a id="change-93fc95a579"></a>

### Build checkpoint hash-cons tables in home-bucket order

**Incremental.** Rebuilding tables in home-bucket order took another 20.4% off a caught-up restart, from 0.839 to 0.668 s. The log-only controls show the effect when no saved table is rebuilt.

- [93fc95a579](https://github.com/yapishu/vere/commit/93fc95a579c558d6a1ae61a325e0e56b8032b19f) — noun: build checkpoint hash-cons tables in home-bucket order

Stack parent before this change is [ad0fcb22d4](https://github.com/yapishu/vere/commit/ad0fcb22d46b552e2c9249365c9b5da0e9c53d81).

**Kelvin 135 checkpoint rebuild**

| Median | Develop | Previous review | Review | Change vs develop | Change vs previous | Runs per build |
|---|---:|---:|---:|---:|---:|---:|
| Restart caught-up ship at event 249 | 1.082 s | 0.839 s | 0.668 s | 38.3% less | 20.4% less | 9 |
| Rebuild within that restart | 542.0 ms | 335.0 ms | 161.0 ms | 70.3% less | 51.9% less | 9 |
| King and worker PSS at reply | 225.2 MiB | 218.7 MiB | 219.3 MiB | 2.6% less | 0.3% more | 9 |
| Reopen event 233 and play events 234–249 | 0.630 s | 0.540 s | 0.370 s | 41.3% less | 31.5% less | 10 |
| Peak RSS on that replay | 197.2 MiB | 190.3 MiB | 190.5 MiB | 3.4% less | 0.1% more | 10 |
| Checkpoint at event 232, play 233–249 | 1.380 s | 1.030 s | 1.035 s | 25.0% less | 0.5% more | 8 |
| Peak RSS for checkpoint replay | 249.0 MiB | 238.8 MiB | 238.8 MiB | 4.1% less | 0.0% more | 8 |
| Restart a checkpoint after its first pill boot | — | 0.855 s | 0.652 s | — | 23.7% less | 7 |
| Restart a checkpoint after compiling Lull | — | 0.838 s | 0.671 s | — | 20.0% less | 7 |
| Restart with a planted duplicate | — | 0.836 s | 0.904 s | — | 8.2% more | 7 |
| King and worker PSS on duplicate | — | 218.7 MiB | 218.9 MiB | — | 0.1% more | 7 |
| Restart with an unmugged root | — | 0.848 s | 0.840 s | — | 1.0% less | 7 |
| King and worker PSS on unmugged root | — | 218.7 MiB | 219.1 MiB | — | 0.2% more | 7 |
| 64-bit loom, restart event 249 | — | 0.883 s | 0.702 s | — | 20.5% less | 7 |
| 64-bit king and worker PSS at reply | — | 379.7 MiB | 380.0 MiB | — | 0.1% more | 7 |
| 64-bit loom, reopen 233 and play through 249 | — | 0.640 s | 0.460 s | — | 28.1% less | 8 |
| 64-bit replay peak RSS | — | 334.4 MiB | 334.6 MiB | — | 0.1% more | 8 |

**Log-only control**

| Pair | Previous review wall | Review wall | Previous review user CPU | Review user CPU |
|---|---:|---:|---:|---:|
| 1 | 148.55 s | 146.63 s | 147.69 s | 145.94 s |
| 2 | 142.29 s | 144.74 s | 141.69 s | 143.98 s |

<a id="change-e4a3c6800a"></a>

### Noun ordering, verified sorting and equal shared graphs

**Multiple commits with a registration-only control.** The complete Clay root listing took 4.603 ms before this group, 2.154 ms with the registration-only build and 1.984 ms on the integrated build. The middle column measures the benefit already available by registering the existing implementation. The full result includes first-difference ordering, verified sort and shared-graph comparison.

- [e4a3c6800a](https://github.com/yapishu/vere/commit/e4a3c6800afc16fd074cd39fc7348bc3b666fd66) — jets: compare the first differing noun fields for aor and dor
- [60c01815cd](https://github.com/yapishu/vere/commit/60c01815cd1d010518fde21eea049fab937bc1f7) — jets: sort verified noun orders with normalized prefixes
- [d9a65c37d2](https://github.com/yapishu/vere/commit/d9a65c37d23cfc6f8af7e7c89518fa73259efb63) — jets: compare equal shared noun graphs in a single walk

Stack parent before this group is [93fc95a579](https://github.com/yapishu/vere/commit/93fc95a579c558d6a1ae61a325e0e56b8032b19f).

**Noun ordering on Kelvin 135**

| Median | Develop | Previous review | Registration alone | Review | Change vs previous | Change vs registration |
|---|---:|---:|---:|---:|---:|---:|
| Complete Clay root file listing | 6.082 ms | 4.603 ms | 2.154 ms | 1.984 ms | 56.9% less | 7.9% less |
| Complete Clay lib file listing | 1.974 ms | 1.393 ms | 1.095 ms | 1.080 ms | 22.5% less | 1.4% less |
| Clay base paths, alphabetical | 3.529 ms | 2.733 ms | 0.303 ms | 0.138 ms | 95.0% less | 54.5% less |
| Clay library paths, alphabetical | 0.430 ms | 0.341 ms | 0.050 ms | 0.029 ms | 91.5% less | 42.0% less |
| Clay base paths, numeric noun order | 0.245 ms | 0.284 ms | 0.284 ms | 0.124 ms | 56.3% less | 56.3% less |
| Lull source lines, alphabetical | 47.293 ms | 33.582 ms | 0.541 ms | 0.120 ms | 99.6% less | 77.8% less |
| Clay path records, numeric noun order | 0.275 ms | 0.349 ms | 0.347 ms | 0.154 ms | 55.9% less | 55.6% less |
| Listed paths, already ordered | 71.915 ms | 53.530 ms | 4.054 ms | 0.070 ms | 99.9% less | 98.3% less |
| Listed paths, reverse order | 72.136 ms | 53.477 ms | 3.944 ms | 0.122 ms | 99.8% less | 96.9% less |
| Sort and format Clay paths | 3.918 ms | 3.014 ms | 0.581 ms | 0.411 ms | 86.4% less | 29.3% less |
| Full Lull source lines | 365.430 ms | 258.637 ms | 4.344 ms | 0.708 ms | 99.7% less | 83.7% less |
| Custom comparator control | 3.983 ms | 3.076 ms | 0.642 ms | 0.604 ms | 80.4% less | 5.9% less |
| Atom comparator control | 2.057 ms | 2.203 ms | 2.190 ms | 2.210 ms | 0.3% more | 0.9% more |

**Memory**

| PSS | Develop | Previous review | Registration alone | Review | Change vs previous | Change vs registration |
|---|---:|---:|---:|---:|---:|---:|
| King and worker PSS after sorting %base paths | 258.45 MiB | 252.38 MiB | 252.09 MiB | 252.64 MiB | 0.1% more | 0.2% more |
| King and worker PSS after sorting the Lull source | 258.45 MiB | 252.38 MiB | 252.09 MiB | 252.64 MiB | 0.1% more | 0.2% more |

<a id="change-d1bab7c2e9"></a>

### Compile axis edits in a compressed binary trie

**Incremental.** Replaying 205,042 actual compiler calls to the axis-edit builder fell from 363.85 to 6.86 ms. Whole self-hosted Hoon compile took 4.709 versus 4.493 s, a 4.6% saving.

- [d1bab7c2e9](https://github.com/yapishu/vere/commit/d1bab7c2e9a34fba7d5b560b513aed10546f8f9a) — jets: compile axis edits in a compressed binary trie

Stack parent before this change is [d9a65c37d2](https://github.com/yapishu/vere/commit/d9a65c37d23cfc6f8af7e7c89518fa73259efb63).

**Compiler axis edits on Kelvin 135**

| Workload | Develop | Previous review | Candidate | Change vs previous review |
|---|---:|---:|---:|---:|
| self-hosted-hoon | 10.546 s | 4.709 s | 4.493 s | 4.6% less |
| compile-lull | 8.394 s | 4.240 s | 3.939 s | 7.1% less |

**Compiler axis edits on Kelvin 135**

| Workload | Develop | Previous review | Candidate | Change vs previous review |
|---|---:|---:|---:|---:|
| self-hosted-hoon | 283.68 MiB | 279.66 MiB | 280.16 MiB | 0.2% more |
| compile-lull | 285.94 MiB | 288.51 MiB | 289.03 MiB | 0.2% more |

**Calls from the real compiler workload**

| Loom | Calls | Hoon battery | Native jet | Reduction |
|---|---:|---:|---:|---:|
| 32-bit | 205,042 | 363.85 ms | 6.86 ms | 98.1% less |
| 64-bit | 205,042 | 355.26 ms | 7.28 ms | 98.0% less |

<a id="change-a56b375a03"></a>

### Build private JSON object maps by treap splitting

**Incremental.** Building private JSON object maps by splitting reduced live decode of 27 documents from 9.749 to 8.601 ms. PSS at the corpus reply rose from 230.67 to 231.12 MiB.

- [a56b375a03](https://github.com/yapishu/vere/commit/a56b375a0320e4389e80a462cf0c7e27f84974d1) — jets: build private JSON object maps by treap splitting

Stack parent before this change is [d1bab7c2e9](https://github.com/yapishu/vere/commit/d1bab7c2e9a34fba7d5b560b513aed10546f8f9a).

**Live gate time in milliseconds, default 32-bit loom**

| Workload | Develop | Previous review | Review | Change vs previous review |
|---|---:|---:|---:|---:|
| all 27 documents | 11.415 | 9.749 | 8.601 | 11.8% less |
| init | 5.287 | 4.376 | 3.500 | 20.0% less |
| groupsInit | 2.528 | 2.248 | 2.099 | 6.6% less |
| contactsDirectory | 0.737 | 0.640 | 0.614 | 4.1% less |
| openapi | 0.374 | 0.329 | 0.320 | 2.7% less |
| channelPosts | 0.279 | 0.251 | 0.249 | 0.8% less |
| activityInitFeeds | 0.104 | 0.092 | 0.096 | 4.3% more |
| pins | 0.006 | 0.006 | 0.006 | 0.0% less |
| decode and encode group initialization | 4.243 | 3.405 | 3.326 | 2.3% less |

**Summed king and worker PSS at corpus reply in MiB**

| Workload | Develop | Previous review | Review | Change vs previous review |
|---|---:|---:|---:|---:|
| all 27 documents | 231.00 | 230.67 | 231.12 | 0.2% more |

<a id="change-6de113961a"></a>

### Scan buffered ASCII strings and whitespace in runs

**Incremental.** Scanning plain bytes in runs reduced live parse of the same source JSON by a further 46.2%. Compact versions of the documents saw a 41.1% reduction.

- [6de113961a](https://github.com/yapishu/vere/commit/6de113961a19eaf2873d3bbdaf9ac5eeb88f1cd8) — pdjson: scan buffered ASCII strings and whitespace in runs

Stack parent before this change is [a56b375a03](https://github.com/yapishu/vere/commit/a56b375a0320e4389e80a462cf0c7e27f84974d1).

**Source JSON in a running ship**

| Workload | Develop | Previous review | Review | Less time vs develop | Change vs previous review |
|---|---:|---:|---:|---:|---:|
| all 27 documents | 11.458 ms | 8.648 ms | 4.655 ms | 59.4% less | 46.2% less |
| init | 5.315 ms | 3.515 ms | 1.690 ms | 68.2% less | 51.9% less |
| groupsInit | 2.556 ms | 2.120 ms | 1.191 ms | 53.4% less | 43.8% less |
| contactsDirectory | 0.740 ms | 0.623 ms | 0.381 ms | 48.5% less | 38.8% less |
| openapi | 0.377 ms | 0.323 ms | 0.198 ms | 47.5% less | 38.7% less |
| channelPosts | 0.278 ms | 0.252 ms | 0.135 ms | 51.4% less | 46.4% less |
| activityInitFeeds | 0.105 ms | 0.097 ms | 0.049 ms | 53.3% less | 49.5% less |
| pins | 0.007 ms | 0.008 ms | 0.007 ms | 0.0% less | 12.5% less |
| decode and encode group initialization | 4.318 ms | 3.568 ms | 2.608 ms | 39.6% less | 26.9% less |

**Compact JSON in a running ship**

| Workload | Develop | Previous review | Review | Less time vs develop | Change vs previous review |
|---|---:|---:|---:|---:|---:|
| all 27 documents | 10.220 ms | 7.354 ms | 4.331 ms | 57.6% less | 41.1% less |
| init | 4.977 ms | 3.174 ms | 1.607 ms | 67.7% less | 49.4% less |
| groupsInit | 2.176 ms | 1.711 ms | 1.079 ms | 50.4% less | 36.9% less |
| contactsDirectory | 0.627 ms | 0.516 ms | 0.345 ms | 45.0% less | 33.1% less |
| openapi | 0.287 ms | 0.239 ms | 0.173 ms | 39.7% less | 27.6% less |
| channelPosts | 0.216 ms | 0.192 ms | 0.116 ms | 46.3% less | 39.6% less |
| activityInitFeeds | 0.081 ms | 0.076 ms | 0.041 ms | 49.4% less | 46.1% less |
| pins | 0.007 ms | 0.008 ms | 0.007 ms | 0.0% less | 12.5% less |
| decode and encode group initialization | 3.907 ms | 3.062 ms | 2.541 ms | 35.0% less | 17.0% less |

**Retained ship memory**

| Build | Before decode | With all decoded documents held |
|---|---:|---:|
| develop, 32-bit loom | 221.26 MiB | 255.52 MiB |
| previous review, 32-bit loom | 220.91 MiB | 255.68 MiB |
| review, 32-bit loom | 220.93 MiB | 255.70 MiB |
| previous review, 64-bit loom | 381.92 MiB | 402.48 MiB |
| review, 64-bit loom | 381.88 MiB | 402.44 MiB |

<a id="change-783265084f"></a>

### Advance the cache clock through occupied HAMT children

**Incremental.** Advancing the clock through occupied children cut puts in the 512-entry table by 41.8%. Fake-ship cold replay took 142.94 versus 142.19 s, a 0.5% reduction.

- [783265084f](https://github.com/yapishu/vere/commit/783265084f58ee9361c9b4b094168dfbcee3067e) — noun: advance the cache clock through occupied HAMT children

Stack parent before this change is [6de113961a](https://github.com/yapishu/vere/commit/6de113961a19eaf2873d3bbdaf9ac5eeb88f1cd8).

**Occupied-child traversal for bounded Vere caches**

| Full workload, median | Develop | Previous review | Review | Reduction vs previous |
|---|---:|---:|---:|---:|
| Full log through event 143 | 327.690 s | 142.940 s | 142.190 s | 0.5% |
| Full log, user CPU | 322.530 s | 142.280 s | 141.530 s | 0.5% |
| Full log, peak RSS | 1145.164 MiB | 835.613 MiB | 835.762 MiB | -0.0% |
| compile-lull | 9.783 s | 4.481 s | 4.442 s | 0.9% |
| self-hosted-hoon | 12.412 s | 5.126 s | 5.089 s | 0.7% |

**Eviction and trimming**

| Workload | Previous review | Review | Less time |
|---|---:|---:|---:|
| 32-bit 512-entry table, put | 5.587 ms | 3.254 ms | 41.8% |
| 32-bit 512-entry table, half | 0.025 ms | 0.010 ms | 58.9% |
| 32-bit 512-entry table, rest | 0.026 ms | 0.009 ms | 67.6% |
| 32-bit 16,384-entry table, put | 31.265 ms | 19.766 ms | 36.8% |
| 32-bit 16,384-entry table, half | 0.884 ms | 0.356 ms | 59.7% |
| 32-bit 16,384-entry table, rest | 0.771 ms | 0.271 ms | 64.8% |
| 32-bit 200,000-entry table, put | 181.972 ms | 113.447 ms | 37.7% |
| 32-bit 200,000-entry table, half | 21.692 ms | 8.535 ms | 60.7% |
| 32-bit 200,000-entry table, rest | 11.722 ms | 6.169 ms | 47.4% |
| 64-bit 512-entry table, put | 5.676 ms | 3.341 ms | 41.1% |
| 64-bit 512-entry table, half | 0.025 ms | 0.010 ms | 59.0% |
| 64-bit 512-entry table, rest | 0.026 ms | 0.009 ms | 67.0% |
| 64-bit 16,384-entry table, put | 30.256 ms | 19.714 ms | 34.8% |
| 64-bit 16,384-entry table, half | 0.868 ms | 0.357 ms | 58.8% |
| 64-bit 16,384-entry table, rest | 0.755 ms | 0.279 ms | 63.0% |
| 64-bit 200,000-entry table, put | 208.321 ms | 153.669 ms | 26.2% |
| 64-bit 200,000-entry table, half | 23.946 ms | 13.742 ms | 42.6% |
| 64-bit 200,000-entry table, rest | 15.554 ms | 11.533 ms | 25.8% |
| event232, ford, phase 1 | 5.496 ms | 5.546 ms | -0.9% |
| event232, ford, phase 2 | 26.242 ms | 29.247 ms | -11.5% |
| event232, per, phase 1 | 12.491 ms | 13.198 ms | -5.7% |
| event232, per, phase 2 | 23.364 ms | 28.726 ms | -22.9% |
| json, ford, phase 1 | 10.448 ms | 10.319 ms | 1.2% |
| json, ford, phase 2 | 39.325 ms | 41.578 ms | -5.7% |
| json, per, phase 1 | 30.867 ms | 29.007 ms | 6.0% |
| json, per, phase 2 | 50.012 ms | 50.746 ms | -1.5% |
| wide232, ford, phase 1 | 6.387 ms | 5.560 ms | 13.0% |
| wide232, ford, phase 2 | 31.221 ms | 32.619 ms | -4.5% |
| wide232, per, phase 1 | 12.996 ms | 13.285 ms | -2.2% |
| wide232, per, phase 2 | 27.421 ms | 27.590 ms | -0.6% |

**Repeated snapshot trims on dedicated cores**

| Fixture | Cache | Phase | Previous P-core | Review P-core | Change | Previous E-core | Review E-core | Change |
|---|---|---|---:|---:|---:|---:|---:|---:|
| event232 | ford | 1 | 5.549 ms | 4.727 ms | 14.8% | 7.216 ms | 6.067 ms | 15.9% |
| event232 | ford | 2 | 26.737 ms | 25.718 ms | 3.8% | 36.860 ms | 34.318 ms | 6.9% |
| event232 | per | 1 | 11.456 ms | 11.108 ms | 3.0% | 14.970 ms | 14.794 ms | 1.2% |
| event232 | per | 2 | 23.790 ms | 22.716 ms | 4.5% | 31.695 ms | 31.173 ms | 1.6% |
| json | ford | 1 | 9.284 ms | 8.122 ms | 12.5% | 13.689 ms | 12.377 ms | 9.6% |
| json | ford | 2 | 32.769 ms | 32.232 ms | 1.6% | 44.592 ms | 44.862 ms | -0.6% |
| json | per | 1 | 26.348 ms | 26.568 ms | -0.8% | 39.958 ms | 40.702 ms | -1.9% |
| json | per | 2 | 42.412 ms | 42.108 ms | 0.7% | 60.731 ms | 62.307 ms | -2.6% |
| wide232 | ford | 1 | 5.403 ms | 4.708 ms | 12.9% | 7.060 ms | 6.387 ms | 9.5% |
| wide232 | ford | 2 | 27.371 ms | 27.169 ms | 0.7% | 33.538 ms | 35.034 ms | -4.5% |
| wide232 | per | 1 | 11.026 ms | 10.375 ms | 5.9% | 13.820 ms | 13.839 ms | -0.1% |
| wide232 | per | 2 | 22.259 ms | 22.640 ms | -1.7% | 29.020 ms | 30.184 ms | -4.0% |

<a id="change-091cd3074c"></a>

### Reap inherited programs only when a jet site changes

**Incremental.** Reaping inherited programs only when a jet site changes cut the measured moon replay loop from 38.59 to 37.55 s. Self-hosted Hoon compilation saved 5.5%.

- [091cd3074c](https://github.com/yapishu/vere/commit/091cd3074c57dce5835240fb4c5c02ea5cdf29e5) — nock: reap inherited programs only when a jet site changes

Stack parent before this change is [783265084f](https://github.com/yapishu/vere/commit/783265084f58ee9361c9b4b094168dfbcee3067e).

**Moon**

| Measure | Develop | Previous review | New review | Reduction vs previous |
|---|---:|---:|---:|---:|
| Replay event loop | 59.90 s | 38.59 s | 37.55 s | +2.7% |
| Whole replay command | 68.00 s | 43.68 s | 42.61 s | +2.4% |
| Peak process RSS | 896.12 MiB | 920.97 MiB | 920.97 MiB | +0.0% |

**Other workloads**

| Measure | Previous review | Candidate | Reduction |
|---|---:|---:|---:|
| Self-hosted Hoon compile | 5.309 s | 5.015 s | +5.5% |
| Lull compile | 4.453 s | 4.337 s | +2.6% |
| Cold ~zod replay, wall time | 140.840 s | 137.200 s | +2.6% |
| Cold ~zod replay, user CPU | 140.210 s | 136.490 s | +2.7% |
| Cold ~zod replay, peak RSS | 835.762 MiB | 835.406 MiB | +0.0% |

<a id="change-09b3f06281"></a>

### Merge dense batches into populated maps and sets

**Incremental.** Dense batch updates cut the live set workload by 83.1%. A single change in the Clay workload took 1.1% longer. The table separates batch sizes.

- [09b3f06281](https://github.com/yapishu/vere/commit/09b3f062811c3cb43bb13c91877dba953c1da4f1) — jets: merge dense batches into populated maps and sets

Stack parent before this change is [091cd3074c](https://github.com/yapishu/vere/commit/091cd3074c57dce5835240fb4c5c02ea5cdf29e5).

**Running Clay**

| Desk revisions | Develop ms | Previous review ms | Candidate ms | Less time vs review |
|---:|---:|---:|---:|---:|
| 1 | 2.372 | 1.757 | 1.776 | -1.1% |
| 5 | 5.325 | 3.317 | 3.057 | 7.8% |
| 17 | 14.540 | 8.889 | 7.753 | 12.8% |

**Live Dojo batch operations**

| Operation | Develop µs | Previous review µs | Candidate µs | Less time vs review |
|---|---:|---:|---:|---:|
| set-dense | 16,330.5 | 10,441.0 | 1,763.5 | 83.1% |
| map-dense | 18,207.0 | 15,231.0 | 2,181.5 | 85.7% |
| set-unchanged | 12,395.5 | 4,799.0 | 1,307.5 | 72.8% |
| map-unchanged | 12,714.5 | 4,665.5 | 1,509.0 | 67.7% |
| set-repeated | 6,959.5 | 3,751.5 | 601.5 | 84.0% |
| set-sparse | 55.5 | 39.5 | 40.0 | -1.3% |
| set-256 | 285.5 | 177.5 | 177.0 | 0.3% |
| set-2048 | 2,057.5 | 987.5 | 987.0 | 0.1% |
| set-tiny | 14.0 | 12.0 | 11.5 | 4.2% |
| compiler-arm-map | 5.0 | 4.0 | 4.0 | 0.0% |

**Peak memory**

| Loom and operation | Previous review MiB | Candidate MiB |
|---|---:|---:|
| 32 set | 16.27 | 16.27 |
| 32 map | 26.38 | 16.27 |
| 64 set | 28.54 | 16.12 |
| 64 map | 49.95 | 16.12 |

<a id="change-e402da877a"></a>

### Refurbish recursive list types in one pass

**Incremental.** Refurbishing recursive list types in one pass cut the HTML entity module compile from 12.159 to 0.102 s. This is a specific real module that spent heavily on these types.

- [e402da877a](https://github.com/yapishu/vere/commit/e402da877aefc62970777aca019682a672c76501) — jets: refurbish recursive list types in one pass

Stack parent before this change is [09b3f06281](https://github.com/yapishu/vere/commit/09b3f062811c3cb43bb13c91877dba953c1da4f1).

**Refurbishing recursive list types in Vere**

| Workload | Previous review | Candidate | Reduction |
|---|---:|---:|---:|
| HTML entity module, seconds | 12.159 | 0.102 | +99.2% |
| Cold replay to event 143, seconds | 123.45 | 116.46 | +5.7% |
| Self-hosted Hoon compile, seconds | 4.522 | 4.417 | +2.3% |
| Lull compile, seconds | 3.803 | 3.832 | -0.7% |
| Post-HTML compile king + worker PSS, MiB | 298.2 | 275.7 | +7.6% |
| Cold replay peak process RSS, MiB | 835.7 | 835.6 | +0.0% |

<a id="change-4942a7dd88"></a>

### Share field reads when mapping records with turn

**Incremental.** Shared field reads cut the notes-members query with 128 members by 28.0%. Self-hosted Hoon compilation took 4.269 versus 4.389 s.

- [4942a7dd88](https://github.com/yapishu/vere/commit/4942a7dd88b550fb5238227631410fa46563acb9) — jets: share field reads when mapping records with turn

Stack parent before this change is [e402da877a](https://github.com/yapishu/vere/commit/e402da877aefc62970777aca019682a672c76501).

**Fake ship application queries**

| Query body (µs) | Develop | Previous review | Candidate | Less time vs review |
|---|---:|---:|---:|---:|
| Notes members, 128 | 11.0 | 12.5 | 9.0 | 28.0% |
| Notes invites, 128 | 12.5 | 11.0 | 8.5 | 22.7% |
| Notes members, 8,192 | 489.5 | 381.0 | 242.0 | 36.5% |
| Notes invites, 8,192 | 558.5 | 498.0 | 318.5 | 36.0% |
| Notes published, 8,192 | 695.0 | 604.0 | 490.0 | 18.9% |
| Captured event tag, 8,192 | 238.5 | 236.0 | 73.0 | 69.1% |

**Record transformations**

| Row transformation (µs) | Develop | Previous review | Candidate | Less time vs review |
|---|---:|---:|---:|---:|
| Six fields reordered | 5,604.5 | 5,277.0 | 3,934.5 | 25.4% |
| Sparse fields | 4,485.5 | 4,120.5 | 3,209.0 | 22.1% |
| Repeated fields | 5,691.5 | 4,948.0 | 3,469.5 | 29.9% |
| Rebuild sibling record | 4,801.0 | 4,842.0 | 2,504.5 | 48.3% |
| Fields and literals | 5,889.5 | 5,253.5 | 3,462.5 | 34.1% |
| One field | 4,072.5 | 2,492.0 | 2,477.5 | 0.6% |

**Compiler and replay**

| Workload | Previous review | Candidate |
|---|---:|---:|
| self-hosted-hoon | 4.269 s | 4.389 s |
| compile-lull | 3.805 s | 3.680 s |

<a id="change-cc0ddf6b53"></a>

### Batch independent reads when flattening large maps and sets

**Incremental.** Overlapping independent reads cut flattening the member map from 214 to 83 µs. A control with 2,048 independently built tiny maps took 1.8% longer.

- [cc0ddf6b53](https://github.com/yapishu/vere/commit/cc0ddf6b5371203dc514b5ba63bd046fd913273a) — jets: batch independent reads when flattening large maps and sets

Stack parent before this change is [4942a7dd88](https://github.com/yapishu/vere/commit/4942a7dd88b550fb5238227631410fa46563acb9).

**Live fake ships**

| Workload (µs) | Develop | Previous review | New review | Less time vs previous review |
|---|---:|---:|---:|---:|
| Read member map | 214.0 | 214.0 | 83.0 | 61.2% |
| Read invite map | 233.0 | 283.0 | 92.0 | 67.5% |
| Read published map | 301.0 | 295.0 | 97.0 | 67.1% |
| Notes members query body | 459.0 | 237.0 | 122.0 | 48.5% |
| Notes invites query body | 521.0 | 301.0 | 150.0 | 50.2% |
| Notes published query body | 689.0 | 450.0 | 274.0 | 39.1% |
| Read 65,536-entry map | 2,012.0 | 2,238.0 | 677.0 | 69.7% |
| Read 65,536-entry set | 2,091.0 | 1,690.0 | 595.0 | 64.8% |
| Read 128-entry member map | 4.0 | 4.0 | 4.0 | 0.0% |

**Input lifetime and downstream work**

| Workload (µs) | Previous review | New review | Less time |
|---|---:|---:|---:|
| 2,048 separately built seven-entry maps | 793.0 | 807.0 | -1.8% |
| 2,048 reads of a 32-entry map | 453.0 | 412.0 | 9.1% |
| Read and count the returned list | 271.0 | 139.0 | 48.7% |
| Read and jam the returned list | 2,091.0 | 1,992.0 | 4.7% |
| Read a freshly built 8,192-entry map | 202.0 | 189.0 | 6.4% |
| Build and read the map | 853.0 | 828.0 | 2.9% |

<a id="change-030e24f609"></a>

### Protected cue on the receiving road and reader specialization

**Multiple commits with a parser-only control.** For a protected cue of an 8,192-entry map, previous review took 572 µs, parser-only took 502 µs and the full receiving-road construction took 447 µs. The step from parser-only to full was 10.9%. A small valid protected message took 15 versus 16 µs.

- [030e24f609](https://github.com/yapishu/vere/commit/030e24f60944239fc4eb9528b43663a1839e146d) — noun: construct terminal protected cue results on the receiving road
- [6b1dea57d2](https://github.com/yapishu/vere/commit/6b1dea57d284c925e30d525f85d3ce938423de48) — noun: specialize the cue reader for each constructor and close cells inline

Stack parent before this group is [cc0ddf6b53](https://github.com/yapishu/vere/commit/cc0ddf6b5371203dc514b5ba63bd046fd913273a).

**Live Dojo**

| Workload | Develop µs | Previous review µs | Candidate µs | Less than review | Less than develop |
|---|---|---|---|---|---|
| Small valid message | 19 | 15 | 16 | -6.7% | 15.8% |
| Small invalid message | 17 | 15 | 15 | 0.0% | 11.8% |
| 8,192-entry map | 3,348 | 560 | 445 | 20.5% | 86.7% |
| 65,536-entry map | 44,431 | 4,715 | 3,703 | 21.5% | 91.7% |
| 65,536-item list | 15,289 | 1,436 | 1,106 | 23.0% | 92.8% |
| Hoon source syntax tree | 14,500 | 2,630 | 2,097 | 20.3% | 85.5% |
| Lull source syntax tree | 20,565 | 2,264 | 1,779 | 21.4% | 91.3% |

**Live Dojo**

| Workload | Develop µs | Previous review µs | Candidate µs | Less than review | Less than develop |
|---|---|---|---|---|---|
| Ordinary cue of map | 44,809 | 3,074 | 2,711 | 11.8% | 93.9% |
| Ordinary cue of list | 14,556 | 1,033 | 874 | 15.4% | 94.0% |
| Ordinary cue of Hoon syntax tree | 13,377 | 1,806 | 1,626 | 10.0% | 87.8% |
| Ordinary cue of Lull syntax tree | 19,611 | 1,544 | 1,366 | 11.5% | 93.0% |
| Return map entries | 3,092 | 1,520 | 1,594 | -4.9% | 48.4% |
| Return scalar list | 4,221 | 2,808 | 2,761 | 1.7% | 34.6% |
| Return record list | 3,154 | 2,128 | 2,125 | 0.1% | 32.6% |

**Live Dojo**

| King + worker after the last Dojo round | Develop | Previous review | Candidate |
|---|---|---|---|
| PSS, MiB | 236.0 | 210.2 | 211.2 |
| Anonymous PSS, MiB | 122.7 | 124.9 | 126.0 |

**Protected cue in microseconds per call**

| Protected workload | Previous review | Parser change alone | Parser and direct construction | Incremental saving from construction |
|---|---|---|---|---|
| 8,192-entry map | 572 | 502 | 447 | 10.9% |
| 65,536-entry map | 4,666 | 4,456 | 3,774 | 15.3% |
| 65,536-item list | 1,500 | 1,341 | 1,069 | 20.3% |
| Hoon source syntax tree | 2,756 | 2,582 | 2,210 | 14.4% |
| Lull source syntax tree | 2,388 | 2,229 | 2,010 | 9.8% |

<a id="change-d6eee3e8bb"></a>

### Scan plain JSON string spans and write the exact output atom

**Incremental.** Encoding the 27 documents separately fell from 5.590 to 4.215 ms, a 24.6% saving. The report includes small JSON primitives and ship memory alongside the corpus.

- [d6eee3e8bb](https://github.com/yapishu/vere/commit/d6eee3e8bb79a24ccfd270acd1395c4f2b8e9d87) — jets: scan plain JSON string spans and write the exact output atom

Stack parent before this change is [6b1dea57d2](https://github.com/yapishu/vere/commit/6b1dea57d284c925e30d525f85d3ce938423de48).

**Live gate timings**

| Workload | Develop ms | Previous review ms | New review ms | Less time vs review | Less time vs develop |
|---|---:|---:|---:|---:|---:|
| 27 documents, separate encodes | 7.835 | 5.590 | 4.215 | 24.6% | 46.2% |
| Tlon init | 3.292 | 1.964 | 1.425 | 27.4% | 56.7% |
| Groups init | 1.796 | 1.507 | 1.122 | 25.5% | 37.5% |
| Contacts directory | 0.550 | 0.446 | 0.378 | 15.4% | 31.4% |
| Notes OpenAPI | 0.215 | 0.159 | 0.141 | 11.4% | 34.8% |
| Channel posts | 0.171 | 0.111 | 0.100 | 9.9% | 41.3% |
| Activity init feeds | 0.076 | 0.045 | 0.044 | 3.3% | 42.1% |
| Pins | 0.008 | 0.008 | 0.007 | 12.5% | 12.5% |
| Decode and re-encode groups init | 4.337 | 2.622 | 2.522 | 3.8% | 41.8% |
| Two complete corpora in one array | 15.515 | 10.282 | 7.383 | 28.2% | 52.4% |
| Eight init documents in one array | 18.480 | 7.672 | 5.712 | 25.5% | 69.1% |
| Two init documents in one array | 5.043 | 2.405 | 1.851 | 23.0% | 63.3% |
| Sixteen groups init documents in one array | 18.593 | 10.828 | 8.387 | 22.5% | 54.9% |

**Live gate timings**

| Entry shape | Develop ms | Previous review ms | New review ms | Less time vs review | Less time vs develop |
|---|---:|---:|---:|---:|---:|
| boolean | 0.822 | 0.879 | 0.873 | 0.7% | -6.2% |
| empty-array | 0.740 | 0.761 | 0.770 | -1.2% | -4.1% |
| empty-object | 0.944 | 1.061 | 0.949 | 10.6% | -0.6% |
| nested | 2.456 | 2.505 | 2.377 | 5.1% | 3.2% |
| null | 0.983 | 0.992 | 0.912 | 8.0% | 7.2% |
| number | 0.883 | 0.973 | 0.887 | 8.8% | -0.5% |

**Memory and native controls**

| Measurement | Develop MiB | Previous review MiB | New review MiB |
|---|---:|---:|---:|
| before, Rss | 224.18 | 222.97 | 223.48 |
| before, Pss | 217.52 | 216.71 | 216.83 |
| before, Pss_Anon | 79.11 | 67.30 | 67.36 |
| after, Rss | 254.32 | 252.52 | 253.04 |
| after, Pss | 247.66 | 246.29 | 246.38 |
| after, Pss_Anon | 85.99 | 73.75 | 73.82 |

<a id="change-3421967e39"></a>

### Combine cell mugs directly from their two words

**Incremental.** Combining cell mugs directly cut hashing a fresh 100,000-item list by 29.9%. Hashing all 143 recorded events took 14.9% less time on the default loom.

- [3421967e39](https://github.com/yapishu/vere/commit/3421967e396930a4da3b8feae6cc14cb3d2e8b45) — noun: combine cell mugs directly from their two words

Stack parent before this change is [d6eee3e8bb](https://github.com/yapishu/vere/commit/d6eee3e8bb79a24ccfd270acd1395c4f2b8e9d87).

**Running ships**

| Workload | Develop ms | Previous review ms | New review ms | Less time vs previous review | Measured rounds |
|---|---:|---:|---:|---:|---:|
| Mug a fresh 100,000-item list | 2.118 | 2.063 | 1.446 | 29.9% | 14 |
| Mug 65,536 fresh three-field records | 5.365 | 3.987 | 2.889 | 27.5% | 14 |
| Mug the Hoon syntax tree | 2.304 | 2.209 | 1.839 | 16.7% | 14 |
| Mug the Lull syntax tree | 2.425 | 2.277 | 1.750 | 23.2% | 14 |
| Read cached records mug | 0.001 | 0.001 | 0.001 | 0.0% | 14 |
| Construct and mug 65,536 records | 15.319 | 12.459 | 11.430 | 8.3% | 14 |
| Call the memoized gate 40,000 times | 8.694 | 8.264 | 7.185 | 13.1% | 14 |
| Cue and mug 65,536 records | 62.524 | 6.514 | 5.322 | 18.3% | 14 |
| 20,000 successful map lookups, fresh pair keys | 10.969 | 7.771 | 7.699 | 0.9% | 12 |
| 20,000 unsuccessful map lookups, fresh pair keys | 10.950 | 7.864 | 7.840 | 0.3% | 12 |
| 20,000 successful map lookups, reused keys | 9.917 | 6.772 | 6.732 | 0.6% | 12 |
| Compile CSV library | 63.183 | 25.439 | 24.790 | 2.6% | 12 |

**Recorded events and memory**

| Loom and workload | Previous review ms | New review ms | Less time |
|---|---:|---:|---:|
| 32-bit, all 143 | 129.83 | 110.53 | 14.9% |
| 32-bit, without largest | 15.61 | 14.90 | 4.5% |
| 64-bit, all 143 | 165.73 | 138.47 | 16.4% |
| 64-bit, without largest | 17.76 | 16.58 | 6.7% |

**Recorded events and memory**

| Build | Before PSS MiB | Holding PSS MiB | Holding RSS MiB |
|---|---:|---:|---:|
| develop | 215.34 | 228.10 | 235.11 |
| review | 214.98 | 228.49 | 235.02 |
| candidate32-urbit | 214.97 | 228.49 | 234.91 |

<a id="change-4d116b0a7d"></a>

### Carry the mugged memo key from lookup to save

**Incremental.** Carrying the mugged memo key from lookup to save cut 20,000 distinct memoized additions from 5.099 to 4.139 ms. The full Hoon compile saved 1.0%.

- [4d116b0a7d](https://github.com/yapishu/vere/commit/4d116b0a7d62697eba2067bc4d38b44fa51748d0) — nock: carry the mugged memo key from lookup to save

Stack parent before this change is [3421967e39](https://github.com/yapishu/vere/commit/3421967e396930a4da3b8feae6cc14cb3d2e8b45).

**Running ships**

| Workload | Develop ms | Previous review ms | Candidate ms | Less time vs previous review |
|---|---:|---:|---:|---:|
| 20,000 distinct memoized additions | 6.096 | 5.099 | 4.139 | 18.8% |
| 20,000 pairs visited twice | 9.249 | 7.628 | 6.661 | 12.7% |
| One pair visited 40,000 times | 5.467 | 4.404 | 4.176 | 5.2% |
| 40,000 additions without a memo hint | 4.336 | 3.627 | 3.451 | 4.9% |
| Compile CSV library | 67.853 | 28.533 | 27.372 | 4.1% |

**Compiling and replaying**

| Workload | Develop ms | Previous review ms | Candidate ms | Less time vs previous review |
|---|---:|---:|---:|---:|
| Self-hosted Hoon compilation | 11509.325 | 4840.277 | 4791.655 | 1.0% |
| Lull compilation | 9328.094 | 4232.693 | 4211.748 | 0.5% |

**Mechanism and independent checks**

| Workload | Review ms | Carry key ms | Carry key and fill mugs ms |
|---|---:|---:|---:|
| 20,000 distinct memoized additions | 4.373 | 3.903 | 3.558 |
| 20,000 pairs visited twice | 6.714 | 6.207 | 5.672 |
| One pair visited 40,000 times | 4.127 | 3.939 | 3.689 |
| 40,000 additions without a memo hint | 3.119 | 3.095 | 3.020 |
| Compile CSV library | 23.460 | 23.593 | 23.375 |

<a id="change-16525ae06c"></a>

### Compose noun axes directly in the result words

**Incremental.** Direct axis composition cut the native random sample of 45,986 real replay calls by 16.0%. Building a fresh mixed path 12,000 steps long inside Dojo took 21.8% less.

- [16525ae06c](https://github.com/yapishu/vere/commit/16525ae06c4848a5d638c54694a3252f8a53aba4) — jets: compose noun axes directly in the result words

Stack parent before this change is [4d116b0a7d](https://github.com/yapishu/vere/commit/4d116b0a7d62697eba2067bc4d38b44fa51748d0).

**Recorded replay inputs**

| Recorded inputs | Sample size | Previous review ns | New review ns | Less time |
|---|---:|---:|---:|---:|
| Random sample of replay calls | 45,986 | 15.06 | 12.65 | 16.0% |
| Runs of 64 consecutive calls | 46,336 | 10.23 | 8.09 | 20.9% |
| Both inputs fit a 31-bit atom | 31,199 | 4.13 | 4.22 | -2.1% |
| Prefix over 64 bits plus one direction | 11,986 | 37.19 | 29.56 | 20.5% |
| Suffix over 64 bits (six calls) | 6 | 22.69 | 5.55 | 75.5% |

**Live Dojo**

| Workload | Develop ms | Previous review ms | New review ms | Less time |
|---|---:|---:|---:|---:|
| 12,000-step mixed path | 2.482 | 1.854 | 1.450 | 21.8% |
| 24,000 short joins | 2.504 | 1.911 | 1.875 | 1.8% |
| 24,000 wide prefixes plus one step | 3.199 | 2.813 | 2.495 | 11.3% |
| 24,000 one-step prefixes plus wide suffixes | 3.172 | 2.592 | 2.248 | 13.3% |
| Hoon compiler +tend | 1.740 | 1.258 | 1.087 | 13.7% |
| Ames +unroll on a spine | 1.744 | 1.560 | 1.486 | 4.7% |
| Ames +unroll on a balanced tree | 6.465 | 5.753 | 5.631 | 2.1% |
| Parse axis syntax with +ven | 4.694 | 2.429 | 2.346 | 3.4% |
| Compile a 384-field tuple test with +fish | 0.430 | 0.325 | 0.329 | -1.1% |
| Read and parse CSV library | 10.819 | 7.245 | 7.375 | -1.8% |
| Compile CSV library | 62.648 | 24.402 | 23.778 | 2.6% |
| Compile Hoon | 11182.963 | 4670.917 | 4655.440 | 0.3% |
| Compile Lull | 9086.584 | 3919.954 | 3966.437 | -1.2% |

<a id="change-de20d1d452"></a>

### Materialize field axes only when a name resolves

**Incremental.** Deferring field axes cut resolving all 384 names in the live test from 41.1695 to 0.6345 ms. Full Hoon compilation took 0.7% longer in this series.

- [de20d1d452](https://github.com/yapishu/vere/commit/de20d1d452fa57671c5725a8b22185f2f2034df7) — jets: materialize field axes only when a name resolves

Stack parent before this change is [16525ae06c](https://github.com/yapishu/vere/commit/16525ae06c4848a5d638c54694a3252f8a53aba4).

**Compile and execute Hoon source**

| Fields selected | Develop ms | Previous review ms | New review ms | Less time |
|---|---:|---:|---:|---:|
| 12 | 0.1320 | 0.0460 | 0.0440 | 4.3% |
| 96 | 4.0080 | 2.6460 | 0.2650 | 90.0% |
| 384 | 60.9470 | 41.7130 | 1.3960 | 96.7% |

**Named lookup on a live ship**

| Lookup workload | Develop ms | Previous review ms | New review ms | Less time |
|---|---:|---:|---:|---:|
| All 384 fields | 57.6995 | 41.1695 | 0.6345 | 98.5% |
| 384 absent names | 118.5295 | 83.8490 | 1.1500 | 98.6% |
| All 384 fields over a core | 58.2550 | 40.4690 | 0.6455 | 98.4% |
| 13 underlying chapter arms | 3.9700 | 2.7585 | 0.0325 | 98.8% |
| 32 distinct underlying door arms | 9.6435 | 6.5055 | 0.0660 | 99.0% |
| First field repeated 384 times | 0.4250 | 0.0825 | 0.0810 | 1.8% |

**Whole compilers and replay**

| Compile | Develop ms | Previous review ms | New review ms | Less time |
|---|---:|---:|---:|---:|
| Hoon | 11148.2475 | 4625.1545 | 4655.6700 | -0.7% |
| Lull | 8712.0305 | 4083.0960 | 3979.8545 | 2.5% |

<a id="change-3ba5984dea"></a>

### Move transient memo lookups past cheap decisions

**Incremental.** Moving transient memo lookups past cheap decisions cut 4,096 small distinct types through +bran from 6.088 to 4.389 ms. The source uses signed change in elapsed time, where negative means faster.

- [3ba5984dea](https://github.com/yapishu/vere/commit/3ba5984dea65c33f29384f2e034d23dd48a923dc) — nock: move transient memo lookups past cheap decisions

Stack parent before this change is [de20d1d452](https://github.com/yapishu/vere/commit/de20d1d452fa57671c5725a8b22185f2f2034df7).

**Live Dojo times in milliseconds**

| Case | Upstream develop | Prior review | Candidate | Time change vs prior review |
|---|---:|---:|---:|---:|
| 4,096 distinct small types through +bran:ut | 9.107 | 6.088 | 4.389 | -27.9% |
| 128 types with the Zuse type as their tail through +bran:ut | 0.261 | 0.181 | 0.156 | -13.8% |
| 4,096 cheap arms | 0.774 | 0.522 | 0.105 | -79.9% |
| 4,096 mixed cheap/expensive arms | 1.004 | 0.706 | 0.493 | -30.2% |
| 4,096 repeated cheap arms | 0.566 | 0.407 | 0.100 | -75.4% |
| 4,096 repeated expensive arms | 0.590 | 0.454 | 0.464 | +2.2% |
| 12,000 eight-cell constructions, distinct inputs | — | 2.192 | 0.623 | -71.6% |
| 12,000 eight-cell constructions, repeated input | — | 1.143 | 0.607 | -46.9% |
| 18-way switch across all tags | — | 1.668 | 1.680 | +0.7% |
| 18-way switch, repeated expensive last arm | — | 1.187 | 1.181 | -0.5% |
| 18-way switch, distinct expensive last-arm inputs | — | 2.602 | 2.616 | +0.5% |
| Ship-name formatting, distinct | 36.038 | 27.006 | 27.067 | +0.2% |
| Ship-name formatting, repeated | 13.500 | 10.441 | 10.544 | +1.0% |
| Date formatting, repeated | 17.609 | 13.821 | 13.982 | +1.2% |
| CSV parsing | 8.088 | 5.452 | 5.500 | +0.9% |
| Self-hosted Hoon compilation | 11046.208 | 4822.872 | 4711.314 | -2.3% |
| Prelude compilation | 8660.770 | 4049.722 | 4161.712 | +2.8% |

**Fake-ship replay**

| Bootstrap through event 143 | Trials | Wall s | User CPU s | Peak RSS MiB |
|---|---:|---:|---:|---:|
| develop | 1 | 329.56 | 326.46 | 1145.16 |
| review | 3 | 114.21 | 110.73 | 835.38 |
| candidate32-urbit | 3 | 116.22 | 112.71 | 835.61 |

<a id="change-cdff69c001"></a>

### Reclaim transient memo entries in groups

**Incremental.** Reclaiming transient memo entries in groups cut Hoon time by 8.6% and Prelude by 11.8%. Same-core median cold replay saved 7.1%. Two alternating pairs gave 2.9%. Peak replay RSS was essentially level.

- [cdff69c001](https://github.com/yapishu/vere/commit/cdff69c0015084d46b442f962ed77876092b98dc) — noun: reclaim transient memo entries in groups

Stack parent before this change is [3ba5984dea](https://github.com/yapishu/vere/commit/3ba5984dea65c33f29384f2e034d23dd48a923dc).

**Reclaiming transient memo entries on a fake ~zod**

| Workload | Upstream develop | Previous review | New review | Reduction vs previous review |
|---|---:|---:|---:|---:|
| All outer +bran calls in cold replay, thread CPU | — | 17.59 s | 12.46 s | 29.1% |
| 36 matched outer +bran calls, thread CPU | — | 15.61 s | 10.92 s | 30.1% |
| Hoon compilation | 10.98 s | 4.63 s | 4.23 s | 8.6% |
| Prelude compilation | 8.64 s | 4.05 s | 3.57 s | 11.8% |
| Cold replay through event 143, same-core median | 311.73 s | 114.27 s | 106.17 s | 7.1% |
| Cold replay through event 143, signed three-way | 311.73 s | 114.27 s | 102.71 s | 10.1% |
| Cold replay through event 143, two alternating pairs | — | 112.06 s | 108.85 s | 2.9% |
| Cold replay peak RSS, signed three-way | 1145.10 MiB | 835.48 MiB | 835.41 MiB | 0.0% |

<a id="change-e833fce569"></a>

### Use memo dependencies to set CLOCK insertion priority

**Incremental.** Changing CLOCK insertion priority cut Prelude by 21.2% and cold replay by 2.6%. Hoon took 1.9% longer and short checkpoint replay took 3.3% longer.

- [e833fce569](https://github.com/yapishu/vere/commit/e833fce56986cbf6d7f280ec8b984ec33f2d865b) — noun: use memo dependencies to set CLOCK insertion priority

Stack parent before this change is [cdff69c001](https://github.com/yapishu/vere/commit/cdff69c0015084d46b442f962ed77876092b98dc).

**Memo insertion priority on a Kelvin 135 fake ~zod**

| Workload | Develop | Previous review | Candidate | Less time vs review | Peak RSS on review → candidate |
|---|---:|---:|---:|---:|---:|
| Compile Prelude | 9.631 s | 3.838 s | 3.026 s | +21.2% | 226.7 → 228.3 MiB |
| Compile Hoon | 12.195 s | 4.614 s | 4.701 s | -1.9% | 216.4 → 216.8 MiB |
| Compile Zuse | 3.750 s | 1.620 s | 1.591 s | +1.8% | 225.5 → 227.4 MiB |
| Compile Arvo | 0.657 s | 0.322 s | 0.320 s | +0.6% | 211.9 → 211.8 MiB |
| Compile the CSV library | 0.057 s | 0.022 s | 0.022 s | -0.7% | 213.5 → 213.3 MiB |
| Cold replay through event 143 | 323.430 s | 112.785 s | 109.800 s | +2.6% | 835.4 → 835.2 MiB |
| Replay events 233–249 | 2.765 s | 1.800 s | 1.860 s | -3.3% | 241.8 → 241.7 MiB |

<a id="change-4f095fdad7"></a>

### Grow packed hash trie nodes in place

**Incremental.** Growing packed hash-trie nodes in place cut the 6,000-item regular-edit diff by 10.9%. Full Prelude compilation saved 0.9%.

- [4f095fdad7](https://github.com/yapishu/vere/commit/4f095fdad7171b3cdb5aff18bc7c081fe8109778) — noun: grow packed hash trie nodes in place

Stack parent before this change is [e833fce569](https://github.com/yapishu/vere/commit/e833fce56986cbf6d7f280ec8b984ec33f2d865b).

**Recorded ship workloads**

| Workload | Develop | Previous review | Candidate | Less time vs previous review | Peak RSS in MiB, review → candidate |
|---|---:|---:|---:|---:|---:|
| Compile Prelude | 10.287 s | 2.952 s | 2.926 s | +0.9% | 228.3 → 228.5 |
| Compile Hoon | 12.896 s | 4.531 s | 4.526 s | +0.1% | 216.8 → 216.9 |
| Compile Zuse | 4.102 s | 1.617 s | 1.600 s | +1.1% | 227.4 → 227.5 |
| Compile Arvo | 0.709 s | 0.320 s | 0.319 s | +0.3% | 211.9 → 212.1 |
| Compile the CSV library | 0.062 s | 0.021 s | 0.021 s | +0.3% | 213.3 → 213.6 |
| Replay events 233–249 | 2.090 s | 1.185 s | 1.190 s | -0.4% | 241.7 → 241.7 |
| Cold replay through 143 | 335.670 s | 108.235 s | 107.560 s | +0.6% | 835.2 → 835.5 |

**Longest common subsequence inside Dojo**

| Two input lists | Develop | Previous review | Candidate | Less time vs previous review |
|---|---:|---:|---:|---:|
| 6,000 distinct IDs, every 37th changed | 7.755 ms | 0.699 ms | 0.623 ms | +10.9% |
| 6,000 distinct IDs, every 37th changed (second boot) | 7.899 ms | 0.709 ms | 0.616 ms | +13.1% |
| 30,000 distinct IDs, every 37th changed | 51.264 ms | 4.104 ms | 3.219 ms | +21.6% |
| 30,000 distinct IDs, every 37th changed (second boot) | 51.144 ms | 4.048 ms | 3.277 ms | +19.0% |
| 6,000 IDs repeating over 17 and 23 values | 28.453 ms | 4.266 ms | 4.213 ms | +1.2% |
| One line deleted from the ship’s Clay source | 11.188 ms | 2.047 ms | 1.942 ms | +5.1% |

**Cache operations and Nock**

| Workload | Previous review | Candidate | Less time |
|---|---:|---:|---:|
| Fill a 512-entry table | 0.029 ms | 0.027 ms | +6.1% |
| Insert/evict 65,536 keys in a 512-entry table | 6.596 ms | 5.893 ms | +10.7% |
| Fill a 16,384-entry table | 1.292 ms | 0.970 ms | +24.9% |
| Insert/evict 131,072 keys in a 16,384-entry table | 19.260 ms | 15.906 ms | +17.4% |
| Fill a 200,000-entry table | 20.859 ms | 15.460 ms | +25.9% |
| Insert/evict 400,000 keys in a 200,000-entry table | 100.862 ms | 84.419 ms | +16.3% |
| Eval a Nock memo with 1 increment | 47.090 ms | 48.634 ms | -3.3% |
| Eval a Nock memo with 512 increments | 96.512 ms | 88.579 ms | +8.2% |
| LCS of two revisions of /sys/arvo | 0.494 ms | 0.471 ms | +4.6% |
| LCS of two revisions of /sys/lull | 1.785 ms | 1.733 ms | +2.9% |
| LCS of 6,000 mostly equal unique IDs | 0.799 ms | 0.695 ms | +13.0% |
| LCS of overlapping, repeating lists | 0.476 ms | 0.485 ms | -1.9% |
| LCS of disjoint, repeating lists | 0.295 ms | 0.287 ms | +2.5% |

<a id="change-d5f8811878"></a>

### Find canonical loss along bounded edit frontiers

**Incremental.** A bounded edit frontier cut the native corpus of 30 core source revisions by 89.5%. The live one-line Clay subsequence fell from 1.982 to 0.071 ms. A 6,000-ID test with regularly spaced changes took 0.6% longer.

- [d5f8811878](https://github.com/yapishu/vere/commit/d5f88118788c2856a452fe81ed2c5bf636550a29) — jets: find canonical loss along bounded edit frontiers

Stack parent before this change is [4f095fdad7](https://github.com/yapishu/vere/commit/4f095fdad7171b3cdb5aff18bc7c081fe8109778).

**Inside Dojo**

| Workload | Develop, ms | Previous review, ms | Candidate, ms | Less time vs review |
|---|---:|---:|---:|---:|
| 6,000 IDs, every 37th replaced | 7.737 | 0.612 | 0.616 | -0.6% |
| 30,000 IDs, every 37th replaced | 51.459 | 3.216 | 3.216 | -0.0% |
| Clay source, delete one line, LCS | 11.028 | 1.965 | 0.066 | +96.7% |
| Clay source, move one block, LCS | 11.027 | 1.970 | 0.079 | +96.0% |
| Complete %txt patch, delete one line | 13.883 | 3.811 | 1.867 | +51.0% |
| Complete %txt patch, move one block | 14.059 | 3.779 | 1.891 | +50.0% |

**Git source revisions**

| Workload | Develop, ms, sum | Previous review, ms, sum | Candidate, ms, sum | Less time vs review |
|---|---:|---:|---:|---:|
| 30 revisions of Arvo, Clay, Hoon, Lull and Zuse | 637.923 | 117.924 | 12.387 | +89.5% |
| 30 revisions of six other vanes | 402.219 | 77.228 | 4.141 | +94.6% |
| 30 held-out application and library revisions | 38.280 | 5.038 | 2.389 | +52.6% |

**Memory under a single replacement in two separately allocated lists of distinct atoms**

| IDs | Develop peak MiB | Previous review peak MiB | Candidate peak MiB |
|---:|---:|---:|---:|
| 6,000 | 2.85 | 2.59 | 2.08 |
| 30,000 | 7.46 | 5.02 | 3.00 |
| 160,000 | 32.41 | 18.05 | 8.00 |

**One-line patch stages measured in their own Dojo calls**

| Workload | Develop, ms | Previous review, ms | Candidate, ms | Less time vs review |
|---|---:|---:|---:|---:|
| lcs-only | 11.099 | 1.982 | 0.071 | +96.4% |
| construct-with-known-lcs | 2.716 | 1.913 | 1.857 | +3.0% |
| apply-precomputed-patch | 0.220 | 0.192 | 0.202 | -4.9% |
| end-to-end-construction | 14.017 | 3.885 | 1.895 | +51.2% |

<a id="change-c45cfd8d75"></a>

### List validation and position indexes for +loss

**Multiple commits.** Counting both input spines and indexing occurrence chains by position together reduced live +loss on 30 adjacent app revisions from 1.744 to 1.382 ms. The complete Clay text diff took 8.861 versus 8.435 ms. On 160,000 mostly distinct items peak process RSS fell from 17.88 to 16.59 MiB.

- [c45cfd8d75](https://github.com/yapishu/vere/commit/c45cfd8d752db1a63486c055eb7068f19452e218) — jets: count both loss inputs while validating their spines
- [136deb1539](https://github.com/yapishu/vere/commit/136deb15399dc0a52024ab31b00cb98ae9968362) — jets: index loss occurrence chains by integer position

Stack parent before this group is [d5f8811878](https://github.com/yapishu/vere/commit/d5f88118788c2856a452fe81ed2c5bf636550a29).

**Inside a live ship**

| Workload | Develop, ms | Previous review, ms | Signed candidate, ms | Less vs review |
|---|---:|---:|---:|---:|
| 30 adjacent application revisions, +loss | 23.369 | 1.744 | 1.382 | +20.8% |
| Same 30 revisions, complete Clay %txt diff | 33.404 | 8.861 | 8.435 | +4.8% |
| 19 wider application/library revision pairs, +loss | 16.501 | 1.700 | 1.381 | +18.8% |
| Same 19 pairs, complete Clay %txt diff | 24.720 | 7.356 | 6.965 | +5.3% |
| 6,000 distinct IDs with regular replacements | 7.932 | 0.633 | 0.401 | +36.7% |
| 30,000 distinct IDs with regular replacements | 55.181 | 3.312 | 2.180 | +34.2% |
| Clay source with one line deleted, +loss | 11.552 | 0.072 | 0.044 | +38.9% |
| Clay source with adjacent blocks swapped, +loss | 11.788 | 0.081 | 0.060 | +25.9% |
| Clay one-line edit, complete text-patch expression | 14.354 | 1.940 | 1.905 | +1.8% |
| Clay block move, complete text-patch expression | 14.432 | 1.970 | 1.925 | +2.3% |
| Two disjoint 6,000-item lists | 7.987 | 0.496 | 0.296 | +40.4% |
| 6,000 items drawn from two small alphabets | 174.457 | 5.103 | 4.994 | +2.1% |
| 1,000 separate four-item diffs | 0.852 | 0.431 | 0.312 | +27.6% |

**Native source-file corpus**

| Workload | Develop, ms | Previous review, ms | Signed candidate, ms | Less vs review |
|---|---:|---:|---:|---:|
| 30 core source revisions | 1,049.697 | 20.279 | 18.272 | +9.9% |
| 30 vane revisions | 630.544 | 5.831 | 5.753 | +1.3% |
| 30 application/library revisions | 60.196 | 3.853 | 3.295 | +14.5% |
| 19 wider application/library pairs | 45.184 | 3.917 | 3.279 | +16.3% |
| 19 additional application/library revisions | 9.733 | 0.493 | 0.382 | +22.6% |

**Memory**

| Workload | Develop, MiB | Previous review, MiB | Signed candidate, MiB | Less vs review |
|---|---:|---:|---:|---:|
| Disjoint, eight distinct values per side, 160000 | 9.61 | 8.13 | 8.08 | +0.6% |
| Disjoint, eight distinct values per side, 30000 | 3.10 | 3.13 | 3.08 | +1.6% |
| Disjoint, eight distinct values per side, 6000 | 1.90 | 2.20 | 2.16 | +2.1% |
| Mostly distinct, 160000 | 31.89 | 17.88 | 16.59 | +7.2% |
| Mostly distinct, 30000 | 7.28 | 4.99 | 4.82 | +3.3% |
| Mostly distinct, 6000 | 2.73 | 2.59 | 2.45 | +5.4% |

**Memory**

| Workload | Develop, MiB PSS | Previous review, MiB PSS | Signed candidate, MiB PSS | Less vs review |
|---|---:|---:|---:|---:|
| Resident ships after the 49 patch pairs, first boot | 287.48 | 286.52 | 287.27 | -0.3% |
| Resident ships after the 49 patch pairs, second boot | 285.20 | 284.79 | 285.26 | -0.2% |
| Resident ships after the shape tests, first boot | 191.95 | 177.75 | 178.00 | -0.1% |
| Resident ships after the shape tests, second boot | 191.64 | 178.21 | 177.98 | +0.1% |

**Compiler and replay**

| Workload | Develop, s | Previous review, s | Signed candidate, s | Less vs review |
|---|---:|---:|---:|---:|
| Compile Prelude | 9.728 | 2.909 | 3.016 | -3.7% |
| Compile Hoon | 12.720 | 4.946 | 4.717 | +4.6% |
| Compile Zuse | 4.045 | 1.538 | 1.563 | -1.6% |
| Checkpoint replay through event 249 | 1.810 | 1.270 | 1.270 | +0.0% |
| Cold replay through event 143 | 331.210 | 112.890 | 114.060 | -1.0% |

<a id="change-56b5327a55"></a>

### Reuse private cores across prepared native gate calls

A list jet such as `+sort`, `+roll` or `+turn` can prepare its gate once. When that gate calls a native jet, Vere reuses the two cells holding the battery, sample and context while it owns both cells exclusively. A jet that retains either cell keeps its ordinary noun, and the next call gets a fresh core. Each call evaluates the jet with the current sample.

**Live Kelvin 135 Dojo.** Each column gives the medians from two independent fake-`~zod` boots, in order. Each boot has one warm-up and 13 timed rounds per executable. All three builds take turns on the same core. The timer covers the list operation. After each evaluation the driver checks the SHA-256 of the ordered jammed result.

| Workload | Develop `baf98ea9c3`, ms | Previous review `136deb1539`, ms | Review `56b5327a55`, ms | Less time vs previous review |
|---|---:|---:|---:|---:|
| Sort 8,192 atoms with `+lth` | 2.470 / 2.434 | 2.534 / 2.440 | 1.935 / 1.892 | 23.6% / 22.5% |
| Sum the Lull source byte tape with `+roll` and `+add` | 3.316 / 3.049 | 3.473 / 3.224 | 2.507 / 2.330 | 27.8% / 27.7% |
| Hash each Lull source line with `+mug` | 0.240 / 0.194 | 0.211 / 0.195 | 0.170 / 0.156 | 19.4% / 20.0% |
| Join 45,986 sampled axis pairs with `+peg` | 2.300 / 2.117 | 2.204 / 2.131 | 1.939 / 2.110 | 12.0% / 1.0% |
| Join 46,336 axis pairs from consecutive runs with `+peg` | 1.787 / 1.895 | 2.055 / 1.735 | 1.495 / 1.505 | 27.3% / 13.3% |
| Compare the 45,986 pairs with `+gth` | 1.553 / 1.247 | 1.848 / 1.388 | 1.198 / 1.193 | 35.2% / 14.0% |
| Add the 45,986 pairs with `+add` | 2.093 / 1.843 | 2.336 / 2.094 | 1.842 / 2.020 | 21.1% / 3.5% |
| Read warm mugs for the 45,986 pairs | 1.537 / 1.389 | 1.389 / 1.281 | 1.217 / 1.030 | 12.4% / 19.6% |
| Join sampled pairs through a Hoon wrapper around `+peg` | 6.214 / 5.834 | 5.588 / 4.970 | 4.958 / 4.706 | 11.3% / 5.3% |
| Join consecutive pairs through the same wrapper | 5.868 / 5.517 | 4.514 / 4.532 | 4.484 / 4.372 | 0.7% / 3.5% |

The sort input is `(turn (gulf 1 8.192) mug)`. The fold and line workloads use the running ship’s Lull source. The axis pairs come from the event-143 cold replay, sampled both uniformly and in consecutive runs. All 92,322 axis joins match an independent integer calculation. The last two rows use an interpreted wrapper and give a control for variation between boots.

**Memory.** At the end of the two runs, combined king and worker PSS measured 285.40 / 287.17 MiB on develop, 242.07 / 242.51 MiB on the preceding review, and 244.79 / 244.53 MiB on this commit.

**Replay.** Four cold-replay pairs through event 143, with the two builds exchanging cores, took a median 112.35 s on the preceding review and 112.62 s with core reuse. Peak RSS measured 835.35 and 835.34 MiB respectively. The comparison used a pre-commit build of the same runtime change. A separate instrumented run counted 74,630 native calls among 33,874,137 prepared calls during cold replay (0.22%), and 79 among 1,104,362 during Hoon compilation.

On a checkpoint replay of events 144–153, nine timed pairs against develop took 1.913 s versus 1.311 s for the whole command (31.5% less), with peak RSS of 247.04 versus 221.25 MiB. A separate nine-pair comparison against the preceding review took 1.240 s versus 1.305 s (5.2% longer), with peak RSS of 221.37 versus 221.25 MiB. Each binary had a warm-up on a disposable copy of the same fake ship. These wall times exclude the first-open version roll.

**Compilers.** Each build replays the same recorded Dojo evaluation from a copy of the same checkpoint, rotating build order on one core. The table gives the median evaluation time after a warm-up and three timed runs per build.

| Workload | Develop, s | Previous review, s | Review, s | Change vs previous review |
|---|---:|---:|---:|---:|
| Compile Hoon | 11.419 | 4.222 | 4.405 | 4.3% longer |
| Compile Lull | 8.967 | 2.705 | 2.710 | 0.2% longer |
| Compile Zuse | 3.612 | 1.480 | 1.465 | 1.1% less |

**Correctness.** Eight native test executables pass on both loom widths, including tests for a jet that retains the core, payload or sample and a jet that punts to Nock. Develop also opens a checkpoint saved by the signed build at event 294 and replays event 295. On each loom width the replay audit matches develop’s canonical byte lengths and SHA-256 hashes for the bootstrap core, 138 cold-replay products and 17 checkpoint products (33.28 GB per width). The audited noun library and the signed build’s noun library differ only in debug metadata and assertion source line numbers.

<a id="change-52bac63838"></a>

### Delete keys from persistent maps and sets

When the key is absent, `(~(del by m) key)` and `(~(del in s) key)` return the original root. On a hit, the jet joins the two child treaps by priority and copies the ancestors whose child changes. A single stack holds both search and join paths. The measurements compare this commit with develop `baf98ea9c3` and the preceding review `be2f5d242a` (runtime `56b5327a55`).

**Calls from cold replay.** A Kelvin 135 fake `~zod` replay through event 143 makes 2,011 nonempty map deletions. The key is absent 1,014 times. Both set deletions find their key. The sample below comprises 50 input trees, keys and expected results captured by call position. Each live Dojo batch evaluates the indicated group 64 times. The timer covers the Hoon operation, including its list traversal. Each timed result matches develop by SHA-256 of the complete jammed noun. Each pair of figures gives the medians for two independent boots, with one warm-up and 11 timed trials per build on each boot.

| Calls per live batch | Develop, µs | Previous review, µs | Review, µs | Less time vs previous review |
|---|---:|---:|---:|---:|
| 26 absent map keys × 64 | 623 / 618 | 587 / 575 | 328 / 324 | 44.1% / 43.7% |
| 22 present map keys × 64 | 523 / 508 | 489 / 483 | 346 / 350 | 29.2% / 27.5% |
| 2 present set members × 64 | 48 / 48 | 37 / 37 | 35 / 35 | 5.4% / 5.4% |

An isolated native run measures the same 26 misses at 495 → 47 ns per call on the 32-bit loom and 440 → 46 ns on the 64-bit loom. The 24 hits take 414 → 366 ns and 373 → 310 ns respectively. Each median covers 12 timed trials after warm-up, alternating the two implementations in the same binary.

**Removal of versioned files.** Starting at Arvo `5a187feded` and Vere `baf98ea9c3`, take the 24 most recent first-parent revisions in each repository with deletions under `pkg/arvo/` or `pkg/`, respectively. Count renames as deletion and addition. Build a set of file paths and a map from path to Git object ID using the parent tree. Apply each revision’s deletions in Git order, then repeat them against the remaining collection. Both implementations produce the exact canonical jam of a collection rebuilt independently from the surviving paths.

The live runs use the largest removal in each corpus. Arvo `3ce1c00105` removes 294 of 688 paths. Vere `b339b5067a` removes 160 of 452. The three builds rotate on one core. Each column shows the medians from two independent boots, each with one warm-up and 11 timed trials. Every timed output has the same digest across builds and boots.

| Live Dojo batch | Develop, µs | Previous review, µs | Review, µs | Less time vs previous review |
|---|---:|---:|---:|---:|
| Arvo, map, present | 218 / 223 | 203 / 192 | 167 / 189 | 17.7% / 1.6% |
| Arvo, set, present | 210 / 216 | 180 / 185 | 161 / 155 | 10.6% / 16.2% |
| Arvo, map, already absent | 203 / 203 | 168 / 167 | 99 / 97 | 41.1% / 41.9% |
| Arvo, set, already absent | 189 / 184 | 168 / 166 | 94 / 89 | 44.0% / 46.4% |
| Vere, map, present | 128 / 122 | 116 / 118 | 110 / 101 | 5.2% / 14.4% |
| Vere, set, present | 124 / 117 | 111 / 104 | 99 / 101 | 10.8% / 2.9% |
| Vere, map, already absent | 106 / 110 | 105 / 112 | 72 / 63 | 31.4% / 43.8% |
| Vere, set, already absent | 106 / 102 | 102 / 113 | 59 / 60 | 42.2% / 46.9% |

Each native sweep covers all 24 revisions in one repository, totaling 478 file deletions for Arvo and 328 for Vere. The old and new deletion functions run in the same binary. Each trial averages 45 sweeps, with alternating execution order, one warm-up and nine measured trials.

| Native sweep | 32-bit loom, previous → review, µs | Less time | 64-bit loom, previous → review, µs | Less time |
|---|---:|---:|---:|---:|
| Arvo, map, present | 303.4 → 266.6 | 12.1% | 309.2 → 269.5 | 12.8% |
| Arvo, set, present | 310.6 → 261.1 | 15.9% | 312.0 → 263.2 | 15.6% |
| Arvo, map, already absent | 283.4 → 28.8 | 89.9% | 283.5 → 28.8 | 89.9% |
| Arvo, set, already absent | 291.1 → 26.1 | 91.0% | 290.5 → 26.4 | 90.9% |
| Vere, map, present | 199.8 → 177.4 | 11.2% | 181.3 → 160.0 | 11.8% |
| Vere, set, present | 207.3 → 170.3 | 17.8% | 187.2 → 155.3 | 17.1% |
| Vere, map, already absent | 180.3 → 17.5 | 90.3% | 166.9 → 16.5 | 90.1% |
| Vere, set, already absent | 187.5 → 16.3 | 91.3% | 171.1 → 15.0 | 91.2% |

**Memory.** All 806 repeated deletions return their input root for both maps and sets. For each collection type, the sum of newly reachable cells across those calls falls from 17,650 to zero. Successful removals produce 16,528 newly reachable cells on either implementation. These are per-call counts of cells reachable only from the output. Combined king and worker PSS after the two file-history runs is 277.88 / 276.84 MiB on develop, 216.88 / 216.35 MiB on the preceding review, 216.51 / 217.14 MiB on this commit.

**Whole-ship controls.** Four pairs of cold replays exchange physical cores on successive runs. Median wall time is 109.065 s on the preceding review and 109.895 s on this commit (0.8% longer). The range across the four trials is 105.13–119.04 s and 106.26–113.61 s respectively. Peak RSS is 835.49 and 835.28 MiB. Other workloads run concurrently on separate cores.

Nine pairs of checkpoint replays over events 144–153 measure 1.308 → 1.399 s wall time against the preceding review (6.9% longer) and 1.812 → 1.312 s against develop (27.6% less) in a separate series. Each binary has a warm-up on a disposable copy before timing. The three compiler controls below and checkpoint events 233–249 make zero calls to either deletion jet. Compiler times are medians of three measured evaluations per build after warm-up.

| Compiler | Develop, s | Previous review, s | Review, s |
|---|---:|---:|---:|
| Hoon | 12.425 | 4.909 | 4.874 |
| Lull | 9.329 | 3.007 | 3.032 |
| Zuse | 3.763 | 1.540 | 1.563 |

**Correctness.** Eight native test suites pass on each loom width. Differential tests against the original implementation match all 26,553 successful products and 13,055 exits per width, including malformed trees and structurally unequal keys with the same mug. On each width the bootstrap core, 138 cold-replay products and 17 checkpoint products match develop in canonical length and SHA-256 (33,279,069,693 bytes per width). Develop opens a checkpoint saved by this build at event 294 and replays event 295.

<a id="change-0aebc87353"></a>

### Pass treap partitions directly through difference

For `(~(dif by a) b)` and `(~(dif in a) b)`, a matching root key exposes the two child trees directly. A split below the root hands its two results to the recursive calls separately. A sole surviving branch becomes the result. The public `+bif` gate uses the same search and returns its ordinary pair. The comparison uses develop `baf98ea9c3` and the preceding review `76be11f6aa` (runtime `52bac63838`).

**Complete file revisions in a live Dojo.** Use the parent and child Git trees for Arvo `3ce1c00105` under `pkg/arvo/` and Vere `b339b5067a` under `pkg/`. A set holds file paths as lists of components. A map associates each path with its Git object ID. Each batch takes the indicated difference 32 times. The driver independently checks the complete result against Git, including the values on the surviving keys. SHA-256 over each ordered batch of jammed results agrees across all three builds. The table gives medians in µs from two independent Kelvin 135 fake-`~zod` boots. Each boot has one warm-up and 11 timed rounds per build in rotating order on the same core.

| 32 differences | Develop, µs | Previous review, µs | Review, µs | Less time vs previous review |
|---|---:|---:|---:|---:|
| Arvo, map, removals | 1301 / 1352 | 1170 / 1265 | 961 / 1016 | 17.9% / 19.7% |
| Arvo, map, additions | 1198 / 1256 | 858 / 873 | 680 / 679 | 20.7% / 22.2% |
| Arvo, set, removals | 1245 / 1213 | 1192 / 1187 | 974 / 969 | 18.3% / 18.4% |
| Arvo, set, additions | 1057 / 1102 | 890 / 891 | 608 / 623 | 31.7% / 30.1% |
| Vere, map, removals | 845 / 854 | 747 / 749 | 636 / 638 | 14.9% / 14.8% |
| Vere, map, additions | 806 / 798 | 603 / 598 | 453 / 449 | 24.9% / 24.9% |
| Vere, set, removals | 770 / 786 | 794 / 751 | 618 / 625 | 22.2% / 16.8% |
| Vere, set, additions | 714 / 744 | 583 / 603 | 434 / 452 | 25.6% / 25.0% |

**Native file-history sweeps.** Take the 24 most recent first-parent commits with deletions under each source prefix, starting at Arvo `5a187feded` and Vere `baf98ea9c3`, with rename detection off. Construct the complete parent and child collections independently. Both directions match the canonical jam of the paths computed by literal filename comparison. Times are µs per sweep of 24 revision pairs. Both loom widths take turns on the same core after replay and compilation finish. Each median covers nine timed trials after warm-up, averaging 18 sweeps per trial and alternating the old and new functions.

| Native sweep | 32-bit previous → review, µs | Less time | 64-bit previous → review, µs | Less time |
|---|---:|---:|---:|---:|
| Arvo, map, removals | 496.9 → 436.6 | 12.1% | 559.4 → 493.4 | 11.8% |
| Arvo, map, additions | 490.8 → 422.1 | 14.0% | 547.3 → 477.3 | 12.8% |
| Arvo, set, removals | 484.1 → 418.1 | 13.6% | 523.6 → 468.0 | 10.6% |
| Arvo, set, additions | 478.8 → 404.8 | 15.5% | 518.1 → 454.3 | 12.3% |
| Vere, map, removals | 334.7 → 287.5 | 14.1% | 369.9 → 319.6 | 13.6% |
| Vere, map, additions | 328.3 → 278.4 | 15.2% | 360.6 → 312.1 | 13.4% |
| Vere, set, removals | 330.4 → 277.2 | 16.1% | 352.0 → 306.7 | 12.9% |
| Vere, set, additions | 323.5 → 271.5 | 16.1% | 343.1 → 295.4 | 13.9% |

Across both directions and all 48 revisions, the preceding difference implementation allocates 3,263 intermediate partition-pair cells for maps and the same number for sets. This commit allocates zero such cells. Standalone native `+bif` trials split the same parent trees at 12 evenly spaced present keys, 12 absent keys and the root key. The reductions across projects, key groups and collection types span 9.2–29.6% on the two loom widths.

**Clay.** A new desk receives 320 numbered text files and a probe file. The bulk commit edits 240 numbered files and the probe, removes 32 files and adds 96. The third commit changes just the probe. Each commit is checked by reading its probe back through Clay. The 53-event window makes 13 calls with a nonempty right operand. A timer around the registered difference gates measures the original arguments during replay, once per call. The median sum over all 13 calls is 114.7 µs on the preceding review and 96.8 µs on this commit (15.6% less, one warm-up and 14 timed replays per build in alternating order). In a separate nine-replay comparison, median wall time for all 53 events is 1.90 s on develop, 1.01 s on the preceding review and 0.97 s on this commit. The time saved inside the measured gates is about 18 µs per replay.

**Memory and cold replay.** Combined king and worker PSS after the two file-history Dojo runs is 278.98 / 278.81 MiB on develop, 222.01 / 221.47 MiB on the preceding review, 221.63 / 222.15 MiB on this commit. Peak process RSS in the 53-event desk replay is 305.18 MiB, 317.17 MiB, 317.04 MiB respectively. Four cold replays through event 143 exchange cores between the preceding review and candidate. Median wall time is 114.500 → 114.465 s and peak RSS is 835.35 → 835.35 MiB. This cold replay has ten calls with a nonempty right operand. Other benchmarks run concurrently on separate cores.

**Compiler controls.** Each compiler replay makes 12 map-difference calls with an empty right operand and zero set-difference calls. The table gives the median Dojo evaluation time after one warm-up and three timed trials per build, replaying the same recorded expression on each build.

| Compiler | Develop, s | Previous review, s | Review, s |
|---|---:|---:|---:|
| Hoon | 12.731 | 4.818 | 4.940 |
| Lull | 9.692 | 3.125 | 3.159 |
| Zuse | 3.930 | 1.747 | 1.699 |

**Correctness.** Eight native suites pass on each loom width. A differential test for split and difference matches 49,792 successful products and 34,010 exits per width, including malformed trees, hash collisions and unbalanced search paths. On both widths, the bootstrap core, 138 cold-replay products and 17 checkpoint products match develop in canonical length and SHA-256 (33,279,069,693 bytes per width). The signed 32-bit build also matches upstream on every product from the 53 desk events (12,708,297,167 bytes). Develop opens the checkpoint this build writes at event 300 and replays events 301–302.

<a id="change-78c2194073"></a>

### Prepare one-hole constructors for repeated gate calls

Vere recognizes a gate that selects one field of its sample and builds a noun around it. Gate preparation records the field axis and the fixed siblings on the path to that field. Each call fills the hole with its own input. The path can contain up to 16 cells. At each level the fixed sibling can sit on either side and can be a literal or a field of the gate’s battery or lexical environment. A missing field takes the ordinary Nock path and reports the original source locations. The comparison uses develop `baf98ea9c3` and the preceding review `1610b3c303` (runtime `0aebc87353`).

**Clay gates in a running ship.** Take the 24 most recent first-parent Arvo commits with deletions under `pkg/arvo/` starting at `5a187feded`, with rename detection off. In chronological order, build one map from each resulting Git tree, keyed by path components with the hexadecimal blob ID as its value. The 24 maps contain 12,251 entries in total. Apply `+run:by` to each with the gate from `+tako-ford` or the changed-file gate at Clay line 1958. The canonical jam of each benchmark battery equals the bytes captured from that gate during desk replay. The control applies the same value constructor without source annotations. Each number is the median of 18 timed evaluations after one warm-up in a fresh Kelvin 135 fake `~zod`. The three builds rotate on one core. The ordered batch products agree by SHA-256.

| Workload on 24 revisions | Develop, µs | Previous review, µs | Review, µs | Time vs previous review |
|---|---:|---:|---:|---:|
| Clay `+tako-ford` | 1,061 / 953 | 1,062.5 / 1,032 | 743 / 685 | 30.1% less / 33.6% less |
| Clay changed-file gate | 1,003 / 963 | 1,068 / 1,099.5 | 709 / 697 | 33.6% less / 36.6% less |
| Unannotated control | 814 / 752 | 738 / 719.5 | 712 / 659 | 3.5% less / 8.4% less |

During two additional boots with concurrent compiler builds, the unannotated control ranges from 12.4% longer to 5.6% less time on review.

Combined king and worker PSS after these workloads is 272.36 / 272.39 MiB on develop, 213.59 / 213.59 MiB on previous review, 212.26 / 212.24 MiB on review.

**Captured values.** Three `+turn` workloads apply a gate twice to the same 12,251 entries with a different captured value on the second pass. They construct `[owner path]`, `[hash new]` and `[[%sand hash] owner]` respectively. These exercise left, right and nested captures. Each median covers 18 evaluations after one warm-up. All three builds return the same ordered products.

| Two passes over all entries | Develop, µs | Previous review, µs | Review, µs | Time vs previous review |
|---|---:|---:|---:|---:|
| Fixed left sibling | 1,017.5 | 983 | 442 | 55.0% less |
| Fixed right sibling | 1,033 | 960.5 | 543 | 43.5% less |
| Nested constructor | 1,156 | 1,074.5 | 610 | 43.2% less |

**Three desk changes in replay.** The 53 events cover creation of a desk, a bulk edit with additions and deletions, and a one-file edit. Identical timers around the public map jets record 140 nonempty `+run:by` and seven nonempty `+urn:by` calls per replay. Fourteen calls use the first Clay battery and six use the second. Each table entry is the median sum of durations per replay. The series contain 15 and 17 replays per build.

| Time inside the public map calls | Previous review, ms | Review, ms |
|---|---:|---:|
| All `+run:by` | 1.832 / 1.762 | 1.413 / 1.300 |
| All `+urn:by` | 0.561 / 0.536 | 0.565 / 0.561 |
| Combined | 2.390 / 2.305 | 1.989 / 1.858 |

Across 12 replays per uninstrumented binary, the median event interval is 966.188 / 707.937 / 695.025 ms on develop, previous review and review, respectively. Whole-command wall time is 2.064 / 1.046 / 1.013 s. Each build opens its own prepared copy of the same checkpoint before timing.

**Cold replay and compiler controls.** For cold replay through event 143, four paired trials exchange cores between the two builds. Median wall time is 107.22 → 108.71 s and peak RSS is 835.50 → 835.19 MiB. The three-build compiler comparison rotates the order over three trials. A second series alternates the two review builds over six trials. Each compiler time is the recorded Dojo evaluation in seconds.

| Compiler | Develop | Previous review | Review | Previous review / review, second series |
|---|---:|---:|---:|---:|
| Hoon | 11.408 | 4.961 | 4.652 | 4.556 / 4.652 |
| Lull | 9.746 | 3.245 | 3.024 | 2.958 / 2.994 |
| Zuse | 3.668 | 1.709 | 1.643 | 1.588 / 1.585 |

Separate invocation counts give 54,596 direct constructions for 53,915 prepared constructors in the Hoon compiler replay, 19,095 for 17,477 in Lull and 14,190 for 13,879 in Zuse. The desk replay records 14,965 direct constructions for 3,310 prepared constructors.

**Correctness.** Eight native suites pass on each loom width. In 8,500 differential cases, all 4,735 successful products and all 3,765 exits, including their source traces, match the full Nock evaluator. Develop and both review loom widths produce byte-identical traces. The bootstrap core, 138 cold-replay products and 17 checkpoint products match develop in canonical length and SHA-256 on both widths (33,279,069,693 bytes per width). All 53 additional desk-event products match develop on the 32-bit loom (12,708,342,437 bytes). Develop opens the checkpoint written by review at event 300 and replays events 301–302.

<a id="change-0c171e9479"></a>

### Prepare short-lived gates without building their fallback

Vere can execute a prepared gate by selecting a field, running a program on the sample, or filling a noun constructor. For an unregistered gate with one of these entry points, it can defer looking up the full Nock program until a call needs it. A constructor with one fixed sibling stores that noun and the field axis directly in the existing call site. Map and set traversals retain the nouns they need across calls. A traversal that borrows interior pointers resolves a constructor’s fallback before borrowing, since program lookup can unify equal nouns. The site has the same size and lifetime as on the preceding review. The preceding review is `9ddd3b8720` and develop is `baf98ea9c3`.

**Short traversals in a running ship.** The inputs are the 12,251 path and blob-ID pairs from the same 24 Arvo revisions as the Clay constructor test. The first two rows map the gate `|=(r=[path @t] [%ktcl r])` across 12,251 singleton lists or sets constructed before timing. Its battery also occurs in the compiler. The next three rows include singleton construction in the timed expression. The last two controls amortize preparation over more rows. Each pair of numbers gives the medians from two fresh Kelvin 135 fake ~zod boots. Each boot uses one warm-up and 18 timed Dojo evaluations per build and workload, rotating the three binaries on one core. SHA-256 of the ordered jam product agrees for every evaluation. [Revision selection and map construction](#change-78c2194073).

| Workload | Develop, µs | Previous review, µs | Review, µs | Time vs previous review |
|---|---:|---:|---:|---:|
| Tag prebuilt singleton lists | 2,545.5 / 2,587 | 2,252.5 / 2,283 | 1,750.5 / 1,714.5 | 22.3% less / 24.9% less |
| Tag prebuilt singleton sets | 3,540 / 3,592 | 2,776.5 / 2,798 | 2,236 / 2,201 | 19.5% less / 21.3% less |
| Build and tag singleton lists | 4,698 / 4,741.5 | 3,810 / 3,838 | 3,317.5 / 3,286 | 12.9% less / 14.4% less |
| Build and tag singleton sets | 9,631.5 / 9,704.5 | 7,018 / 7,039.5 | 6,404.5 / 6,343.5 | 8.7% less / 9.9% less |
| Tag singleton lists with a captured value | 3,886 / 3,900 | 3,194 / 3,260.5 | 2,654.5 / 2,622 | 16.9% less / 19.6% less |
| Map one list with a captured value | 563 / 521 | 256.5 / 229.5 | 264.5 / 224.5 | 3.1% longer / 2.2% less |
| Clay `+tako-ford` across 24 maps | 873 / 820.5 | 571.5 / 503.5 | 575.5 / 482.5 | 0.7% longer / 4.2% less |

An additional boot during concurrent replay measures 23.1% less time on the prebuilt lists and 20.2% less on the prebuilt sets. The long-list and Clay controls in that boot take 4.8% longer and 10.4% longer, respectively.

Combined king and worker PSS after the full sequence is 272.61 / 272.59 MiB on develop, 213.54 / 213.84 MiB on previous review, 213.95 / 213.55 MiB on review.

**Inside the compiler.** Identical timers surround each public jet invocation, including preparation and traversal. The table gives the median sum per replay over six alternating trials on each review build. The set gate pairs each sample with a lexical value. Other `+turn` rows show the literal `%ktcl` gate and a larger sample expression.

| Compiler and public jet | Calls per replay | Previous review, ms | Review, ms | Less time |
|---|---:|---:|---:|---:|
| Hoon, captured `+run:in` | 52,152 | 8.984 | 6.408 | 28.7% |
| Hoon, `%ktcl` `+turn` | 1,840 | 0.671 | 0.533 | 20.6% |
| Hoon, larger `+turn` | 84,165 | 34.520 | 33.286 | 3.6% |
| Lull, captured `+run:in` | 13,594 | 2.986 | 2.148 | 28.1% |
| Lull, `%ktcl` `+turn` | 4,294 | 1.158 | 1.007 | 13.0% |
| Lull, larger `+turn` | 65,137 | 26.007 | 24.622 | 5.3% |
| Zuse, captured `+run:in` | 13,086 | 2.449 | 1.683 | 31.2% |
| Zuse, `%ktcl` `+turn` | 862 | 0.267 | 0.207 | 22.5% |
| Zuse, larger `+turn` | 30,095 | 12.902 | 11.911 | 7.7% |

**Whole compiler and replay controls.** For each binary, replay prepares its own checkpoint immediately before the timed events. Four trials per binary rotate the order on one core. Times are the median recorded Dojo evaluations.

| Compiler | Develop, s | Previous review, s | Review, s | Time vs previous review |
|---|---:|---:|---:|---:|
| Hoon | 11.909 | 4.627 | 4.974 | 7.5% longer |
| Lull | 9.594 | 3.106 | 3.063 | 1.4% less |
| Zuse | 3.642 | 1.603 | 1.616 | 0.9% longer |

A second Hoon series alternates the two review binaries for ten trials each. The median is 4.732 s on previous review and 4.725 s on review (0.2% less). The short-gate savings are only a few milliseconds of this complete job.

Four paired cold replays rebuild the ship through event 143 and exchange cores between trials. Median wall time is 115.11 s on previous review and 115.87 s on review (0.7% longer). Median peak RSS is 835.22 and 835.35 MiB, respectively.

**Correctness.** Eight native suites pass on each loom width. Both ways of preparing a gate match develop byte for byte across 4,735 successful products and 3,765 exits, including source traces, on both widths. The bootstrap core, 138 cold replay products and 17 checkpoint products match develop’s canonical jam lengths and SHA-256 on each width (33,279,069,693 bytes per width). All six events in the three compiler windows also match develop on the 32-bit loom (1,439,791,403 bytes). Develop opens the checkpoint review writes at event 294 and replays event 295.

**Wet-gate profile.** Two separately timed compiler replays on the preceding review record 42,363 wet entries to the `+fire:ut` callback during self-hosted Hoon compilation. Of its 7,432 direct calls to `+mull`, 1,253 hit the memo table and 6,179 miss. The miss bodies account for 1.090 / 1.091 s in the instrumented runs, with time in nested prepared gates charged separately. This measures where compilation spends time. An optimization would still need to show how much of that work it can avoid.

<a id="change-7f16135a2d"></a>

### Generate deterministic bits in the output atom

`+raw:og` writes each 256-bit block into its position in the output atom and keeps the previous digest in a fixed buffer. After mixing the seed, remaining width and previous digest, the SHA input ends at its highest nonzero byte.

Twenty successful local logins to a Kelvin 135 fake ~zod each invoke `+raw:og` three times for 128 bits with a 32-byte seed. The first row replays the 60 inputs in order 64 times. Each build also reproduces all 60 recorded draws individually. The remaining rows use `(turn (gulf 1 2.048) shax)` for seeds or, for the single large draw, `(~(raw og 0x3141.5926.5358.9793) 8.388.608)`. Dojo times exclude seed construction and result hashing. Each boot warms once and rotates three builds on one physical core for eight measured trials. Columns show the first boot’s medians.

| Workload | Develop | Previous review | Review | Less time vs previous review, two boots |
|---|---:|---:|---:|---:|
| 3,840 draws from recorded Eyre login inputs | 3.754 ms | 3.676 ms | 2.805 ms | 23.7–24.2% |
| 2,048 Eyre session-key expressions | 2.957 ms | 2.761 ms | 2.375 ms | 12.2–14.0% |
| 2,048 Eyre eauth-token expressions | 3.314 ms | 2.787 ms | 2.430 ms | 12.8–14.5% |
| 2,048 Ames jitter expressions | 4.489 ms | 3.895 ms | 3.292 ms | 15.0–15.5% |
| 2,048 draws of 128 bits | 2.002 ms | 1.890 ms | 1.508 ms | 17.1–20.2% |
| 2,048 draws of 513 bits | 3.732 ms | 3.607 ms | 2.784 ms | 19.4–22.8% |
| One 1 MiB draw | 15.380 ms | 14.989 ms | 9.793 ms | 33.3–34.7% |

Median loopback HTTP login time over 80 requests per build goes from 2.04 to 2.07 ms in one boot pair and 1.96 to 1.96 ms in the other. The log-only cold replay through event 143 makes zero calls to this generator.

In two series run on their own, the Ames packet round-trip tests take 8.6–8.7% less time for `+data` and 7.6–7.8% less for `+all`. Each test uses 96 cases and 12 measured trials per build. With other ship workloads running, the complete test ranges from 7.7% faster to 39.2% slower.

Generating an 8 MiB atom from seed `0x53.4f21` in a fresh process gives the following medians over six alternating runs per build. RSS covers the process. The timer covers the generator call. All results match the same independent SHA-256 digest.

| Loom | Previous review, ms | Review, ms | Previous peak RSS | Review peak RSS |
|---|---:|---:|---:|---:|
| 32-bit | 131.6 | 79.9 | 37.2 MiB | 8.9 MiB |
| 64-bit | 133.1 | 80.4 | 49.2 MiB | 8.8 MiB |

**Correctness.** Both loom widths match an independent SHA-256 implementation of the Hoon recurrence byte for byte on 5,039 seed and width combinations. Cases include seed and width cancellation, partial bytes and blocks, and seeds wider than the digest. Both widths pass eight native suites and match develop’s canonical lengths and SHA-256 hashes for the bootstrap core and 155 replay products. All 34 further event products in a recorded session containing two HTTP logins also match develop on both widths.

<a id="change-c7aab99a51"></a>

### Hash atom storage directly in +shax and +shas

Cold replay through event 143 calls `+shas` with the salt `%sham` on messages of 1,417,051 and 21,780,543 bytes. Both are canonical jams. The table times `(sham noun)` on those exact cued inputs in a Kelvin 135 fake ~zod. Each median covers six measured Dojo evaluations per build after warmup, rotating build order on one core.

| Message in the salted hash | Develop | Previous review `a25b3bc11f` | Review | Time vs previous review |
|---|---:|---:|---:|---:|
| 1.35 MiB | 3.80 ms | 3.41 ms | 3.29 ms | 3.6% less |
| 20.77 MiB | 65.85 ms | 55.52 ms | 51.12 ms | 7.9% less |

For the larger message, `+shas` alone takes 7.1–10.5% less time over two independent boots. After the hash batches, combined king and worker PSS falls from 354.0–354.3 to 336.3–336.7 MiB. In a fresh native process, peak RSS for one call falls from 42.7 to 21.7 MiB on the 32-bit loom and from 42.5 to 21.8 MiB on the 64-bit loom (12 trials each). A synthetic message of the same width can be prepared in Dojo with `=msg (bex 174.244.336)` and timed with `~>(%bout (shas %sham msg))`.

Across 24 Hoon source files sampled by size from the mounted base desk, four passes through Clay's `+page-to-lobe` take 5.34 ms versus 5.59 ms on previous review. Hashing 4,096 small atoms with `+sham` takes 3.10 ms versus 3.07 ms.

During the 20.77 MiB call in an instrumented cold replay, worker RSS grows by 19.24 MiB on previous review and by 0 KiB on review, measured at entry and immediately after hashing. Four paired cold replays swap cores between trials. Median wall time and peak RSS are 109.58 s and 835.3 MiB on previous review and 110.47 s and 816.0 MiB on review.

Both loom widths match Python `hashlib` on all 1,482 `+shax` and `+shas` cases, including empty messages, wide salts and the same atom passed as salt and message. Eight native suites pass on each width. The bootstrap core and all 155 replay products match develop’s canonical jam lengths and SHA-256 hashes (33,279,069,693 bytes per width).

<a id="change-9d47dd576c"></a>

### Select hardware SHA-256 compression in Nettle

The first compression call selects a routine using the host CPU features. SHA-224 and SHA-256 share this routine. Vere ships assembly generated from Nettle's pinned implementation and selects its C compressor on CPUs without the instructions. [Nettle x86-64 source](https://github.com/gnutls/nettle/blob/nettle_4.0_release_20260205/x86_64/sha_ni/sha256-compress-n.asm) · [Nettle AArch64 source](https://github.com/gnutls/nettle/blob/nettle_4.0_release_20260205/arm64/crypto/sha256-compress-n.asm)

The table shows median Dojo time from the first of two independent Kelvin 135 fake ~zod boots on an i7-12700K. Each case warms once. Builds rotate on one physical core for seven to twelve measured evaluations per boot. All builds return the same checked products.

| Workload | Develop | Previous review `f9be5a706c` | Review | Less time vs previous review, two boots |
|---|---:|---:|---:|---:|
| `+sham` on the noun with a 20.77 MiB jam from cold replay | 59.95 ms | 48.91 ms | 12.73 ms | 71.1–74.0% |
| Clay `+page-to-lobe`, four passes over 24 base desk source files | 5.57 ms | 5.30 ms | 1.32 ms | 74.8–75.0% |
| `+sham` on 4,096 small atoms | 3.95 ms | 2.98 ms | 2.08 ms | 30.1–30.3% |
| Ames `+all:test`, 96 packet round-trip cases | 34.57 ms | 29.83 ms | 23.68 ms | 20.6–20.9% |

The portable `x86_64-linux-musl` binaries also run on the same physical CPU. The large `+sham` call takes 63.42 ms on the previous review and 17.64 ms on review. The complete Ames test takes 27.03 and 20.75 ms. The Dojo expression for Ames is ``` ~(all test:ames `@uvJ``@`0x3141.5926 96) ```.

Successful loopback Eyre logins take 2.31 versus 2.27 ms in one boot pair and 2.14 versus 2.13 ms in another (80 requests per build). In eight alternating compiler trials, Hoon takes 5.18 versus 5.28 s, Lull 3.32 versus 3.35 s and Zuse 1.79 s on both. A four-trial series gives a larger Hoon difference (7.10 versus 5.25 s) that does not repeat.

Four paired cold replays through event 143 overlap other workloads. They take 120.81 s on previous review and 121.77 s on review, with peak RSS of 816.1 and 816.2 MiB. Two further pairs after the hash audits finish take 97.64 s and 97.28 s, respectively. Builds exchange cores between pairs.

Both loom widths pass eight native suites and 5,352 hash-jet cases. The bootstrap core and 155 replay products match develop’s canonical lengths and SHA-256 on each width. All six compiler-window products match on the 32-bit loom. SHA-224 and SHA-256 match Python’s `hashlib` for 1,178 byte strings on the physical x86-64 host and under x86-64 and AArch64 emulation. The portable Vere jets suite passes under AArch64 emulation and with SHA masked on x86-64. The Nettle library and a program calling its public hash API cross-link for all nine supported target combinations.

Across the nine targets, the SHA assembly produces three distinct machine-code sequences after linking. Linux, FreeBSD and macOS have identical compression instructions on each architecture. Windows x86-64 has a different calling convention. A Linux harness executes all three sequences, using the Microsoft ABI for the Windows code and QEMU for AArch64. Each matches Python’s SHA-224 and SHA-256 on a further 1,178 inputs, including empty inputs, every byte length through 512 and a 1 MiB + 1 byte message.
