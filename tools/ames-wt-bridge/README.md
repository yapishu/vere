# Ames WebTransport UDP gateway

This directory lets a browser/WASM Vere participate in the ordinary Ames
network without adding WebTransport or QUIC support to Arvo, galaxies, or
native Vere.

```text
browser Vere
  Ames packet + destination lane
           |
      WebTransport
           |
  gateway-owned UDP socket
           |
   ordinary UDP Ames network
```

Every WebTransport session gets its own UDP socket and therefore its own
source port. One browser ship uses one port. Many ships can use one gateway
concurrently as long as the configured UDP range contains enough ports.
Peers see an ordinary UDP Ames endpoint and need no upgrade.

## Build and test

From the Vere repository:

```sh
zig build vere-disk-wasm
(cd tools/ames-wt-bridge && go test ./...)
node --test tools/ames-wt-bridge/web/*.test.mjs
```

The Go module builds:

- `bridge`: WebTransport-to-UDP gateway.
- `pincheck`: development-certificate helper.

An Ames packet normally occupies one WebTransport datagram. If it does not
fit, it uses one finite unidirectional stream. The small `AUDP` envelope
carries the destination lane toward the gateway and the actual UDP source
lane toward the browser.

## Run a gateway

For local development:

```sh
cd tools/ames-wt-bridge
go run ./cmd/bridge \
  -listen 127.0.0.1:8443 \
  -udp-ip 127.0.0.1 \
  -dev \
  -token local-secret
```

`-dev` prints the base64 SHA-256 certificate hash. Put that value in the
browser demo's development-certificate field and use:

```text
https://127.0.0.1:8443/~_~/ames?token=local-secret
```

For a public gateway, use a WebPKI certificate and an externally reachable
UDP port range:

```sh
ames-wt-bridge \
  -listen :443 \
  -cert /path/fullchain.pem \
  -key /path/privkey.pem \
  -udp-ip 0.0.0.0 \
  -udp-port-min 20000 \
  -udp-port-max 20255 \
  -token 'long-random-secret'
```

Allow the WebTransport/HTTP3 UDP port and the configured Ames UDP range
through the host and cloud firewalls. The gateway binds the first available
port for each session and releases it when that session closes. With
`-udp-port-min 0 -udp-port-max 0`, the OS chooses ephemeral ports instead.
Add `-log-packets` while debugging to log compact WebTransport-to-UDP and
UDP-to-WebTransport packet routes without dumping packet contents.

Do not expose a tokenless gateway publicly. Without `-token`, anyone who can
reach it can use it as a UDP relay. A production service should additionally
put per-user authorization, connection limits, packet rate limits, and
observability around the gateway.

Galaxy lanes are resolved as normal `${galaxy}.urbit.org` Ames addresses.
Use `-ames-domain` and `-galaxy-base-port` only for a private/test network.

## Browser Vere demo

Build the WASM artifact, then use the included server. This server is
required for owned-ship boot because browser CORS prevents direct access to
some public boot endpoints:

```sh
zig build vere-disk-wasm
node tools/ames-wt-bridge/serve-wasm-demo.mjs \
  --host 0.0.0.0 \
  --port 8093
```

Open:

```text
http://localhost:8093/vere/tools/ames-wt-bridge/web/wasm-webterm.html
```

In Options:

1. Choose **Fake ship** for development, or **Owned ship from keyfile**.
2. Enter the ship as `~patp`, decimal, or `0x...`.
3. For an owned ship, select its keyfile.
4. Choose a stable persistent-pier slot.
5. Optionally enter the WebTransport gateway URL and development certificate
   hash.
6. Click **Boot**.

The keyfile is placed only in the worker's in-memory boot filesystem. It is
used by Vere to construct and validate the `%dawn` boot event and is never
sent to the gateway or Roller. The host fetches public Azimuth point,
sponsor, galaxy, and DNS data through `POST /_vere/http-client`.

The pier filesystem is checkpointed to IndexedDB. Reloading the page and
booting the same owned ship and slot resumes it without the keyfile or a new
public dawn fetch. Select **Erase this pier slot before boot** only when you
intentionally want a fresh pier. Keyfiles are never included in the
checkpoint; select one only when a fresh owned boot needs it.

When a gateway URL is present, the demo opens WebTransport and starts the
resident input loop automatically. Without one, Dojo, hosted Iris HTTP, and
the browser-hosted Eyre view still work locally.

## Browser hosting model

`web/wasm-webterm.html` runs `vere-disk-wasm` in a worker and hosts:

- Dill terminal input/output;
- Behn timers;
- `%http-client` through the same-origin server proxy;
- inbound `%http-server` requests used by the embedded Eyre view;
- Ames UDP effects through the WebTransport gateway.

`web/index.html` remains a low-level transport and Mesa development
workbench.

The bridge is only a network interface. Ames packet authentication,
encryption, routing, retransmission, and peer discovery remain in the normal
Urbit stack.
