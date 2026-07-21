#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
URBIT_REPO="${URBIT_REPO:-$(cd "$ROOT_DIR/../urbit" && pwd)}"
VERE_BIN="${VERE_BIN:-$ROOT_DIR/zig-out/x86_64-linux-musl/urbit}"
PILL="${PILL:-$URBIT_REPO/bin/brass.pill}"
WT_DIR="$ROOT_DIR/tools/ames-wt-bridge"

WORK_DIR="${WORK_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/ames-quic-browser-galaxy.XXXXXX")}"
ARVO_DIR="$WORK_DIR/pkg/arvo"
REUSE_PIERS="${REUSE_PIERS:-0}"
PREPARE_ONLY="${PREPARE_ONLY:-0}"

ZOD_UDP_PORT="${ZOD_UDP_PORT:-52337}"
BINZOD_UDP_PORT="${BINZOD_UDP_PORT:-52339}"
ZOD_QUIC_PORT="${ZOD_QUIC_PORT:-19543}"
BRIDGE_PORT="${BRIDGE_PORT:-18443}"
HTTP_PORT="${HTTP_PORT:-18093}"
CDP_PORT="${CDP_PORT:-19222}"
BOOT_TIMEOUT="${BOOT_TIMEOUT:-1200}"
SMOKE_TIMEOUT_MS="${SMOKE_TIMEOUT_MS:-600000}"
WORKER_MODE="${WORKER_MODE:-auto}"

NODE="${NODE:-node}"
CHROME="${CHROME:-$(command -v google-chrome || command -v chromium || command -v chromium-browser || true)}"

declare -A PIER=(
  [zod]="$WORK_DIR/fzod"
  [binzod]="$WORK_DIR/fbinzod"
)

declare -A LOG=(
  [zod]="$WORK_DIR/zod.log"
  [binzod]="$WORK_DIR/binzod.log"
)

declare -A HTTP_LOOPBACK=()
declare -A START_PID=()

BRIDGE_PID=""
HTTP_PID=""
CHROME_PID=""

log() {
  printf '%s\n' "$*" >&2
}

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

require_port_free() {
  local kind="$1"
  local port="$2"

  if [[ "$kind" == "tcp" ]]; then
    if ss -ltn "( sport = :$port )" | tail -n +2 | rg -q .; then
      log "tcp port $port is already in use"
      exit 1
    fi
  else
    if ss -lun "( sport = :$port )" | tail -n +2 | rg -q .; then
      log "udp port $port is already in use"
      exit 1
    fi
  fi
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

clear_stale_lock() {
  local ship="$1"
  local pid

  pid="$(ship_pid "$ship" || true)"
  if [[ -n "$pid" ]] && ! kill -0 "$pid" 2>/dev/null; then
    rm -f "${PIER[$ship]}/.vere.lock"
  fi
}

check_ship_alive() {
  local ship="$1"

  if [[ -n "${START_PID[$ship]:-}" ]] &&
     ! kill -0 "${START_PID[$ship]}" 2>/dev/null &&
     ! is_running "$ship"; then
    log "~$ship exited before becoming ready"
    if [[ -f "${LOG[$ship]}" ]]; then
      tail -80 "${LOG[$ship]}" >&2 || true
    fi
    exit 1
  fi
}

lens_dojo() {
  local ship="$1"
  local cmd="$2"
  local app="${3:-}"
  local timeout="${4:-60}"
  local port="${HTTP_LOOPBACK[$ship]}"
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

  if [[ -n "${HTTP_LOOPBACK[$ship]:-}" ]] && is_running "$ship"; then
    lens_dojo "$ship" '+hood/exit' hood 15 >/dev/null 2>&1 || true
  fi
}

cleanup() {
  local status=$?

  if [[ -n "$CHROME_PID" ]]; then
    kill "$CHROME_PID" 2>/dev/null || true
  fi
  if [[ -n "$BRIDGE_PID" ]]; then
    kill "$BRIDGE_PID" 2>/dev/null || true
  fi
  if [[ -n "$HTTP_PID" ]]; then
    kill "$HTTP_PID" 2>/dev/null || true
  fi

  if [[ "${KEEP_SHIPS:-0}" != 1 ]]; then
    stop_ship binzod
    stop_ship zod

    sleep 2

    for ship in binzod zod; do
      if is_running "$ship"; then
        kill "$(ship_pid "$ship")" 2>/dev/null || true
      fi
      if [[ -n "${START_PID[$ship]:-}" ]]; then
        kill "${START_PID[$ship]}" 2>/dev/null || true
      fi
    done
  fi

  if [[ $status -ne 0 ]]; then
    log "browser fake-galaxy demo failed; logs are in $WORK_DIR"
    for path in "$WORK_DIR"/*.log; do
      if [[ -f "$path" ]]; then
        log "--- tail $path ---"
        tail -80 "$path" >&2 || true
      fi
    done
  elif [[ "${KEEP_WORK:-0}" == 1 || "${KEEP_SHIPS:-0}" == 1 ]]; then
    log "browser fake-galaxy demo artifacts kept in $WORK_DIR"
  else
    rm -rf "$WORK_DIR"
  fi

  exit "$status"
}

trap cleanup EXIT

wait_for_file() {
  local path="$1"
  local timeout="$2"
  local ship="${3:-}"
  local start

  start="$(date +%s)"
  while [[ ! -f "$path" ]]; do
    if [[ -n "$ship" ]]; then
      check_ship_alive "$ship"
    fi
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

  wait_for_file "$ports_file" "$BOOT_TIMEOUT" "$ship"

  start="$(date +%s)"
  while true; do
    check_ship_alive "$ship"
    port="$(awk '/loopback/ { print $1; exit }' "$ports_file")"
    if [[ -n "$port" ]]; then
      HTTP_LOOPBACK[$ship]="$port"
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

  log "$ship HTTP: ${HTTP_LOOPBACK[$ship]}"
}

wait_for_log() {
  local ship="$1"
  local pattern="$2"
  local timeout="$3"
  local start

  start="$(date +%s)"
  while true; do
    check_ship_alive "$ship"
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
    check_ship_alive "$ship"
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
  : >"${LOG[$ship]}"

  if [[ "$REUSE_PIERS" == 1 ]]; then
    clear_stale_lock "$ship"
    "$VERE_BIN" \
      -d \
      -L \
      -p "$udp_port" \
      --http-port 0 \
      "$@" \
      "${PIER[$ship]}" >>"${LOG[$ship]}" 2>&1 &
  else
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
  fi

  START_PID[$ship]=$!
}

hood_pass() {
  local ship="$1"
  local task="$2"
  local cmd="+hood/pass $task"

  log "~$ship hood> $cmd"
  lens_dojo "$ship" "$cmd" hood 120 | tee -a "$WORK_DIR/dojo-$ship.log"
  printf '\n' | tee -a "$WORK_DIR/dojo-$ship.log" >/dev/null
}

ames_ping() {
  local ship="$1"
  local peer="$2"

  hood_pass "$ship" "[%g %deal [~$ship ~$peer /] %ping %poke %noun !>(~)]"
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

urlencode() {
  python3 - "$1" <<'PY'
import sys
from urllib.parse import quote

print(quote(sys.argv[1], safe=''))
PY
}

wait_for_bridge_hash() {
  local log_file="$1"
  local timeout="$2"
  local start hash

  start="$(date +%s)"
  while true; do
    hash="$(awk -F': ' '/dev cert sha-256/ { print $NF; exit }' "$log_file" 2>/dev/null || true)"
    if [[ -n "$hash" ]]; then
      printf '%s\n' "$hash"
      return
    fi
    if (( $(date +%s) - start > timeout )); then
      log "timed out waiting for bridge certificate hash"
      exit 1
    fi
    sleep 1
  done
}

run_browser_smoke() {
  local cert_hash="$1"
  local chrome_profile="$WORK_DIR/chrome-profile"
  local url

  mkdir -p "$chrome_profile"
  url="http://localhost:$HTTP_PORT/vere/tools/ames-wt-bridge/web/wasm-vere-disk-route-smoke.html"
  url+="?url=$(urlencode "https://127.0.0.1:$BRIDGE_PORT/~_~/ames")"
  url+="&hash=$(urlencode "$cert_hash")"
  url+="&fake-ship=0x100"
  url+="&peer=0x200"
  url+="&mate-peer=0x200"
  url+="&worker=$(urlencode "$WORKER_MODE")"
  url+="&scope=browser-fake-galaxy-$(date +%s)"

  log "browser route smoke: $url"
  "$CHROME" \
    --headless=new \
    --no-sandbox \
    --disable-gpu \
    --user-data-dir="$chrome_profile" \
    --remote-debugging-port="$CDP_PORT" \
    --remote-allow-origins='*' \
    "$url" >>"$WORK_DIR/chrome.log" 2>&1 &
  CHROME_PID=$!

  "$NODE" --experimental-websocket \
    "$WT_DIR/cdp-wait-smoke.mjs" \
    --port "$CDP_PORT" \
    --match wasm-vere-disk-route-smoke.html \
    --timeout-ms "$SMOKE_TIMEOUT_MS" |
    tee "$WORK_DIR/browser-smoke.log"
}

require_file "$VERE_BIN" "vere binary"
require_file "$PILL" "brass pill"
require_file "$ROOT_DIR/zig-out/bin/vere-disk-wasm.wasm" "vere-disk-wasm artifact"
require_dir "$URBIT_REPO/pkg" "urbit pkg tree"
require_file "$WT_DIR/cdp-wait-smoke.mjs" "CDP smoke waiter"

if [[ -z "$CHROME" ]]; then
  log "missing Chrome/Chromium executable; set CHROME=/path/to/chrome"
  exit 1
fi

require_port_free udp "$ZOD_UDP_PORT"
require_port_free udp "$BINZOD_UDP_PORT"
require_port_free udp "$ZOD_QUIC_PORT"
require_port_free udp "$BRIDGE_PORT"
require_port_free tcp "$HTTP_PORT"
require_port_free tcp "$CDP_PORT"

log "work dir: $WORK_DIR"
mkdir -p "$WORK_DIR"
if [[ "$REUSE_PIERS" == 1 ]]; then
  log "reusing existing piers"
  require_dir "${PIER[zod]}/.urb" "existing ~zod pier"
  require_dir "${PIER[binzod]}/.urb" "existing ~binzod pier"
else
  if [[ -e "${PIER[zod]}" || -e "${PIER[binzod]}" ]]; then
    log "work dir already contains piers; set REUSE_PIERS=1 or use a fresh WORK_DIR"
    exit 1
  fi
  log "copying urbit pkg tree"
  cp -a "$URBIT_REPO/pkg" "$WORK_DIR/pkg"
fi

start_ship zod "$ZOD_UDP_PORT" \
  --ames-quic-port "$ZOD_QUIC_PORT" \
  --no-ames-mdns \
  --ames-quic-log
if [[ "$PREPARE_ONLY" == 1 ]]; then
  start_ship binzod "$BINZOD_UDP_PORT" \
    --ames-quic-port 0 \
    --no-ames-mdns \
    --ames-quic-log
else
  start_ship binzod "$BINZOD_UDP_PORT" \
    --ames-quic-port 0 \
    --ames-quic-sponsor \
    --ames-quic-sponsor-port "$ZOD_QUIC_PORT" \
    --no-ames-mdns \
    --ames-quic-log
fi

for ship in zod binzod; do
  wait_for_http "$ship"
  wait_for_log "$ship" 'ames: mdns disabled' 180
  wait_for_log "$ship" 'mesa: quic live on' 180
done

if [[ "$PREPARE_ONLY" == 1 ]]; then
  log "browser fake-galaxy piers prepared in $WORK_DIR"
  log "rerun with REUSE_PIERS=1 WORK_DIR=$WORK_DIR to execute the route smoke"
  exit 0
fi

bootstrap_mesa zod binzod
bootstrap_mesa binzod zod marzod

log "binding native ~binzod to fake ~zod over raw QUIC"
ames_ping binzod zod
wait_for_log zod 'mesa: quic: handshake complete: 127\.0\.0\.1:' 120
wait_for_log zod 'mesa: session bind 0x0\.0000000000000200 -> [0-9]+' 120

log "starting WebTransport bridge to fake ~zod"
(
  cd "$WT_DIR"
  go run ./cmd/bridge \
    -listen "127.0.0.1:$BRIDGE_PORT" \
    -ames "127.0.0.1:$ZOD_UDP_PORT" \
    -dev
) >>"$WORK_DIR/bridge.log" 2>&1 &
BRIDGE_PID=$!
cert_hash="$(wait_for_bridge_hash "$WORK_DIR/bridge.log" 60)"
log "bridge cert sha-256: $cert_hash"

log "serving parent workspace for browser artifacts"
python3 -m http.server "$HTTP_PORT" --directory "$ROOT_DIR/.." \
  >>"$WORK_DIR/http.log" 2>&1 &
HTTP_PID=$!

run_browser_smoke "$cert_hash"
wait_for_log zod 'mesa: forward 0x0\.0000000000000200 over session [0-9]+' 120

log "browser fake-galaxy demo completed"
log "logs are in $WORK_DIR"
