#!/usr/bin/env bash
set -euo pipefail

TOOLS_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
VERE_ROOT="$(cd -- "$TOOLS_DIR/../.." && pwd)"
URBIT_REPO="${URBIT_REPO:-"$VERE_ROOT/../urbit"}"
PILL="${PILL:-"$URBIT_REPO/bin/brass.pill"}"
OUT="${OUT:-"$TOOLS_DIR/dist/wasm-dojo-demo"}"
CACHE_DIR="${CACHE_DIR:-"$TOOLS_DIR/cache"}"
TLON_PROPS_4_6_PILL="${TLON_PROPS_4_6_PILL:-"$CACHE_DIR/brass-4.6.pill"}"
TLON_PROPS_4_6_PILL_URL="${TLON_PROPS_4_6_PILL_URL:-https://bootstrap.tlon.network/props/4.6/brass.pill}"

cd "$VERE_ROOT"
zig build -Doptimize=ReleaseFast vere-disk-wasm

if [[ ! -f "$PILL" ]]; then
  echo "missing brass pill: $PILL" >&2
  exit 1
fi

rm -rf "$OUT"
mkdir -p "$OUT/assets"
mkdir -p "$OUT/assets/debug"
mkdir -p "$OUT/apps"
mkdir -p "$OUT/vendor/xterm"

runtime_modules=(
  ames-client.mjs
  ames-ship.mjs
  ames-transport.mjs
  ames-wasm-behn.mjs
  ames-wasm-effects.mjs
  ames-wasm-events.mjs
  ames-wasm-http-client.mjs
  ames-wasm-http-server.mjs
  ames-wasm-router.mjs
  ames-wasm-runtime-loop.mjs
  ames-wasm-runtime-route-demo.mjs
  ames-wasm-runtime-service.mjs
  ames-wasm-runtime-worker.mjs
  ames-wasm-terminal.mjs
  mesa-pact.mjs
  urbit-noun.mjs
  vere-wasm-host.mjs
  vere-wasm-memory.mjs
)

for module in "${runtime_modules[@]}"; do
  cp "$TOOLS_DIR/web/$module" "$OUT/"
done
cp "$TOOLS_DIR/web/wasm-webterm.html" "$OUT/index.html"
cp "$TOOLS_DIR/web/vere-wasm-eyre-resource-worker.js" "$OUT/apps/__vere_wasm_sw.js"
cp "$TOOLS_DIR/web/vendor/xterm/xterm.js" "$OUT/vendor/xterm/xterm.js"
cp "$TOOLS_DIR/web/vendor/xterm/xterm.js.map" "$OUT/vendor/xterm/xterm.js.map"
cp "$TOOLS_DIR/web/vendor/xterm/xterm.css" "$OUT/vendor/xterm/xterm.css"
cp "$TOOLS_DIR/web/vendor/xterm/LICENSE" "$OUT/vendor/xterm/LICENSE"
cp "$TOOLS_DIR/serve-wasm-demo.mjs" "$OUT/serve-wasm-demo.mjs"
cp "$VERE_ROOT/zig-out/bin/vere-disk-wasm.wasm" "$OUT/assets/vere-disk-wasm.wasm"
cp "$PILL" "$OUT/assets/brass.pill"
if [[ "${SKIP_TLON_PROPS_4_6_PILL:-0}" != 1 ]]; then
  mkdir -p "$CACHE_DIR"
  if [[ ! -f "$TLON_PROPS_4_6_PILL" ]]; then
    curl -fL --retry 3 --connect-timeout 20 \
      "$TLON_PROPS_4_6_PILL_URL" \
      -o "$TLON_PROPS_4_6_PILL.tmp"
    mv "$TLON_PROPS_4_6_PILL.tmp" "$TLON_PROPS_4_6_PILL"
  fi
  cp "$TLON_PROPS_4_6_PILL" "$OUT/assets/brass-4.6.pill"
fi
cp "$URBIT_REPO/pkg/arvo/app/debug/"*.js "$OUT/assets/debug/"
cp "$URBIT_REPO/pkg/arvo/app/debug/"*.css "$OUT/assets/debug/"
cp "$URBIT_REPO/pkg/arvo/app/debug/"*.html "$OUT/assets/debug/"

cat > "$OUT/Dockerfile" <<'DOCKER'
FROM node:24-alpine
WORKDIR /usr/share/vere-wasm-dojo-demo
COPY . .
EXPOSE 8080
ENV HOST=0.0.0.0
ENV PORT=8080
ENV ROOT=/usr/share/vere-wasm-dojo-demo
CMD ["node", "serve-wasm-demo.mjs"]
DOCKER

cat > "$OUT/README.md" <<'README'
# Vere WASM Dojo Demo Bundle

Serve this directory with the included host server:

```sh
BIND=0.0.0.0 PORT=8093 ./serve.sh
```

Then open:

```text
http://localhost:8093/
```

Or build and run the container:

```sh
docker build -t vere-wasm-dojo-demo .
docker run --rm -p 0.0.0.0:8093:8080 vere-wasm-dojo-demo
```

The browser runtime services Urbit's own `%http-client` effects. Browser
JavaScript cannot read arbitrary cross-origin response bodies, so this bundle
uses the included `/_vere/http-client` host proxy. The WASM runtime emits the
same Iris HTTP requests it would emit natively; the proxy performs those
outbound HTTP/HTTPS requests outside the browser CORS sandbox and streams the
responses back into the runtime.
README

cat > "$OUT/serve.sh" <<'SH'
#!/usr/bin/env sh
set -eu
node ./serve-wasm-demo.mjs --host "${BIND:-0.0.0.0}" --port "${PORT:-8093}" --root .
SH
chmod +x "$OUT/serve.sh"
find "$OUT" -type d -exec chmod 0755 {} +
find "$OUT" -type f -exec chmod 0644 {} +
chmod +x "$OUT/serve.sh"

cat <<EOF
wrote $OUT

Serve locally:
  cd "$OUT"
  BIND=0.0.0.0 PORT=8093 ./serve.sh

Or as a container:
  cd "$OUT"
  docker build -t vere-wasm-dojo-demo .
  docker run --rm -p 0.0.0.0:8093:8080 vere-wasm-dojo-demo
EOF
