import assert from 'node:assert/strict';
import test from 'node:test';

import {
  describePactLane,
  isPactGalaxyLane,
  routeMesaEffects,
} from './ames-wasm-router.mjs';
import { atomFromBytesLE, jamBytes, list, termAtom, tuple } from './urbit-noun.mjs';

function packetAtom(bytes = [1, 2, 3]) {
  return atomFromBytesLE(Uint8Array.from(bytes));
}

function pushEffect(...lanes) {
  return list([
    [termAtom('ames'), 0n],
    tuple(termAtom('give'), tuple(termAtom('push'), list(...lanes), packetAtom())),
  ]);
}

function sendEffect(lane) {
  return list([
    [termAtom('ames'), 0n],
    tuple(termAtom('give'), tuple(termAtom('send'), lane, packetAtom())),
  ]);
}

test('describePactLane classifies galaxy and IPv4 lanes', () => {
  const ipLane = tuple(termAtom('if'), 0x7f000001n, 13337n);
  assert.equal(isPactGalaxyLane(0n), true);
  assert.equal(isPactGalaxyLane(255n), true);
  assert.equal(isPactGalaxyLane(256n), false);
  assert.deepEqual(describePactLane(0n), { type: 'galaxy', ship: 0 });
  assert.deepEqual(describePactLane({
    type: 'if',
    ip: 0x7f000001,
    port: 13337,
    noun: ipLane,
  }), {
    type: 'if',
    ip: 0x7f000001,
    port: 13337,
  });
  assert.deepEqual(describePactLane(0x100n), {
    type: 'unsupported',
    lane: 0x100n,
  });
});

test('routeMesaEffects sends galaxy and IPv4 lanes through the UDP callback', async () => {
  const routed = [];
  const ipLane = tuple(termAtom('if'), 0x7f000001n, 13337n);
  const out = await routeMesaEffects(jamBytes(pushEffect(0n, ipLane)), {
    sendLane: async (lane, packet) => {
      routed.push([lane, Array.from(packet)]);
      return 'datagram';
    },
  });

  assert.deepEqual(routed, [
    [{ type: 'galaxy', ship: 0 }, [1, 2, 3]],
    [{ type: 'if', ip: 0x7f000001, port: 13337 }, [1, 2, 3]],
  ]);
  assert.equal(out.sent.length, 2);
  assert.equal(out.dropped.length, 0);
});

test('routeMesaEffects sends legacy Ames galaxy and packed IPv4 lanes', async () => {
  const routed = [];
  const galaxy = [0n, 42n];
  const address = [1n, (8443n << 32n) | 0x7f00_0001n];
  for (const lane of [galaxy, address]) {
    await routeMesaEffects(jamBytes(sendEffect(lane)), {
      sendLane: async (described, packet) => {
        routed.push([described, Array.from(packet)]);
        return 'datagram';
      },
    });
  }

  assert.deepEqual(routed, [
    [{ type: 'galaxy', ship: 42 }, [1, 2, 3]],
    [{ type: 'if', ip: 0x7f00_0001, port: 8443 }, [1, 2, 3]],
  ]);
});

test('routeMesaEffects drops unsupported lanes', async () => {
  const drops = [];
  const out = await routeMesaEffects(jamBytes(pushEffect(0x100n)), {
    sendLane: async () => 'datagram',
    onDrop: drop => drops.push(drop),
  });

  assert.equal(out.sent.length, 0);
  assert.equal(out.dropped.length, 1);
  assert.deepEqual(drops[0].lane, { type: 'unsupported', lane: 0x100n });
});
