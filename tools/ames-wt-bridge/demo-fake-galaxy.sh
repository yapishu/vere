#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
URBIT_REPO="${URBIT_REPO:-$(cd "$ROOT_DIR/../urbit" && pwd)}"
VERE_BIN="${VERE_BIN:-$ROOT_DIR/zig-out/x86_64-linux-musl/urbit}"
PILL="${PILL:-$URBIT_REPO/bin/brass.pill}"

WORK_DIR="${WORK_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/ames-quic-fake-galaxy.XXXXXX")}"
ARVO_DIR="$WORK_DIR/pkg/arvo"
ZOD_QUIC_PORT="${ZOD_QUIC_PORT:-9443}"
BOOT_TIMEOUT="${BOOT_TIMEOUT:-900}"

ZOD_UDP_PORT="${ZOD_UDP_PORT:-31337}"
MARZOD_UDP_PORT="${MARZOD_UDP_PORT:-31338}"
BINZOD_UDP_PORT="${BINZOD_UDP_PORT:-31339}"

declare -A PIER=(
  [zod]="$WORK_DIR/fzod"
  [marzod]="$WORK_DIR/fmarzod"
  [binzod]="$WORK_DIR/fbinzod"
)

declare -A LOG=(
  [zod]="$WORK_DIR/zod.log"
  [marzod]="$WORK_DIR/marzod.log"
  [binzod]="$WORK_DIR/binzod.log"
)

declare -A HTTP_PORT=()
declare -A START_PID=()

log() {
  printf '%s\n' "$*" >&2
}

ship_pid() {
  local ship="$1"
  local lock="${PIER[$ship]}/.vere.lock"

  if [[ -f "$lock" ]]; then
    cat "$lock"
  fi
}

is_running() {
  local ship="$1"
  local pid

  pid="$(ship_pid "$ship" || true)"
  [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null
}

lens_dojo() {
  local ship="$1"
  local cmd="$2"
  local app="${3:-}"
  local timeout="${4:-60}"
  local port="${HTTP_PORT[$ship]}"
  local payload

  payload="$(
    AMES_DEMO_CMD="$cmd" AMES_DEMO_APP="$app" python3 - <<'PY'
import json
import os

sink = {"app": os.environ["AMES_DEMO_APP"]} if os.environ["AMES_DEMO_APP"] else {"stdout": None}
print(json.dumps({"source": {"dojo": os.environ["AMES_DEMO_CMD"]}, "sink": sink}))
PY
  )"

  curl -fsS \
    --max-time "$timeout" \
    -H "Content-Type: application/json" \
    --data "$payload" \
    "http://127.0.0.1:$port" |
    { xargs printf '%s' | sed 's/\\n/\n/g' || true; }
}

stop_ship() {
  local ship="$1"

  if [[ -n "${HTTP_PORT[$ship]:-}" ]] && is_running "$ship"; then
    lens_dojo "$ship" '+hood/exit' hood 15 >/dev/null 2>&1 || true
  fi
}

cleanup() {
  local status=$?

  if [[ "${KEEP_SHIPS:-0}" != 1 ]]; then
    stop_ship binzod
    stop_ship marzod
    stop_ship zod

    sleep 2

    for ship in binzod marzod zod; do
      if is_running "$ship"; then
        kill "$(ship_pid "$ship")" 2>/dev/null || true
      fi
      if [[ -n "${START_PID[$ship]:-}" ]]; then
        kill "${START_PID[$ship]}" 2>/dev/null || true
      fi
    done
  fi

  if [[ $status -ne 0 ]]; then
    log "demo failed; logs are in $WORK_DIR"
    for ship in zod marzod binzod; do
      if [[ -f "${LOG[$ship]}" ]]; then
        log "--- tail ${LOG[$ship]} ---"
        tail -80 "${LOG[$ship]}" >&2 || true
      fi
    done
  elif [[ "${KEEP_WORK:-0}" == 1 || "${KEEP_SHIPS:-0}" == 1 ]]; then
    log "demo artifacts kept in $WORK_DIR"
  else
    rm -rf "$WORK_DIR"
  fi

  exit "$status"
}

trap cleanup EXIT

require_file() {
  local path="$1"
  local label="$2"

  if [[ ! -f "$path" ]]; then
    log "missing $label: $path"
    exit 1
  fi
}

require_dir() {
  local path="$1"
  local label="$2"

  if [[ ! -d "$path" ]]; then
    log "missing $label: $path"
    exit 1
  fi
}

wait_for_file() {
  local path="$1"
  local timeout="$2"
  local start

  start="$(date +%s)"
  while [[ ! -f "$path" ]]; do
    if (( $(date +%s) - start > timeout )); then
      log "timed out waiting for $path"
      exit 1
    fi
    sleep 1
  done
}

wait_for_http() {
  local ship="$1"
  local ports_file="${PIER[$ship]}/.http.ports"
  local start port

  wait_for_file "$ports_file" "$BOOT_TIMEOUT"

  start="$(date +%s)"
  while true; do
    port="$(awk '/loopback/ { print $1; exit }' "$ports_file")"
    if [[ -n "$port" ]]; then
      HTTP_PORT[$ship]="$port"
      if lens_dojo "$ship" '3' "" 15 >/dev/null 2>&1; then
        break
      fi
    fi

    if (( $(date +%s) - start > BOOT_TIMEOUT )); then
      log "timed out waiting for $ship HTTP control plane"
      exit 1
    fi
    sleep 2
  done

  log "$ship HTTP: ${HTTP_PORT[$ship]}"
}

wait_for_log() {
  local ship="$1"
  local pattern="$2"
  local timeout="$3"
  local start

  start="$(date +%s)"
  while true; do
    if rg -q "$pattern" "${LOG[$ship]}" 2>/dev/null; then
      break
    fi
    if (( $(date +%s) - start > timeout )); then
      log "timed out waiting for $ship log pattern: $pattern"
      exit 1
    fi
    sleep 2
  done
}

wait_for_log_count() {
  local ship="$1"
  local pattern="$2"
  local count="$3"
  local timeout="$4"
  local start seen

  start="$(date +%s)"
  while true; do
    seen="$(rg -c "$pattern" "${LOG[$ship]}" 2>/dev/null || true)"
    if [[ -n "$seen" ]] && (( seen >= count )); then
      break
    fi
    if (( $(date +%s) - start > timeout )); then
      log "timed out waiting for $count $ship log matches: $pattern"
      exit 1
    fi
    sleep 2
  done
}

start_ship() {
  local ship="$1"
  local udp_port="$2"
  shift 2

  log "starting ~$ship"
  "$VERE_BIN" \
    -F "$ship" \
    -B "$PILL" \
    -A "$ARVO_DIR" \
    -l \
    -d \
    -p "$udp_port" \
    --http-port 0 \
    -c "${PIER[$ship]}" \
    "$@" >>"${LOG[$ship]}" 2>&1 &

  START_PID[$ship]=$!
}

dojo() {
  local ship="$1"
  local cmd="$2"

  log "~$ship dojo> $cmd"
  lens_dojo "$ship" "$cmd" "" 120 | tee -a "$WORK_DIR/dojo-$ship.log"
  printf '\n' | tee -a "$WORK_DIR/dojo-$ship.log" >/dev/null
}

hood_pass() {
  local ship="$1"
  local task="$2"
  local cmd="+hood/pass $task"

  log "~$ship hood> $cmd"
  lens_dojo "$ship" "$cmd" hood 120 | tee -a "$WORK_DIR/dojo-$ship.log"
  printf '\n' | tee -a "$WORK_DIR/dojo-$ship.log" >/dev/null
}

bootstrap_mesa() {
  local ship="$1"
  shift

  hood_pass "$ship" '[%a %load %mesa]'

  for peer in "$@"; do
    hood_pass "$ship" "[%a %mate \`~$peer %.n]"
    hood_pass "$ship" "[%a %tame ~$peer]"
  done
}

require_file "$VERE_BIN" "vere binary"
require_file "$PILL" "brass pill"
require_dir "$URBIT_REPO/pkg" "urbit pkg tree"

log "work dir: $WORK_DIR"
log "copying urbit pkg tree"
mkdir -p "$WORK_DIR"
cp -a "$URBIT_REPO/pkg" "$WORK_DIR/pkg"

start_ship zod "$ZOD_UDP_PORT" --ames-quic-port "$ZOD_QUIC_PORT"
start_ship marzod "$MARZOD_UDP_PORT" \
  --ames-quic-port 0 \
  --ames-quic-sponsor \
  --ames-quic-sponsor-port "$ZOD_QUIC_PORT"
start_ship binzod "$BINZOD_UDP_PORT" \
  --ames-quic-port 0 \
  --ames-quic-sponsor \
  --ames-quic-sponsor-port "$ZOD_QUIC_PORT"

for ship in zod marzod binzod; do
  wait_for_http "$ship"
  wait_for_log "$ship" 'mesa: quic live on' 180
done

bootstrap_mesa zod marzod binzod
bootstrap_mesa marzod zod binzod
bootstrap_mesa binzod zod marzod

log "binding both children to the fake galaxy over raw QUIC"
dojo marzod '|hi ~zod'
dojo binzod '|hi ~zod'

wait_for_log_count zod 'mesa: quic: handshake complete: 127\.0\.0\.1:' 2 120
sleep 8

log "exercising relay egress through the fake galaxy"
dojo marzod '|hi ~binzod'
sleep 8
dojo binzod '|hi ~marzod'
sleep 8

log "fake galaxy demo completed"
log "logs are in $WORK_DIR"
