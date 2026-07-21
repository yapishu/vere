import assert from 'node:assert/strict';
import test from 'node:test';

import {
  AmesWasmRuntimeService,
  publicRuntimeSnapshot,
} from './ames-wasm-runtime-service.mjs';
import { mesaSessionLane } from './ames-wasm-events.mjs';
import {
  BrowserHttpClientHost,
  httpClientWire,
  httpRequestNoun,
} from './ames-wasm-http-client.mjs';
import {
  atomFromBytesLE,
  jamBytes,
  list,
  termAtom,
  tuple,
} from './urbit-noun.mjs';

function packetAtom(bytes = [1, 2, 3]) {
  return atomFromBytesLE(Uint8Array.from(bytes));
}

function emptyEffects() {
  return jamBytes(0n);
}

function pushEffects(lane = 0n, bytes = [1, 2, 3]) {
  return jamBytes(list([
    [termAtom('ames'), 0n],
    tuple(termAtom('give'), tuple(termAtom('push'), list(lane), packetAtom(bytes))),
  ]));
}

function httpRequestEffects({
  id = 42n,
  url = 'https://example.invalid/data',
} = {}) {
  return jamBytes(list(
    tuple(
      httpClientWire('0v1n.2m9vh'),
      tuple(
        termAtom('request'),
        id,
        httpRequestNoun({
          method: 'GET',
          url,
          headers: [['accept', 'text/plain']],
        }),
      ),
    ),
  ));
}

function makeRuntimeFactory({
  mateEffects = pushEffects(0n),
  keenEffects = pushEffects(0n),
  replyEffects = emptyEffects(),
} = {}) {
  const runtimes = [];

  const factory = async (_wasmUrl, options) => {
    const runtime = {
      options,
      host: { files: new Map() },
      currentEvent: 20n,
      initialized: false,
      saved: false,
      shutDown: false,
      init({ loomExponent, ship }) {
        this.initialized = true;
        this.loomExponent = loomExponent;
        this.ship = ship;
      },
      event() {
        return this.currentEvent;
      },
      pokeLoadMesa({ effectsPath }) {
        this.currentEvent++;
        this.host.files.set(effectsPath, emptyEffects());
      },
      pokeOvum({ ovumPath, effectsPath }) {
        this.currentEvent++;
        if (ovumPath.includes('mate')) {
          this.host.files.set(effectsPath, mateEffects);
        }
        else if (ovumPath.includes('keen')) {
          this.host.files.set(effectsPath, keenEffects);
        }
        else if (ovumPath.includes('reply')) {
          this.host.files.set(effectsPath, replyEffects);
        }
        else {
          this.host.files.set(effectsPath, emptyEffects());
        }
      },
      async save() {
        this.saved = true;
      },
      shutdown() {
        this.shutDown = true;
      },
    };
    runtimes.push(runtime);
    return runtime;
  };

  factory.runtimes = runtimes;
  return factory;
}

function makeClientFactory({ echoReplies = false } = {}) {
  const clients = [];
  const sent = [];

  const factory = ({ onPacket, onStatus }) => {
    const client = {
      closed: false,
      sessionOpen: false,
      async connect() {
        this.sessionOpen = true;
        onStatus({ type: 'open', url: 'https://bridge/~_~/ames' });
      },
      async send(packet) {
        sent.push(Uint8Array.from(packet));
        if (echoReplies) {
          onPacket({ mode: 'datagram', packet: Uint8Array.from([9, 9]) });
        }
        return 'datagram';
      },
      async close() {
        this.closed = true;
        this.sessionOpen = false;
      },
    };
    clients.push(client);
    return client;
  };

  factory.clients = clients;
  factory.sent = sent;
  return factory;
}

test('AmesWasmRuntimeService starts a resident runtime and routes pokes over WebTransport', async () => {
  const logs = [];
  const runtimeFactory = makeRuntimeFactory();
  const clientFactory = makeClientFactory({ echoReplies: true });
  const service = new AmesWasmRuntimeService({
    wasmUrl: 'vere-disk-wasm.wasm',
    pillBytes: Uint8Array.from([1]),
    fileStore: {},
    bridgeUrl: 'https://bridge/~_~/ames',
    fakeShip: 0x100n,
    runtimeFactory,
    clientFactory,
    onLog: event => logs.push(event.message),
  });

  const started = await service.start();
  assert.equal(started.snapshot.started, true);
  assert.equal(started.httpBorn.event, 22n);
  assert.equal(started.born.event, 23n);
  assert.equal(runtimeFactory.runtimes[0].initialized, true);
  assert.equal(runtimeFactory.runtimes[0].ship, 0x100n);
  assert.ok([...runtimeFactory.runtimes[0].host.files.keys()].some(
    path => path.includes('http-client-born'),
  ));

  await service.connect();
  const mate = await service.mate({ ship: 0x200n });
  const keen = await service.keen({ ship: 0x200n, path: '/c/x/1/kids/sys/kelvin' });

  assert.equal(mate.routes.sent, 1);
  assert.equal(keen.routes.sent, 1);
  assert.equal(clientFactory.sent.length, 2);
  assert.ok(logs.includes('WebTransport session open'));
  assert.ok(logs.some(line => line.includes('keen-effects: tx datagram')));

  const looped = await service.runInputLoop({
    maxRounds: 1,
    firstIdleTimeoutMs: 0,
    idleTimeoutMs: 0,
  });

  assert.equal(looped.loop.packets, 2);
  assert.equal(looped.loop.stoppedBy, 'round-cap');
  assert.equal(service.snapshot().totals.inboundPackets, 2);

  await service.save();
  await service.shutdown();
  assert.equal(runtimeFactory.runtimes[0].saved, true);
  assert.equal(runtimeFactory.runtimes[0].shutDown, true);
  assert.equal(clientFactory.clients[0].closed, true);
});

test('AmesWasmRuntimeService can inject a packet directly over a session lane', async () => {
  const runtimeFactory = makeRuntimeFactory({
    replyEffects: pushEffects(mesaSessionLane(1n), [7, 8]),
  });
  const clientFactory = makeClientFactory();
  const service = new AmesWasmRuntimeService({
    wasmUrl: 'vere-disk-wasm.wasm',
    pillBytes: Uint8Array.from([1]),
    fileStore: {},
    runtimeFactory,
    clientFactory,
  });

  await service.start();
  await service.connect();
  const injected = await service.injectPacket({
    packet: Uint8Array.from([9]),
  });

  assert.equal(injected.effects.pushes, 1);
  assert.equal(injected.routes.sent, 1);
  assert.deepEqual([...clientFactory.sent[0]], [7, 8]);
});

test('AmesWasmRuntimeService hosts %http-client requests through browser fetch', async () => {
  const runtimeFactory = makeRuntimeFactory({
    keenEffects: httpRequestEffects(),
  });
  const httpClientHost = new BrowserHttpClientHost({
    fetchFn: async (url, init) => {
      assert.equal(url, 'https://example.invalid/data');
      assert.equal(init.method, 'GET');
      assert.equal(init.headers.accept, 'text/plain');
      return {
        status: 200,
        headers: new Map([['content-type', 'text/plain']]),
        async arrayBuffer() {
          return new TextEncoder().encode('ok').buffer;
        },
      };
    },
  });
  const service = new AmesWasmRuntimeService({
    wasmUrl: 'vere-disk-wasm.wasm',
    pillBytes: Uint8Array.from([1]),
    fileStore: {},
    runtimeFactory,
    clientFactory: makeClientFactory(),
    httpClientHost,
  });

  await service.start();
  const keen = await service.keen({ ship: 0x200n, path: '/c/x/1/kids/sys/kelvin' });
  await httpClientHost.waitAll();

  assert.equal(keen.routes.sent, 0);
  assert.equal(keen.routes.http.requests, 1);
  assert.equal(service.snapshot().totals.httpRequests, 1);
  assert.ok([...runtimeFactory.runtimes[0].host.files.keys()].some(
    path => path.includes('http-receive-42'),
  ));
});

test('AmesWasmRuntimeService can run and stop a background input loop', async () => {
  const runtimeFactory = makeRuntimeFactory();
  const service = new AmesWasmRuntimeService({
    wasmUrl: 'vere-disk-wasm.wasm',
    pillBytes: Uint8Array.from([1]),
    fileStore: {},
    runtimeFactory,
    clientFactory: makeClientFactory(),
  });

  await service.start();
  const started = service.startInputLoop({
    firstIdleTimeoutMs: 10000,
    idleTimeoutMs: 10000,
  });

  assert.equal(started.started, true);
  assert.equal(started.snapshot.inputLoopActive, true);
  assert.equal(service.snapshot().inputLoopActive, true);
  await assert.rejects(
    service.runInputLoop({ firstIdleTimeoutMs: 0 }),
    /runtime input loop is already running/,
  );

  const stopped = await service.stopInputLoop();
  assert.equal(stopped.stopped, true);
  assert.equal(stopped.result.loop.stoppedBy, 'aborted');
  assert.equal(stopped.snapshot.inputLoopActive, false);
  assert.equal(service.snapshot().inputLoopActive, false);

  const stoppedAgain = await service.stopInputLoop();
  assert.equal(stoppedAgain.stopped, false);
});

test('AmesWasmRuntimeService saves, shuts down, and replays a persisted pier', async () => {
  const runtimeFactory = makeRuntimeFactory();
  let replayInput = null;
  const service = new AmesWasmRuntimeService({
    wasmUrl: 'vere-disk-wasm.wasm',
    pillBytes: Uint8Array.from([1, 2, 3]),
    fileStore: { name: 'store' },
    runtimeFactory,
    replayProbe: async (wasmUrl, input) => {
      replayInput = { wasmUrl, input };
      return {
        exitCode: 0,
        plan: {
          initialBytes: 1024,
          maximumBytes: 2048,
        },
      };
    },
  });

  await service.start();
  const replayed = await service.replay();

  assert.equal(runtimeFactory.runtimes[0].saved, true);
  assert.equal(runtimeFactory.runtimes[0].shutDown, true);
  assert.equal(replayInput.wasmUrl, 'vere-disk-wasm.wasm');
  assert.equal(replayInput.input.fileStore.name, 'store');
  assert.deepEqual([...replayInput.input.initialFiles['/brass.pill']], [1, 2, 3]);
  assert.deepEqual(replayInput.input.args, [
    '--loom',
    '29',
    '--fake-ship',
    '256',
    '--load-only',
    '--run-boot',
  ]);
  assert.equal(replayed.replay.exitCode, 0);
  assert.equal(replayed.replay.initialBytes, 1024);
  assert.equal(replayed.replay.maximumBytes, 2048);
});

test('publicRuntimeSnapshot serializes bigint fields', () => {
  assert.deepEqual(publicRuntimeSnapshot({
    started: true,
    connected: false,
    event: 25n,
    fakeShip: 0x100n,
    sessionId: 1n,
    inputLoopActive: true,
    queue: { accepted: 0 },
    hostedHttp: { pending: 0 },
    totals: { sent: 0 },
  }), {
    started: true,
    connected: false,
    event: '25',
    fakeShip: '256',
    sessionId: '1',
    inputLoopActive: true,
    queue: { accepted: 0 },
    hostedHttp: { pending: 0 },
    totals: { sent: 0 },
  });
});
