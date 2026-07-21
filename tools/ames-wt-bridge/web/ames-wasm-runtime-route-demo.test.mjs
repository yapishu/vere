import assert from 'node:assert/strict';
import test from 'node:test';

import {
  assertRuntimeRouteSummary,
  createRuntimeWorkerCommandClient,
  runRuntimeServiceRouteDemo,
  runRuntimeWorkerRouteDemo,
} from './ames-wasm-runtime-route-demo.mjs';

const baseConfig = Object.freeze({
  wasmUrl: 'https://example.invalid/vere-disk-wasm.wasm',
  pillUrl: 'https://example.invalid/brass.pill',
  bridgeUrl: 'https://127.0.0.1:8443/~_~/ames',
  certificateHash: 'hash',
  loomExponent: 29,
  overheadMiB: 512,
  maximumMiB: 1536,
  fakeShip: '0x100',
  peer: '0x200',
  matePeer: '0x200',
  sessionId: '1',
  keenPath: '/c/x/1/kids/sys/kelvin',
  maxRounds: 8,
  idleRoundsToStop: 20,
  firstDelayMs: 12000,
  delayMs: 1500,
  minReplies: 1,
  scope: 'test-scope',
});

class FakeWorker {
  constructor(responses = {}) {
    this.responses = responses;
    this.commands = [];
    this.listeners = {
      message: [],
      error: [],
    };
    this.terminated = false;
  }

  addEventListener(type, listener) {
    this.listeners[type].push(listener);
  }

  postMessage(message) {
    this.commands.push(message);
    queueMicrotask(() => this.#respond(message));
  }

  terminate() {
    this.terminated = true;
  }

  #emit(type, data) {
    for (const listener of this.listeners[type]) {
      listener({ data });
    }
  }

  #respond(message) {
    const response = this.responses[message.type]?.(message) ??
      defaultResponse(message);
    if (response.type === 'error') {
      this.#emit('message', {
        type: 'error',
        id: message.id,
        command: message.type,
        error: response.error,
      });
      return;
    }
    if (response.log) {
      this.#emit('message', {
        type: 'log',
        message: response.log,
      });
    }
    if (response.terminal) {
      this.#emit('message', {
        type: 'terminal',
        event: response.terminal,
      });
    }
    this.#emit('message', {
      type: 'result',
      id: message.id,
      command: message.type,
      result: response.result,
    });
  }
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
}

class FakeService {
  static instances = [];

  constructor(options) {
    this.options = options;
    this.commands = [];
    this.closed = false;
    FakeService.instances.push(this);
    options.onLog({ message: 'service-log', className: 'rx' });
    options.onStdout(new TextEncoder().encode('stdout-line\n'));
  }

  async start() {
    this.commands.push('start');
    return { snapshot: { started: true, event: 21n } };
  }

  async connect() {
    this.commands.push('connect');
    return { snapshot: { connected: true } };
  }

  async mate() {
    this.commands.push('mate');
    return {
      routes: {
        sent: 1,
        dropped: 0,
      },
    };
  }

  async keen() {
    this.commands.push('keen');
    return {
      routes: {
        sent: 1,
        dropped: 0,
      },
    };
  }

  async runInputLoop() {
    this.commands.push('pump');
    return {
      loop: {
        rounds: 1,
        packets: 1,
        routedPushes: 1,
        droppedRoutes: 0,
        droppedInbound: 0,
        queued: 0,
        stoppedBy: 'idle',
      },
      snapshot: { event: 24n },
    };
  }

  async save() {
    this.commands.push('save');
    return { snapshot: { event: 24n } };
  }

  async replay(input) {
    this.commands.push('replay');
    this.replayInput = input;
    this.closed = true;
    return {
      replay: {
        exitCode: 0,
        initialBytes: 1024,
        maximumBytes: 2048,
      },
      snapshot: { event: 24n },
    };
  }

  async shutdown() {
    this.commands.push('shutdown');
    this.closed = true;
    return { snapshot: { started: false } };
  }
}

function okFetch(bytes = [1, 2, 3]) {
  return async () => ({
    ok: true,
    async arrayBuffer() {
      return Uint8Array.from(bytes).buffer;
    },
  });
}

function defaultResponse(message) {
  switch (message.type) {
    case 'start':
      return {
        log: 'pill-bytes=3',
        result: { snapshot: { started: true, event: '21' } },
      };
    case 'connect':
      return { result: { snapshot: { connected: true } } };
    case 'mate':
    case 'keen':
      return {
        result: {
          routes: {
            sent: 1,
            dropped: 0,
          },
          snapshot: { event: '23' },
        },
      };
    case 'pump':
      return {
        result: {
          loop: {
            rounds: 1,
            packets: 1,
            routedPushes: 1,
            droppedRoutes: 0,
            droppedInbound: 0,
            queued: 0,
            stoppedBy: 'idle',
          },
          snapshot: { event: '24' },
        },
      };
    case 'save':
      return { result: { snapshot: { event: '24' } } };
    case 'replay':
      return {
        result: {
          replay: {
            exitCode: 0,
            initialBytes: 1024,
            maximumBytes: 2048,
          },
          snapshot: { event: '24' },
        },
      };
    case 'shutdown':
      return { result: { snapshot: { started: false } } };
    default:
      return { type: 'error', error: `unexpected command ${message.type}` };
  }
}

test('runRuntimeWorkerRouteDemo drives the persistent worker command sequence', async () => {
  const worker = new FakeWorker();
  const emitted = [];

  const summary = await runRuntimeWorkerRouteDemo(baseConfig, {
    createWorker: () => worker,
    emit: message => emitted.push(message),
  });

  assert.equal(summary.ok, true);
  assert.equal(summary.initial.sent, 2);
  assert.equal(summary.loop.packets, 1);
  assert.equal(summary.replay.exitCode, 0);
  assert.deepEqual(worker.commands.map(command => command.type), [
    'start',
    'connect',
    'mate',
    'keen',
    'pump',
    'save',
    'replay',
  ]);
  assert.equal(worker.commands.at(-1).shutdownRuntime, true);
  assert.equal(worker.terminated, true);
  assert.ok(emitted.some(message => message.message === 'initial-sent=2'));
  assert.ok(emitted.some(message => message.message === 'replay-exit=0'));
});

test('runRuntimeServiceRouteDemo drives the inline persistent service sequence', async () => {
  FakeStore.instances = [];
  FakeService.instances = [];
  const emitted = [];

  const summary = await runRuntimeServiceRouteDemo(baseConfig, {
    Service: FakeService,
    FileStore: FakeStore,
    fetchFn: okFetch([4, 5]),
    emit: message => emitted.push(message),
  });

  assert.equal(summary.ok, true);
  assert.deepEqual(FakeService.instances[0].commands, [
    'start',
    'connect',
    'mate',
    'keen',
    'pump',
    'save',
    'replay',
  ]);
  assert.equal(FakeStore.instances[0].options.scope, 'test-scope');
  assert.equal(FakeStore.instances[0].cleared, true);
  assert.deepEqual([...FakeService.instances[0].options.pillBytes], [4, 5]);
  assert.equal(FakeService.instances[0].replayInput.shutdownRuntime, true);
  assert.ok(emitted.some(message => message.message === 'service-log'));
  assert.ok(emitted.some(message => message.message === 'wasm: stdout-line'));
  assert.ok(emitted.some(message => message.message === 'initial-sent=2'));
});

test('runRuntimeWorkerRouteDemo shuts down the persistent worker after command failure', async () => {
  const worker = new FakeWorker({
    keen: () => ({ type: 'error', error: 'keen failed' }),
  });

  await assert.rejects(
    runRuntimeWorkerRouteDemo(baseConfig, {
      createWorker: () => worker,
    }),
    /keen failed/,
  );

  assert.deepEqual(worker.commands.map(command => command.type), [
    'start',
    'connect',
    'mate',
    'keen',
    'shutdown',
  ]);
  assert.equal(worker.terminated, true);
});

test('createRuntimeWorkerCommandClient rejects worker command errors', async () => {
  const worker = new FakeWorker({
    keen: () => ({ type: 'error', error: 'bad keen' }),
  });
  const client = createRuntimeWorkerCommandClient(worker);

  await assert.rejects(
    client.command('keen', { ship: '0x200' }),
    /bad keen/,
  );
});

test('createRuntimeWorkerCommandClient forwards terminal events', async () => {
  const worker = new FakeWorker({
    'terminal-input': () => ({
      terminal: { type: 'write', text: 'dojo> ' },
      result: { events: [] },
    }),
  });
  const emitted = [];
  const client = createRuntimeWorkerCommandClient(worker, {
    emit: message => emitted.push(message),
  });

  await client.command('terminal-input', { text: '+trouble' });

  assert.deepEqual(emitted, [
    {
      type: 'terminal',
      event: { type: 'write', text: 'dojo> ' },
    },
  ]);
});

test('assertRuntimeRouteSummary reports route proof failures', () => {
  assert.throws(
    () => assertRuntimeRouteSummary({
      initial: { sent: 0, dropped: 0 },
      loop: {
        packets: 0,
        routedPushes: 0,
        droppedRoutes: 0,
        droppedInbound: 0,
      },
      replay: { exitCode: 1 },
    }),
    /sent 0 initial packets/,
  );
});
