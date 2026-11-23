#!/usr/bin/env bash
# Replay the same disposable event log in stock, shadow and enabled builds.
# Zero repetitions runs only the shadow check.
# BENCH_CPU pins the process on Linux. U3_MEASURE=1 uses protected replay
# and prints the checkpoint page counts alongside the timings.
set -euo pipefail

if (( $# < 2 || $# > 3 )); then
  echo "usage: $0 STOCK_VERE GUARDED_VERE [REPETITIONS]" >&2
  exit 2
fi
stock=$(realpath "$1")
guarded=$(realpath "$2")
rounds=${3:-3}
[[ -x "$stock" && -x "$guarded" && "$rounds" =~ ^(0|[1-9][0-9]*)$ ]]
fixture=$(dirname "$0")/fixtures/zod-v1.21.tar.gz
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
mkdir "$scratch/source"
tar xzf "$fixture" -C "$scratch/source" zod-1.21/.urb/log
affinity=()
[[ -z ${BENCH_CPU:-} ]] || affinity=(taskset -c "$BENCH_CPU")
flags=(-f --no-migrate)
last=72
if [[ ${U3_MEASURE:-0} != 0 ]]; then
  # Protected full replay attempts to roll this flat log to a new epoch
  # after its final save. Stop one event earlier for the memory comparison.
  last=71
  flags+=(-n "$last")
else
  flags+=(-y)
fi

run() {
  local mode=$1 binary=$2
  local pier="$scratch/trial/zod-1.21"
  mkdir -p "$scratch/trial"
  cp -a "$scratch/source/zod-1.21" "$pier"
  echo "mode=$mode"
  if ! U3_GUARD=$mode /usr/bin/time -f 'seconds=%e peak_rss_kib=%M' \
    "${affinity[@]}" "$binary" play "${flags[@]}" "$pier" >"$scratch/log" 2>&1; then
    cat "$scratch/log" >&2
    exit 1
  fi
  grep -Fq "play ($last): done" "$scratch/log"
  grep -E '^(guard sites=|seconds=)' "$scratch/log"
  if [[ "$mode" != off ]]; then
    grep -Eq '^guard sites=[1-9][0-9]* probes=[1-9][0-9]* hits=[1-9]' "$scratch/log"
  fi
  if [[ "$mode" = shadow ]]; then
    awk '/^guard sites=/ {
      split($4, hits, "=")
      split($5, checked, "=")
      if (hits[2] != checked[2]) exit 1
    }' "$scratch/log"
  fi
  if [[ ${U3_MEASURE:-0} != 0 ]]; then
    grep -hE '^(S|P) ' "$pier"/.urb/measure/measure-*.log
  fi
  rm -rf -- "$scratch/trial"
}

run shadow "$guarded"
for (( round=0; round < rounds; round++ )); do
  if (( round % 2 )); then
    run use "$guarded"
    run off "$stock"
  else
    run off "$stock"
    run use "$guarded"
  fi
done
