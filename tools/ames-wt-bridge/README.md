# ames-wt-bridge

Phase-0 of ames-over-QUIC (`doc/spec/ames-over-quic.md` in the urbit repo):
a transparent WebTransport↔UDP bridge, requiring no changes to vere or arvo.

Three binaries:

- **bridge** — galaxy side. Serves WebTransport (HTTP/3) and gives each
  session its own UDP socket to the ship's ames port, so the ship sees each
  remote peer as a distinct local lane and the UDP 4-tuple provides the
  return path. No ship-level state; trustless for content (ames packets are
  end-to-end authenticated).
- **dial** — client side. Binds a local UDP port and tunnels ames packets to
  a bridge. Stands in for a browser peer until the wasm runtime exists.
- **rawdial** — client side for native Vere. Binds a local UDP port and
  tunnels ames packets to a raw QUIC `ames/1` endpoint, without HTTP/3 or
  WebTransport.

Packet mapping (spec §4.1): one ames packet = one QUIC datagram when it
fits; oversized packets fall back to one ephemeral unidirectional stream
each, preserving out-of-order delivery between packets.

## usage

```
bridge -listen :8443 -ames 127.0.0.1:31337 [-cert crt -key key | -dev]
dial   -udp 127.0.0.1:31337 -url https://host:8443/~_~/ames [-insecure]
rawdial -udp 127.0.0.1:31337 -addr host:8443 [-verify]
```

`-dev` mints a self-signed ECDSA cert (≤14 days) and prints its SHA-256
(of the DER certificate — what the browser `serverCertificateHashes` API
pins; note this is *not* the SPKI hash).

## local end-to-end demo

Run real ames between two fake ships with the QUIC hop interposed on
~zod's czar port:

```
# ~zod with ames on a non-czar port, behind the bridge
urbit -F zod -B brass.pill -l -p 41337 -A pkg/arvo -c ./fzod
bridge -listen 127.0.0.1:8443 -ames 127.0.0.1:41337 -dev

# the dialer occupies ~zod's czar port, where other fakes look for it
dial -udp 127.0.0.1:31337 -url https://127.0.0.1:8443/~_~/ames -insecure

# ~bud, unmodified; its packets to ~zod can only travel via the tunnel
urbit -F bud -B brass.pill -l -A pkg/arvo -c ./fbud
```

Then in ~bud's dojo: `|hi ~zod` — the request and ~zod's ack both traverse
UDP → WebTransport/QUIC → UDP.

## native raw-QUIC smoke

Run czar-port interposition against Vere's native raw QUIC listener:

```
# ~zod with UDP ames off the czar port and raw QUIC on 8443
urbit -F zod -B brass.pill -l -p 41337 --ames-quic-port 8443 -A pkg/arvo -c ./fzod

# the dialer occupies ~zod's czar UDP port, then tunnels to raw QUIC ames/1
rawdial -udp 127.0.0.1:31337 -addr 127.0.0.1:8443
```

Feed Mesa packets into rawdial's UDP socket from a Mesa-capable peer or test
fixture. Phase-1 raw QUIC session ingress is Mesa-only; legacy old-Ames UDP
traffic remains supported on Vere's UDP socket and by the WebTransport bridge,
but non-Mesa packets received on a raw QUIC session are dropped.

For raw QUIC, TLS certificate verification is off by default because peer
identity is authenticated by ames itself; pass `-verify` only when testing a
WebPKI or locally trusted certificate.

For the native sponsor-relay topology, `demo-fake-galaxy.sh` boots a fresh
fake `~zod` plus two fake child ships, forces the children to use `~zod`'s
raw-QUIC sponsor port, and drives the Mesa migration/route-clearing sequence
over `%lens`/`+hood/pass`. This is a local native smoke harness only: fake
ships also register over Vere's fake/mdns loopback path, so this does not
stand in for the browser/WASM topology. The browser demo still needs an
explicit fake-galaxy relay path and a real browser runtime.

## wasm compile gate

`zig build mesa-session-wasm` compiles and links the portable Mesa session
table as `wasm32-wasi` (`zig-out/bin/mesa-session-wasm.wasm`).
`zig build noun-loom-wasm` compiles the first noun memory boundary as
`wasm32-wasi` (`zig-out/bin/noun-loom-wasm.wasm`): native Vere keeps fixed
`mmap`, while `U3_OS_wasm` defaults to `--loom 28` (256MB), rejects
exponents above `31`, and uses a dynamic aligned loom base allocated from
linear memory. The linked WASM probes now import and re-export `env.memory`
with a 16MB declared minimum and the wasm32 maximum, so the browser host owns
the bounded `WebAssembly.Memory`. `web/vere-wasm-memory.mjs` is the browser-side
planner: the default `--loom 28` plan instantiates 320MB up front (256MB loom
plus 64MB heap/stack/host headroom) with a 512MB host maximum; native-default
`--loom 31` is rejected unless the host explicitly chooses a much larger
host maximum. `zig build noun-hostfs-wasm` compiles the first noun host-file
boundary as `wasm32-wasi` (`zig-out/bin/noun-hostfs-wasm.wasm`): native Vere
keeps `open/fstat/mmap/msync/munmap`, while `U3_OS_wasm` calls JS-provided
`u3_wasm_file_size`, `u3_wasm_file_read`, `u3_wasm_file_write`, and
random-access file imports. `u3m_file()` uses this same boundary for
boot-time blob reads, with native coverage in `zig build file-test`.
`events.c` also uses hostfs for snapshot image/patch file persistence;
`zig build events-test` covers the native `u3e_backup()` path. All three
WASM probes run under Node's WASI host and exit 0, with `noun-hostfs-wasm`
also exercising random-access create, write-at, read-at, resize, sync, close,
exists, and unlink. `web/vere-wasm-host.mjs` now has explicit in-memory and
IndexedDB-backed file stores: the browser loads a synchronous in-memory
hostfs mirror before `_start()` and flushes it back after the run.
`web/wasm-hostfs-smoke.html` verifies that path in Chrome by writing `/out`
through the WASM hostfs imports and persisting it to IndexedDB.
`zig build noun-events-wasm` compile-checks `events.c` as a `wasm32-wasi`
object with a no-demand/copying snapshot path and the WASI rsignal shim.
`zig build noun-manage-wasm` compile-checks `manage.c`, including the
`u3m_boot_lite()` boundary, as a WASI static library using dependency header
trees. `zig build noun-boot-lite-wasm` is the stronger linked gate:
it builds `zig-out/bin/noun-boot-lite-wasm.wasm` from the full `noun` static
library, starts it with `web/vere-wasm-host.mjs`, calls `u3m_boot_lite()` with
a 16MB loom, constructs/jams/cues nouns, shuts down, and exits 0. The same JS
host runs under Node and in Chrome; `web/wasm-smoke.html` is an auto-running
browser smoke page that fetches the built artifact from `zig-out`.
`zig build noun-ivory-boot-wasm` goes one step further: it links the full noun
library with the embedded Ivory pill, starts a 64MB loom, cues the pill,
runs `u3v_boot_lite()`, and checks a simple kernel parse path. Under the JS
host this needs a larger browser memory plan: 320MB initial, 512MB maximum.
`web/wasm-ivory-smoke.html` verifies the same artifact in Chrome. The wasm
build uses a 32-bit generic GMP configuration with checked-in generated tables
and generic low-level `mpn` primitives, omits the native `%lia` `wasm3` jet
registration, makes POSIX signal profiling and shared-memory slow-stack
support inert, enables WASI setjmp lowering for the non-bailing boot path, and
keeps native builds unchanged.

These are compile/runtime gates for narrow pieces only. They do not boot Vere,
provide browser WebTransport I/O, or provide crash-safe pier/event-log
storage. The JS host is a smoke runner with planned `WebAssembly.Memory`,
hostfs imports backed by memory or IndexedDB, a small WASI-preview1 subset,
and fail-fast longjmp stubs; full browser Vere still needs the rest of Vere
I/O hosted by the browser.

## browser leg

`web/index.html` is the browser counterpart of `dial`: it opens a
WebTransport session straight from a tab and sends an ames packet — the
exact transport path an in-browser (wasm) ship will use, with no UDP. The
page generates its default mesa `%peek` with `web/mesa-pact.mjs`; the old
captured-hex replay remains available as a manual override. Browser
sessions are managed by `web/ames-client.mjs`, which uses
`web/ames-transport.mjs` for the datagram-or-one-shot-stream send rule and
normalizes inbound datagrams/streams to packet callbacks. `web/ames-identity.mjs`
can generate a browser-held suite-B comet identity; the page's `new comet`
button fills the source-ship field with that derived comet atom.
`web/ames-proof.mjs` can build the comet open-packet self-attestation and wrap
it in a one-fragment signed Mesa `%page`. `web/ames-comet-peer.mjs` watches
inbound pact packets and automatically answers matching comet proof `%peek`s;
the page's `dial + send comet proof` button remains available for manual
debugging. The page can also request a peer's comet proof, verify the returned
proof page, learn the peer pass/life, and derive the `%chum` key. The default
browser comet identity is persisted in IndexedDB and restored on page load.
The browser crypto foundation now also includes Hoon-compatible Mesa
BLAKE3-KDF + XChaCha8/ChaCha8 `crypt`, `seal-path`, `encrypt`, binding
`%hmac`, old-Ames AES-256-SIV primitives, Ames shared-key derivation from
suite-B ring/pass material, and one-fragment encrypted `%chum` `%poke`
assembly with authenticated/decrypted inbound `%poke` opening. The page
exposes manual `%chum` controls for learned comet peer key derivation, peer
suite-B pass based key derivation, raw symmetric-key override, peer
ship/life/rift, flow bone/sequence, encrypted `%poke` send, and matching
inbound `%poke` open. It does not yet discover non-comet peer keys/lifes
automatically or drive a real Ames request/response lifecycle.

Serve the repo root and open it in **Chrome** (or Chromium/Edge — a browser
that honors the WebTransport `serverCertificateHashes` API):

```
zig build noun-boot-lite-wasm noun-ivory-boot-wasm noun-hostfs-wasm
python3 -m http.server 8092 --directory .
# start the bridge with -dev and copy its printed cert sha-256
# open http://localhost:8092/tools/ames-wt-bridge/web/index.html
```

The `run WASM boot-lite` button runs the linked browser smoke against
`zig-out/bin/noun-boot-lite-wasm.wasm`. The `run WASM ivory boot` button boots
the linked Ivory pill with a 64MB loom. The `run WASM hostfs` button runs
`noun-hostfs-wasm` with an IndexedDB-backed store and verifies that `/out`
persists as `quic`. For auto-running smoke pages, open
`http://localhost:8092/tools/ames-wt-bridge/web/wasm-smoke.html`,
`http://localhost:8092/tools/ames-wt-bridge/web/wasm-ivory-smoke.html`, or
`http://localhost:8092/tools/ames-wt-bridge/web/wasm-hostfs-smoke.html`;
success is `data-status="ok"` with `exit=0`.

`serverCertificateHashes` lets the tab trust the bridge's self-signed dev
cert by pinning the SHA-256 of the DER certificate — no CA, no public
domain. Requirements the
bridge already meets: ECDSA P-256 key, total validity ≤ 14 days. Verify a
hash independently with `cmd/pincheck` (dials using the same cert-hash
pin the browser uses):

```
pincheck -hash <base64-cert-sha256>
```

The browser-side pact codec, packet transport helper, WebTransport client
shell, WebCrypto/suite-B identity helper, noun jam/cue helper, `@p` renderer,
BLAKE3/LSS/KDF helper, Mesa crypto helper, AES-SIV helper, and comet proof-page
builder have Node tests:

```
node --test web/*.test.mjs
```

A `CERTIFICATE_VERIFY_FAILED` from the browser almost always means the
hash is wrong (it must be the DER-certificate hash, not the SPKI hash) or
the cert exceeds 14 days validity. A `request origin not allowed` upgrade
error means a cross-origin page dialed a bridge without the open
`CheckOrigin` (the bridge allows all origins: ames packets are end-to-end
authenticated, so origin CSRF checks add nothing).
