#!/usr/bin/env bash
# Build the upstream control and cumulative candidate for the develop benchmarks.
set -euo pipefail

if (( $# < 1 || $# > 3 )); then
  echo "usage: $0 OUTPUT_DIRECTORY [UPSTREAM_REF [CANDIDATE_REF]]" >&2
  exit 2
fi
repo=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
out=$(realpath -m "$1")
base=$(git -C "$repo" rev-parse --verify "${2:-origin/develop}^{commit}")
candidate=$(git -C "$repo" rev-parse --verify "${3:-HEAD}^{commit}")
if ! git -C "$repo" merge-base --is-ancestor "$base" "$candidate"; then
  echo "The candidate does not contain the selected upstream commit." >&2
  exit 1
fi
mkdir -p "$out"
features=(DECISION_BENCH OWNER_BENCH REUSE_BENCH SPINE_BENCH DIRECT_EDIT_BENCH
          CUE_RANK NOUN_SHAPES GATE_SAMPLE LIST_SHARING)

for name in develop optimized; do
  rev=$base
  flags=(-Doptimize=ReleaseFast)
  if [[ $name == optimized ]]; then
    rev=$candidate
    for feature in "${features[@]}"; do flags+=("-Dcopt=-DU3_$feature"); done
  fi
  src="$out/$name-src"
  mkdir "$src" "$out/$name"
  git -C "$repo" archive "$rev" | tar -x -C "$src"
  (
    cd "$src"
    patch -p1 --batch --fuzz=0 < "$repo/tests/bench-desk-eval.patch"
    # build.zig reads the second hash in the final HEAD reflog entry.
    mkdir -p .git/logs
    printf '%s %s\n' "$rev" "$rev" > .git/logs/HEAD
    zig build -j"${BENCH_JOBS:-4}" "${flags[@]}" -p "$out/$name"
    "$out/$name/x86_64-linux-musl/urbit" --version
  )
done
