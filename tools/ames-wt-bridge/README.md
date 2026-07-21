# ames-wt-bridge

Local tooling for Ames over WebTransport, raw QUIC Ames, and the browser/WASM
fake-galaxy demo. The protocol and implementation reference is
`../urbit/doc/spec/ames-over-quic.md`.

## Tools

The Go module builds three small binaries:

- `bridge`: WebTransport server on `/~_~/ames`, forwarding packets to a local
  Ames UDP port. This is the browser entry point used by the demos.
- `dial`: UDP-to-WebTransport client, useful for transparent bridge testing.
- `rawdial`: UDP-to-raw-QUIC client using `alpn=ames/1`, useful for native
  raw-QUIC smoke tests.

Packet mapping is shared by all three: one Ames/Mesa packet is sent as one
QUIC datagram when possible; packets too large for the session datagram size
fall back to one ephemeral unidirectional stream.

## Build

From the Vere repo:

```sh
cd /home/reid/gits/tlon/vere
zig build
zig build pact-test mesa-test ames-test quic-loopback vere-disk-wasm
```

The browser fake-galaxy demo expects:

- `zig-out/x86_64-linux-musl/urbit`
- `zig-out/bin/vere-disk-wasm.wasm`
- sibling repo `/home/reid/gits/tlon/urbit`
- `/home/reid/gits/tlon/urbit/bin/brass.pill`
- Chrome, Chromium, or Edge with WebTransport support

Override paths with `VERE_BIN=...`, `URBIT_REPO=...`, `PILL=...`, or
`CHROME=...` if needed.

## Browser Fake-Galaxy Demo

This is the main end-to-end proof:

```text
browser WASM fake ~marzod
  -> WebTransport bridge
  -> native fake galaxy ~zod
  -> raw QUIC bound session
  -> native fake ~binzod
```

Run it from the Vere repo:

```sh
cd /home/reid/gits/tlon/vere
KEEP_WORK=1 tools/ames-wt-bridge/demo-browser-fake-galaxy.sh
```

Expected browser-smoke markers:

```text
FINAL_STATUS=ok
initial-sent=1
reply-packets=1
replay-exit=0
browser fake-galaxy demo completed
```

The harness prints its work directory. Useful logs:

```sh
tail -80 /tmp/ames-quic-browser-galaxy.XXXXXX/browser-smoke.log
tail -80 /tmp/ames-quic-browser-galaxy.XXXXXX/bridge.log
tail -120 /tmp/ames-quic-browser-galaxy.XXXXXX/zod.log
```

Default ports:

```text
zod UDP       52337
binzod UDP    52339
zod raw QUIC 19543
bridge WT    18443
asset HTTP   18093
Chrome CDP   19222
```

Set `ZOD_UDP_PORT`, `BINZOD_UDP_PORT`, `ZOD_QUIC_PORT`, `BRIDGE_PORT`,
`HTTP_PORT`, or `CDP_PORT` to avoid conflicts.

## Reusing Prepared Piers

Fresh fake boot can be slow. Prepare clean browser-demo piers once:

```sh
cd /home/reid/gits/tlon/vere
KEEP_WORK=1 PREPARE_ONLY=1 tools/ames-wt-bridge/demo-browser-fake-galaxy.sh
```

The script prints a work directory. Reuse it:

```sh
REUSE_PIERS=1 WORK_DIR=/tmp/ames-quic-browser-galaxy.XXXXXX \
  tools/ames-wt-bridge/demo-browser-fake-galaxy.sh
```

Prepare mode exits after the fake piers are booted and Mesa/QUIC are live,
before any binding or relay traffic. Child piers are prepared without
`--ames-quic-sponsor`; sponsor wiring is added only during the reuse run.
Reuse starts existing fake piers with `-L`, not `-F`.

Do not treat arbitrary kept post-demo work directories as reusable baselines.
After network traffic, the fake galaxy's sponsor peer state may be ahead of a
child checkpoint, and Vere can correctly reject the restart as a double boot.

## Native Fake-Galaxy Demo

This proof excludes the browser and exercises raw QUIC on both child links:

```text
fake ~marzod -> raw QUIC -> fake galaxy ~zod -> raw QUIC -> fake ~binzod
```

Run:

```sh
cd /home/reid/gits/tlon/vere
KEEP_WORK=1 tools/ames-wt-bridge/demo-fake-galaxy.sh
```

The harness disables fake-ship mDNS with `--no-ames-mdns`, forces the children
to use `~zod` as raw-QUIC sponsor with `--ames-quic-sponsor`, drives the Mesa
setup over `+hood/pass`, and asserts that the fake galaxy binds both child
ships to sessions and forwards relay traffic over the destination session.

It supports the same prepared-pier flow:

```sh
KEEP_WORK=1 PREPARE_ONLY=1 tools/ames-wt-bridge/demo-fake-galaxy.sh
REUSE_PIERS=1 WORK_DIR=/tmp/ames-quic-fake-galaxy.XXXXXX \
  tools/ames-wt-bridge/demo-fake-galaxy.sh
```

## Manual Bridge and Browser Pages

Run the WebTransport bridge manually:

```sh
cd /home/reid/gits/tlon/vere/tools/ames-wt-bridge
go run ./cmd/bridge \
  -listen 127.0.0.1:8443 \
  -ames 127.0.0.1:31337 \
  -dev
```

`-dev` prints the SHA-256 of the DER certificate in base64. Paste that hash
into browser pages that use `serverCertificateHashes`.

Serve the parent workspace so the browser can fetch both sibling repos:

```sh
cd /home/reid/gits/tlon/vere
python3 -m http.server 8093 --directory ..
```

Interactive developer page:

```text
http://localhost:8093/vere/tools/ames-wt-bridge/web/index.html
```

Browser WASM terminal and hosted Eyre HTTP page:

```text
http://localhost:8093/vere/tools/ames-wt-bridge/web/wasm-webterm.html
```

For the local terminal/HTTP page, build `vere-disk-wasm`, serve the parent
workspace as above, open `wasm-webterm.html`, leave the memory fields at
`loom=29`, `overhead MiB=512`, `maximum MiB=1536`, then click
`Start Terminal`. That boots the browser `vere-disk-wasm` runtime from the
brass pill, starts Dill `/term/1`, and exposes the Eyre request panel. The
bridge is not required for local terminal or Eyre HTTP requests. Run the bridge,
paste its dev certificate hash, and click `Connect Ames` only when testing
WebTransport packet routing.

Auto-running browser route smoke page:

```text
http://localhost:8093/vere/tools/ames-wt-bridge/web/wasm-vere-disk-route-smoke.html?url=https://127.0.0.1:8443/~_~/ames&hash=<bridge-dev-cert-sha256>&fake-ship=0x100&peer=0x200
```

The shell harness starts the bridge, static asset server, Chrome, and CDP
waiter automatically. Manual page runs need those pieces running separately.

## Browser UI and HTTP

`web/index.html` is a developer control page for WebTransport, Mesa pact
packets, comet/proof helpers, `%chum` helpers, and WASM smoke buttons. It is
still the low-level transport workbench.

`web/wasm-webterm.html` starts the resident `vere-disk-wasm` runtime in a
browser worker, hosts a Dill terminal on `/term/1`, and sends native terminal
ova:

- `%d /term/1 %born`
- `%d /term/1 %blew [cols rows]`
- `%d /term/1 %hail`
- `%d /term/1 %belt [%txt ...]` and `%ret`

Terminal output is decoded from `%blit` gifts and rendered in an xterm-style
browser pane. The first renderer is intentionally small: text, clears,
newlines, line clears, bells, URLs, and nested `%mor` blits are handled; `%klr`
styled text is flattened to text.

The browser WASM runtime has outbound HTTP support through a hosted
`%http-client` adapter. The JS host commits `%http-client %born`, decodes
`%request` and `%cancel-request` effects, performs browser `fetch()`, and
injects `%receive` ova back into the resident runtime.

The browser WASM runtime also hosts inbound Eyre/http-server requests through
JS. A browser tab cannot bind a real TCP port, so this is exposed as worker
commands and page controls instead of `localhost:8080`: the JS host commits
`%http-server %born`, `%live`, `%request-local`/`%request`, and
`%cancel-request` ova, then resolves JS `Response`-shaped objects from Eyre
`%response` gifts. `wasm-webterm.html` includes a small Eyre HTTP request
panel for paths such as `/~/name`.

The browser runtime uses Vere's normal loom exponent semantics. The page
defaults to `--loom 29`, which reserves a 512MiB loom; the runtime avoids
touching every fresh wasm page at startup, so this does not imply all 512MiB are
actively used after boot.

## Tests

From the Vere repo:

```sh
bash -n tools/ames-wt-bridge/demo-browser-fake-galaxy.sh
bash -n tools/ames-wt-bridge/demo-fake-galaxy.sh
(cd tools/ames-wt-bridge && go test ./...)
node --test tools/ames-wt-bridge/web/*.test.mjs
git diff --check
```

Focused browser-runtime checks:

```sh
zig build -Doptimize=ReleaseFast mars-boot-wasm vere-disk-wasm noun-boot-lite-wasm
node --test \
  tools/ames-wt-bridge/web/vere-wasm-host.test.mjs \
  tools/ames-wt-bridge/web/ames-wasm-runtime-service.test.mjs \
  tools/ames-wt-bridge/web/ames-wasm-runtime-worker.test.mjs \
  tools/ames-wt-bridge/web/ames-wasm-runtime-route-demo.test.mjs \
  tools/ames-wt-bridge/web/ames-wasm-terminal.test.mjs \
  tools/ames-wt-bridge/web/ames-wasm-http-server.test.mjs
```

From the Urbit repo:

```sh
git diff --check
```

## Troubleshooting

`CERTIFICATE_VERIFY_FAILED` in Chrome usually means the pinned hash is wrong
or the certificate validity is longer than the browser permits. The hash must
be the SHA-256 of the DER certificate, not the SPKI hash. Verify with:

```sh
cd /home/reid/gits/tlon/vere/tools/ames-wt-bridge
go run ./cmd/pincheck -hash <base64-cert-sha256>
```

`request origin not allowed` means a WebTransport endpoint is enforcing a
same-origin policy. The local bridge intentionally accepts all origins because
Ames packets are end-to-end authenticated.

If a reuse run fails before readiness, inspect the per-ship log tails the
harness prints. Reuse removes stale `.vere.lock` files whose PIDs are gone,
but it does not bypass real double-boot protection.
