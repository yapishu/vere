import assert from 'node:assert/strict';
import test from 'node:test';

import {
  createAmesRuntimeWorkerHandler,
  prepareOwnedBoot,
} from './ames-wasm-runtime-worker.mjs';

function okFetch(bytes = [1, 2, 3]) {
  return async () => ({
    ok: true,
    status: 200,
    async arrayBuffer() {
      return Uint8Array.from(bytes).buffer;
    },
  });
}

function recordingFetch(bytes = [1, 2, 3]) {
  const calls = [];
  const fn = async (resource, init = {}) => {
    const url = String(resource?.url ?? resource);
    calls.push({ url, init });
    return {
      ok: true,
      status: 200,
      async arrayBuffer() {
        return Uint8Array.from(bytes).buffer;
      },
    };
  };
  fn.calls = calls;
  return fn;
}

class FakeStore {
  static instances = [];

  constructor(options) {
    this.options = options;
    this.cleared = false;
    FakeStore.instances.push(this);
  }

  async clear() {
    this.cleared = true;
  }

  async load() {
    return { files: new Map(), directories: new Set(['/']) };
  }
}

class FakeService {
  static instances = [];

  constructor(options) {
    this.options = options;
    this.closed = false;
    this.loopPromise = null;
    this.resolveLoop = null;
    FakeService.instances.push(this);
    const encoder = new TextEncoder();
    options.onLog({ message: 'service-log', className: 'rx' });
    options.onStdout(encoder.encode('std'));
    options.onStdout(encoder.encode('out'));
    options.onStdout(encoder.encode('-line\n'));
    options.onStderr(encoder.encode('err'));
    options.onStderr(encoder.encode('or-line\npartial'));
  }

  async start() {
    return { snapshot: { started: true, event: 21n } };
  }

  async connect(input) {
    this.connectInput = input;
    return { snapshot: { connected: true } };
  }

  async mate(input) {
    this.mateInput = input;
    return { event: 22n, routes: { sent: 1 } };
  }

  async keen(input) {
    this.keenInput = input;
    return { event: 23n, routes: { sent: 1 } };
  }

  async injectPacket(input) {
    this.injectInput = input;
    return { event: 24n, effects: { pushes: 0 } };
  }

  async httpRequest(input) {
    this.httpRequestInput = input;
    return {
      status: 200,
      headers: [['content-type', 'text/plain']],
      body: Uint8Array.from([111, 107]),
    };
  }

  async httpClientStart() {
    this.httpClientStarted = true;
    return { event: 25n, routes: { http: { requests: 0 } } };
  }

  async terminalStart(input) {
    this.terminalStartInput = input;
    return { snapshot: { terminal: 'started' } };
  }

  async terminalInput(input) {
    this.terminalInputInput = input;
    return { events: [{ event: 26n }] };
  }

  async terminalData(input) {
    this.terminalDataInput = input;
    return { events: [{ event: 27n }] };
  }

  async runInputLoop(input) {
    this.pumpInput = input;
    return { loop: { packets: 1, stoppedBy: 'idle' } };
  }

  startInputLoop(input) {
    this.pumpStartInput = input;
    this.loopPromise = new Promise(resolve => {
      this.resolveLoop = resolve;
    });
    return {
      started: true,
      snapshot: {
        inputLoopActive: true,
      },
    };
  }

  waitInputLoop() {
    return this.loopPromise ?? {
      running: false,
      snapshot: {
        inputLoopActive: false,
      },
    };
  }

  async stopInputLoop() {
    if (!this.loopPromise) {
      return {
        stopped: false,
        snapshot: {
          inputLoopActive: false,
        },
      };
    }
    const result = {
      loop: {
        packets: 0,
        stoppedBy: 'aborted',
      },
      snapshot: {
        inputLoopActive: true,
      },
    };
    this.resolveLoop(result);
    const looped = await this.loopPromise;
    this.loopPromise = null;
    return {
      stopped: true,
      result: looped,
      snapshot: {
        inputLoopActive: false,
      },
    };
  }

  async save() {
    return { snapshot: { event: 24n } };
  }

  async replay(input) {
    this.replayInput = input;
    return {
      replay: {
        exitCode: 0,
        initialBytes: 1024,
        maximumBytes: 2048,
      },
      snapshot: { event: 25n },
    };
  }

  snapshot() {
    return {
      started: true,
      connected: true,
      event: 24n,
      fakeShip: 0x100n,
      sessionId: 1n,
      inputLoopActive: Boolean(this.loopPromise),
      queue: { accepted: 1 },
      totals: { sent: 2 },
    };
  }

  async shutdown() {
    if (this.loopPromise) {
      await this.stopInputLoop();
    }
    this.closed = true;
    return { snapshot: { started: true, connected: false } };
  }
}

test('prepareOwnedBoot keeps the key local and fetches only public dawn data', async () => {
  const calls = [];
  const fetchFn = async (resource, init = {}) => {
    assert.equal(String(resource), 'https://demo.invalid/_vere/http-client');
    const outer = JSON.parse(init.body);
    assert.equal(outer.url, 'https://roller.urbit.org/v1/azimuth');
    const payload = JSON.parse(
      Buffer.from(outer.bodyBase64, 'base64').toString('utf8'),
    );
    calls.push(payload);
    const body = Array.isArray(payload)
      ? payload.map(request => ({ jsonrpc: '2.0', id: request.id, result: {} }))
      : { jsonrpc: '2.0', id: payload.id, result: {} };
    const bytes = new TextEncoder().encode(JSON.stringify(body));
    return {
      ok: true,
      status: 200,
      async arrayBuffer() {
        return bytes.buffer;
      },
    };
  };
  const keyBytes = new TextEncoder().encode('0w1.test-key');
  const files = await prepareOwnedBoot({
    fetchFn,
    proxyUrl: 'https://demo.invalid/_vere/http-client',
    ship: 0n,
    keyBytes,
  });

  assert.deepEqual(files['/boot/ship.key'], keyBytes);
  assert.ok(files['/boot/point-00000000000000000000000000000000.json']);
  assert.ok(files['/boot/galaxies.json']);
  assert.ok(files['/boot/turf.json']);
  assert.equal(calls.length, 3);
  assert.equal(calls[0].method, 'getPoint');
  assert.equal(calls[0].params.ship, '~zod');
  assert.equal(calls[1].length, 256);
  assert.equal(calls[2].method, 'getDns');
});

test('runtime worker resumes an owned pier without loading a keyfile', async () => {
  class PersistedStore extends FakeStore {
    async load() {
      return {
        files: new Map([
          ['/pier/.urb/log/meta.bin', Uint8Array.of(1)],
          ['/pier/.urb/log/0i0/events.bin', Uint8Array.of(2)],
        ]),
        directories: new Set(['/']),
      };
    }
  }
  FakeService.instances = [];
  const emitted = [];
  const fetchFn = recordingFetch([4, 5]);
  const handler = createAmesRuntimeWorkerHandler({
    emit: message => emitted.push(message),
    Service: FakeService,
    FileStore: PersistedStore,
    fetchFn,
  });

  await handler.handle({
    id: 1,
    type: 'start',
    wasmUrl: 'https://example.invalid/vere-disk-wasm.wasm',
    pillUrl: 'https://example.invalid/brass.pill',
    bootMode: 'owned',
    fakeShip: '0x100',
    scope: 'owned-resume',
  });

  assert.equal(fetchFn.calls.length, 1);
  assert.equal(FakeService.instances[0].options.bootMode, 'owned');
  assert.deepEqual(FakeService.instances[0].options.bootFiles, {});
  assert.ok(emitted.some(message => (
    message.type === 'log' &&
    message.message.includes('keyfile is not required')
  )));
});

test('runtime worker handler runs command lifecycle and serializes results', async () => {
  FakeStore.instances = [];
  FakeService.instances = [];
  const emitted = [];
  const handler = createAmesRuntimeWorkerHandler({
    emit: message => emitted.push(message),
    Service: FakeService,
    FileStore: FakeStore,
    fetchFn: okFetch([4, 5]),
  });

  await handler.handle({
    id: 1,
    type: 'start',
    wasmUrl: 'https://example.invalid/vere-disk-wasm.wasm',
    pillUrl: 'https://example.invalid/brass.pill',
    bridgeUrl: 'https://bridge/~_~/ames',
    fakeShip: '0x100',
    sessionId: '1',
    scope: 'worker-test',
  });
  await handler.handle({ id: 2, type: 'connect', bridgeUrl: 'https://bridge/~_~/ames' });
  await handler.handle({ id: 3, type: 'mate', ship: '0x200' });
  await handler.handle({ id: 4, type: 'keen', ship: '0x200', path: '/c/x/1/kids/sys/kelvin' });
  await handler.handle({ id: 5, type: 'inject-packet', packet: [9, 8, 7] });
  await handler.handle({
    id: 6,
    type: 'http-request',
    method: 'POST',
    url: '/~/name',
    headers: [['content-type', 'text/plain']],
    body: [104, 105],
  });
  await handler.handle({ id: 7, type: 'http-client-start' });
  await handler.handle({ id: 8, type: 'terminal-start', cols: 100, rows: 30 });
  await handler.handle({ id: 9, type: 'terminal-input', text: '+trouble' });
  await handler.handle({ id: 10, type: 'terminal-data', data: '+code\r' });
  await handler.handle({ id: 11, type: 'pump', maxRounds: 1, firstIdleTimeoutMs: 0 });
  await handler.handle({ id: 12, type: 'pump-start', firstIdleTimeoutMs: 1000 });
  await handler.handle({ id: 13, type: 'pump-stop' });
  await handler.handle({ id: 14, type: 'snapshot' });
  await handler.handle({ id: 15, type: 'save' });
  await handler.handle({ id: 16, type: 'replay', shutdownRuntime: false });
  await handler.handle({ id: 17, type: 'shutdown' });

  assert.equal(FakeStore.instances.length, 1);
  assert.equal(FakeStore.instances[0].options.scope, 'worker-test');
  assert.equal(FakeStore.instances[0].cleared, false);
  assert.equal(FakeService.instances.length, 1);
  assert.deepEqual([...FakeService.instances[0].options.pillBytes], [4, 5]);
  assert.equal(FakeService.instances[0].options.fakeShip, 0x100n);
  assert.equal(FakeService.instances[0].mateInput.ship, 0x200n);
  assert.deepEqual([...FakeService.instances[0].injectInput.packet], [9, 8, 7]);
  assert.equal(FakeService.instances[0].httpRequestInput.method, 'POST');
  assert.equal(FakeService.instances[0].httpRequestInput.url, '/~/name');
  assert.equal(FakeService.instances[0].httpRequestInput.local, false);
  assert.deepEqual([...FakeService.instances[0].httpRequestInput.body], [104, 105]);
  assert.equal(FakeService.instances[0].httpClientStarted, true);
  assert.equal(FakeService.instances[0].terminalStartInput.cols, 100);
  assert.equal(FakeService.instances[0].terminalStartInput.rows, 30);
  assert.equal(FakeService.instances[0].terminalInputInput.text, '+trouble');
  assert.equal(FakeService.instances[0].terminalDataInput.data, '+code\r');
  assert.equal(FakeService.instances[0].pumpStartInput.firstIdleTimeoutMs, 1000);
  assert.equal(FakeService.instances[0].replayInput.shutdownRuntime, false);

  assert.ok(emitted.some(message => (
    message.type === 'log' && message.message === 'service-log'
  )));
  assert.ok(emitted.some(message => (
    message.type === 'log' && message.message === 'wasm: stdout-line'
  )));
  assert.equal(
    emitted.filter(message => message.message === 'wasm: stdout-line').length,
    1,
  );
  assert.ok(emitted.some(message => (
    message.type === 'log' &&
    message.message === 'wasm: error-line' &&
    message.className === 'err'
  )));
  assert.ok(emitted.some(message => (
    message.type === 'log' &&
    message.message === 'wasm: partial' &&
    message.className === 'err'
  )));
  assert.ok(emitted.some(message => (
    message.type === 'log' && message.message === 'pill-bytes=2'
  )));
  assert.deepEqual(
    emitted.filter(message => message.type === 'result').map(message => message.id),
    [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17],
  );
  assert.equal(
    emitted.find(message => message.id === 6).result.body.join(','),
    '111,107',
  );
  assert.equal(
    emitted.find(message => message.id === 14).result.snapshot.fakeShip,
    '256',
  );
  assert.equal(
    emitted.find(message => message.id === 16).result.replay.exitCode,
    0,
  );
  assert.ok(emitted.some(message => (
    message.type === 'loop' &&
    message.status === 'complete' &&
    message.result.loop.stoppedBy === 'aborted'
  )));
  assert.equal(FakeService.instances[0].closed, true);
});

test('runtime worker handler reports command errors', async () => {
  const emitted = [];
  const handler = createAmesRuntimeWorkerHandler({
    emit: message => emitted.push(message),
    Service: FakeService,
    FileStore: FakeStore,
    fetchFn: okFetch(),
  });

  await handler.handle({ id: 1, type: 'keen', ship: '0x200', path: '/' });

  assert.equal(emitted.length, 1);
  assert.equal(emitted[0].type, 'error');
  assert.match(emitted[0].error, /runtime service is not started/);
});

test('runtime worker routes hosted http-client fetches through configured proxy', async () => {
  FakeStore.instances = [];
  FakeService.instances = [];
  const emitted = [];
  const fetchFn = recordingFetch([7, 8, 9]);
  const handler = createAmesRuntimeWorkerHandler({
    emit: message => emitted.push(message),
    Service: FakeService,
    FileStore: FakeStore,
    fetchFn,
  });

  await handler.handle({
    id: 1,
    type: 'start',
    wasmUrl: 'https://example.invalid/vere-disk-wasm.wasm',
    pillUrl: 'https://example.invalid/brass.pill',
    bridgeUrl: 'https://bridge/~_~/ames',
    fakeShip: '0x100',
    sessionId: '1',
    httpClientProxyUrl: 'https://demo.invalid/_vere/http-client',
  });

  const host = FakeService.instances[0].options.httpClientHostFactory({
    onLog: message => emitted.push({ type: 'log', ...message }),
  });
  assert.equal(host.streamResponses, true);
  await host.fetchFn('https://bootstrap.urbit.org/glob-0v3.test.glob', {
    method: 'GET',
    headers: { accept: 'application/octet-stream' },
  });

  assert.equal(fetchFn.calls.at(-1).url, 'https://demo.invalid/_vere/http-client');
  assert.equal(fetchFn.calls.at(-1).init.method, 'POST');
  assert.deepEqual(JSON.parse(fetchFn.calls.at(-1).init.body), {
    url: 'https://bootstrap.urbit.org/glob-0v3.test.glob',
    method: 'GET',
    headers: [['accept', 'application/octet-stream']],
  });
  assert.ok(emitted.some(message => (
    message.type === 'log' &&
    message.message.includes('http-client proxy GET https://bootstrap.urbit.org/glob-0v3.test.glob')
  )));
});
