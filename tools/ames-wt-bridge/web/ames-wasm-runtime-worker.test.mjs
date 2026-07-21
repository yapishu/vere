import assert from 'node:assert/strict';
import test from 'node:test';

import {
  createAmesRuntimeWorkerHandler,
} from './ames-wasm-runtime-worker.mjs';

function okFetch(bytes = [1, 2, 3]) {
  return async () => ({
    ok: true,
    async arrayBuffer() {
      return Uint8Array.from(bytes).buffer;
    },
  });
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
    this.closed = false;
    this.loopPromise = null;
    this.resolveLoop = null;
    FakeService.instances.push(this);
    options.onLog({ message: 'service-log', className: 'rx' });
    options.onStdout(new TextEncoder().encode('stdout-line\n'));
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
  await handler.handle({ id: 6, type: 'pump', maxRounds: 1, firstIdleTimeoutMs: 0 });
  await handler.handle({ id: 7, type: 'pump-start', firstIdleTimeoutMs: 1000 });
  await handler.handle({ id: 8, type: 'pump-stop' });
  await handler.handle({ id: 9, type: 'snapshot' });
  await handler.handle({ id: 10, type: 'save' });
  await handler.handle({ id: 11, type: 'replay', shutdownRuntime: false });
  await handler.handle({ id: 12, type: 'shutdown' });

  assert.equal(FakeStore.instances.length, 1);
  assert.equal(FakeStore.instances[0].options.scope, 'worker-test');
  assert.equal(FakeStore.instances[0].cleared, true);
  assert.equal(FakeService.instances.length, 1);
  assert.deepEqual([...FakeService.instances[0].options.pillBytes], [4, 5]);
  assert.equal(FakeService.instances[0].options.fakeShip, 0x100n);
  assert.equal(FakeService.instances[0].mateInput.ship, 0x200n);
  assert.deepEqual([...FakeService.instances[0].injectInput.packet], [9, 8, 7]);
  assert.equal(FakeService.instances[0].pumpStartInput.firstIdleTimeoutMs, 1000);
  assert.equal(FakeService.instances[0].replayInput.shutdownRuntime, false);

  assert.ok(emitted.some(message => (
    message.type === 'log' && message.message === 'service-log'
  )));
  assert.ok(emitted.some(message => (
    message.type === 'log' && message.message === 'wasm: stdout-line'
  )));
  assert.ok(emitted.some(message => (
    message.type === 'log' && message.message === 'pill-bytes=2'
  )));
  assert.deepEqual(
    emitted.filter(message => message.type === 'result').map(message => message.id),
    [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12],
  );
  assert.equal(
    emitted.find(message => message.id === 9).result.snapshot.fakeShip,
    '256',
  );
  assert.equal(
    emitted.find(message => message.id === 11).result.replay.exitCode,
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
