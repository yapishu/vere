#!/usr/bin/env bash
set -euo pipefail

TOOLS_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
VERE_ROOT="$(cd -- "$TOOLS_DIR/../.." && pwd)"
URBIT_REPO="${URBIT_REPO:-"$VERE_ROOT/../urbit"}"
PILL="${PILL:-"$URBIT_REPO/bin/brass.pill"}"
OUT="${OUT:-"$TOOLS_DIR/dist/wasm-dojo-demo"}"

cd "$VERE_ROOT"
zig build -Doptimize=ReleaseFast vere-disk-wasm

if [[ ! -f "$PILL" ]]; then
  echo "missing brass pill: $PILL" >&2
  exit 1
fi

rm -rf "$OUT"
mkdir -p "$OUT/assets"

runtime_modules=(
  ames-client.mjs
  ames-transport.mjs
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
  urbit-noun.mjs
  vere-wasm-host.mjs
  vere-wasm-memory.mjs
)

for module in "${runtime_modules[@]}"; do
  cp "$TOOLS_DIR/web/$module" "$OUT/"
done
cp "$TOOLS_DIR/web/wasm-webterm.html" "$OUT/index.html"
cp "$VERE_ROOT/zig-out/bin/vere-disk-wasm.wasm" "$OUT/assets/vere-disk-wasm.wasm"
cp "$PILL" "$OUT/assets/brass.pill"

cat > "$OUT/nginx.conf" <<'CONF'
events {}

http {
  include /etc/nginx/mime.types;
  types {
    text/javascript mjs;
  }

  server {
    listen 8080;
    server_name _;
    root /usr/share/nginx/html;
    index index.html;

    location / {
      try_files $uri $uri/ /index.html;
    }
  }
}
CONF

cat > "$OUT/Dockerfile" <<'DOCKER'
FROM nginx:1.27-alpine
COPY nginx.conf /etc/nginx/nginx.conf
COPY . /usr/share/nginx/html
DOCKER

cat > "$OUT/README.md" <<'README'
# Vere WASM Dojo Demo Bundle

Serve this directory as static files:

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
README

cat > "$OUT/serve.sh" <<'SH'
#!/usr/bin/env sh
set -eu
python3 -m http.server "${PORT:-8093}" --bind "${BIND:-0.0.0.0}"
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
