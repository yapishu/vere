import assert from 'node:assert/strict';
import test from 'node:test';

import {
  BoundedPacketQueue,
  runWasmMesaEventLoop,
  runQueuedWasmMesaLoop,
} from './ames-wasm-runtime-loop.mjs';

test('BoundedPacketQueue copies packets and reports overflow', () => {
  const drops = [];
  const queue = new BoundedPacketQueue({
    maxPackets: 2,
    onDrop: drop => drops.push(drop),
  });
  const first = new Uint8Array([1, 2, 3]);

  assert.equal(queue.push(first), true);
  first[0] = 9;
  assert.equal(queue.push(new Uint8Array([4])), true);
  assert.equal(queue.push(new Uint8Array([5])), false);

  assert.deepEqual(queue.stats(), {
    accepted: 2,
    dropped: 1,
    queued: 2,
    maxPackets: 2,
  });
  assert.equal(drops.length, 1);
  assert.deepEqual(
    queue.drainAll().map(packet => [...packet]),
    [[1, 2, 3], [4]],
  );
  assert.equal(queue.length, 0);
});

test('BoundedPacketQueue wakes waiters and reports close', async () => {
  const queue = new BoundedPacketQueue();
  const pending = queue.waitForPacket();

  queue.push(new Uint8Array([8]));
  assert.equal(await pending, 'packet');
  assert.equal(await queue.waitForPacket(), 'packet');
  assert.deepEqual([...queue.drainAll()[0]], [8]);

  const closed = queue.waitForPacket();
  queue.close();
  assert.equal(await closed, 'closed');
  assert.equal(queue.push(new Uint8Array([9])), false);
  assert.equal(queue.stats().dropped, 1);
});

test('BoundedPacketQueue waiters time out and honor abort', async () => {
  const queue = new BoundedPacketQueue();
  const controller = new AbortController();
  const aborted = queue.waitForPacket({ signal: controller.signal });

  controller.abort();
  assert.equal(await aborted, 'aborted');
  assert.equal(await queue.waitForPacket({ timeoutMs: 0 }), 'timeout');
});

test('runQueuedWasmMesaLoop stops on idle queue', async () => {
  const queue = new BoundedPacketQueue();
  const idle = [];

  const result = await runQueuedWasmMesaLoop({
    queue,
    maxRounds: 3,
    delay: async () => {},
    runBatch: async () => {
      throw new Error('runBatch should not be called when idle');
    },
    onIdle: event => idle.push(event),
  });

  assert.deepEqual(idle, [{ rounds: 0, packets: 0, idleRounds: 1 }]);
  assert.deepEqual(result, {
    rounds: 0,
    packets: 0,
    routedPushes: 0,
    droppedRoutes: 0,
    droppedInbound: 0,
    queued: 0,
    stoppedBy: 'idle',
  });
});

test('runQueuedWasmMesaLoop can linger through transient idle polls', async () => {
  const queue = new BoundedPacketQueue();
  const idle = [];
  let delays = 0;

  const result = await runQueuedWasmMesaLoop({
    queue,
    maxRounds: 1,
    idleRoundsToStop: 2,
    delay: async () => {
      delays++;
      if (delays === 2) {
        queue.push(new Uint8Array([7]));
      }
    },
    runBatch: async ({ packets }) => ({
      effects: packets.map(packet => ({ byte: packet[0] })),
    }),
    routeEffects: async () => ({ sent: 1, dropped: 0 }),
    onIdle: event => idle.push(event),
  });

  assert.deepEqual(idle, [{ rounds: 0, packets: 0, idleRounds: 1 }]);
  assert.equal(result.rounds, 1);
  assert.equal(result.packets, 1);
  assert.equal(result.routedPushes, 1);
  assert.equal(result.stoppedBy, 'round-cap');
});

test('runQueuedWasmMesaLoop batches packets and counts routed effects', async () => {
  const queue = new BoundedPacketQueue();
  queue.push(new Uint8Array([1]));
  queue.push(new Uint8Array([2]));

  const starts = [];
  const completions = [];
  const result = await runQueuedWasmMesaLoop({
    queue,
    maxRounds: 2,
    delay: async () => {},
    onRoundStart: event => starts.push({
      round: event.round,
      startIndex: event.startIndex,
      packets: event.packets.map(packet => packet[0]),
    }),
    runBatch: async ({ round, startIndex, packets }) => ({
      effects: packets.map((packet, index) => ({
        label: `reply-${startIndex + index}`,
        byte: packet[0],
        round,
      })),
    }),
    routeEffects: async effect => ({
      sent: effect.byte === 1 ? ['a', 'b'] : ['c'],
      dropped: effect.byte === 2 ? ['drop'] : [],
    }),
    onRoundComplete: event => completions.push({
      round: event.round,
      sent: event.sent,
      dropped: event.dropped,
      ok: event.ok,
    }),
  });

  assert.deepEqual(starts, [{
    round: 1,
    startIndex: 0,
    packets: [1, 2],
  }]);
  assert.deepEqual(completions, [{
    round: 1,
    sent: 3,
    dropped: 1,
    ok: true,
  }]);
  assert.equal(result.rounds, 1);
  assert.equal(result.packets, 2);
  assert.equal(result.routedPushes, 3);
  assert.equal(result.droppedRoutes, 1);
  assert.equal(result.stoppedBy, 'idle');
});

test('runQueuedWasmMesaLoop leaves queued packets when the round cap is hit', async () => {
  const queue = new BoundedPacketQueue();
  queue.push(new Uint8Array([1]));

  const result = await runQueuedWasmMesaLoop({
    queue,
    maxRounds: 1,
    delay: async () => {},
    runBatch: async () => {
      queue.push(new Uint8Array([2]));
      return { effects: [] };
    },
  });

  assert.equal(result.rounds, 1);
  assert.equal(result.packets, 1);
  assert.equal(result.queued, 1);
  assert.equal(result.stoppedBy, 'round-cap');
  assert.deepEqual([...queue.drainAll()[0]], [2]);
});

test('runQueuedWasmMesaLoop stops after a failed batch', async () => {
  const queue = new BoundedPacketQueue();
  queue.push(new Uint8Array([1]));

  const result = await runQueuedWasmMesaLoop({
    queue,
    maxRounds: 3,
    delay: async () => {},
    runBatch: async () => ({ ok: false, effects: [{ label: 'failed' }] }),
    routeEffects: async () => ({ sent: 1, dropped: 0 }),
  });

  assert.equal(result.rounds, 1);
  assert.equal(result.packets, 1);
  assert.equal(result.routedPushes, 1);
  assert.equal(result.stoppedBy, 'batch-failed');
});

test('runWasmMesaEventLoop wakes on packets without polling delay', async () => {
  const queue = new BoundedPacketQueue();
  const starts = [];

  const resultPromise = runWasmMesaEventLoop({
    queue,
    maxRounds: 1,
    firstIdleTimeoutMs: 100,
    onRoundStart: event => starts.push({
      round: event.round,
      packets: event.packets.map(packet => packet[0]),
    }),
    runBatch: async ({ packets }) => ({
      effects: packets.map(packet => ({ byte: packet[0] })),
    }),
    routeEffects: async effect => ({
      sent: effect.byte === 4 ? ['a'] : [],
      dropped: [],
    }),
  });

  queue.push(new Uint8Array([4]));
  const result = await resultPromise;

  assert.deepEqual(starts, [{ round: 1, packets: [4] }]);
  assert.equal(result.rounds, 1);
  assert.equal(result.packets, 1);
  assert.equal(result.routedPushes, 1);
  assert.equal(result.stoppedBy, 'round-cap');
});

test('runWasmMesaEventLoop stops after quiet timeout', async () => {
  const queue = new BoundedPacketQueue();
  queue.push(new Uint8Array([1]));

  const result = await runWasmMesaEventLoop({
    queue,
    maxRounds: 3,
    idleTimeoutMs: 0,
    runBatch: async () => ({ effects: [] }),
  });

  assert.equal(result.rounds, 1);
  assert.equal(result.packets, 1);
  assert.equal(result.stoppedBy, 'idle');
});

test('runWasmMesaEventLoop stops when the queue closes', async () => {
  const queue = new BoundedPacketQueue();
  const resultPromise = runWasmMesaEventLoop({
    queue,
    runBatch: async () => {
      throw new Error('runBatch should not be called after close');
    },
  });

  queue.close();
  const result = await resultPromise;

  assert.equal(result.rounds, 0);
  assert.equal(result.packets, 0);
  assert.equal(result.stoppedBy, 'closed');
});
