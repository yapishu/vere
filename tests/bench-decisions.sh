#!/usr/bin/env bash
# Each observation gets a fresh event log, loom, and bytecode cache.
set -euo pipefail
if (( $# < 4 || $# > 5 )); then
  echo "usage: $0 time|cross|audit STOCK_VERE EXPERIMENTAL_VERE 1.21|2.12|4.6 [ROUNDS]" >&2
  exit 2
fi
mode=$1
stock=$(realpath "$2")
candidate=$(realpath "$3")
version=$4
rounds=${5:-2}
[[ -x "$stock" && -x "$candidate" && "$rounds" =~ ^[1-9][0-9]*$ ]]
play_flags=(-f -y --no-migrate)
case $version in
  1.21) first=5; last=72 ;;
  2.12) first=4; last=70 ;;
  4.6)
    : "${BENCH_FROZEN_PIER:?set the frozen 4.6 input pier}" "${BENCH_FIRST_EVENT:?}" "${BENCH_LAST_EVENT:?}"
    first=$BENCH_FIRST_EVENT; last=$BENCH_LAST_EVENT
    [[ $first =~ ^[1-9][0-9]*$ && $last =~ ^[1-9][0-9]*$ ]] && (( first <= last )) || exit 2
    [[ ! -e "$BENCH_FROZEN_PIER/.vere.lock" && $(<"$BENCH_FROZEN_PIER/.urb/log/0i0/vere.txt") == 4.6 ]] || exit 2
    play_flags=(-f -y)
    ;;
  *) exit 2 ;;
esac
case $mode in
  time|audit) ;;
  cross) : "${BENCH_CPU:?set the stock CPU}" "${BENCH_PEER_CPU:?set the second CPU}"
         [[ $BENCH_CPU != "$BENCH_PEER_CPU" ]] ;;
  *) exit 2 ;;
esac
fixture="$(dirname "$0")/fixtures/zod-v$version.tar.gz"
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
ship="zod-$version"
if [[ $version == 4.6 ]]; then
  mkdir -p "$scratch/$ship/.urb"
  cp -a "$BENCH_FROZEN_PIER/.urb/log" "$scratch/$ship/.urb/"
else
  tar xzf "$fixture" -C "$scratch" "$ship/.urb/log"
fi

run() {
  local label=$1 binary=$2 cpu=${3:-${BENCH_CPU:-}}
  local log="$scratch/$label.log" trial="$scratch/$label" timing="$scratch/$label.time"
  local -a affinity=()
  [[ -z $cpu ]] || affinity=(taskset -c "$cpu")
  mkdir "$trial"
  cp -a "$scratch/$ship" "$trial/$ship"
  if ! /usr/bin/time -f 'seconds=%e peak_rss_kib=%M' -o "$timing" \
       "${affinity[@]}" "$binary" play "${play_flags[@]}" "$trial/$ship" >"$log" 2>&1 ||
     ! grep -Fq "play: events $first-$last" "$log" ||
     ! grep -Fq "play ($last): done" "$log"; then
    tail -40 "$log" >&2
    exit 1
  fi
  if [[ $mode == audit ]]; then
    awk -v first="$first" -v last="$last" '
      $1 == "bootstrap-core" {
        if ($2 != first-1 || $3 !~ /^[1-9][0-9]*$/ ||
            $4 !~ /^[a-f0-9]+$/ || length($4) != 64 || NF != 4) bad = 1
        print
        boots++
      }
      $1 == "product" {
        if ($2 != first + n || $3 !~ /^[1-9][0-9]*$/ ||
            $4 !~ /^[a-f0-9]+$/ || length($4) != 64 || NF != 4) bad = 1
        print
        n++
      }
      END { if (bad || boots != 1 || n != last-first+1) exit 1 }
    ' "$log" >"$scratch/$label.products"
  else
    if grep -q '^product ' "$log"; then
      echo 'Use timing builds without U3_REPLAY_AUDIT' >&2
      exit 1
    fi
    echo "$version $label $(cat "$timing")"
  fi
  rm -rf -- "$trial"
}

if [[ $mode == audit ]]; then
  run stock "$stock"
  run candidate "$candidate"
  diff -u "$scratch/stock.products" "$scratch/candidate.products"
  echo "$version verified $((last-first+1)) event products ($first through $last)"
else
  for ((i=1; i<=rounds; ++i)); do
    if [[ $mode == cross ]]; then
      cpu=$BENCH_CPU; peer=$BENCH_PEER_CPU
      if (( i % 2 == 0 )); then cpu=$BENCH_PEER_CPU; peer=$BENCH_CPU; fi
      run "stock-$i" "$stock" "$cpu" & stock_pid=$!
      run "candidate-$i" "$candidate" "$peer" & candidate_pid=$!
      status=0
      wait "$stock_pid" || status=1
      wait "$candidate_pid" || status=1
      (( status == 0 )) || exit 1
    elif (( i % 2 )); then
      run "stock-$i" "$stock"
      run "candidate-$i" "$candidate"
    else
      run "candidate-$i" "$candidate"
      run "stock-$i" "$stock"
    fi
  done
fi
