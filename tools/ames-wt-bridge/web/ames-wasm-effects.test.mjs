import assert from 'node:assert/strict';
import test from 'node:test';

import { encodePeek } from './mesa-pact.mjs';
import {
  amesWire,
  mesaSessionLane,
} from './ames-wasm-events.mjs';
import {
  extractMesaEffects,
  isMesaSessionLane,
  mesaSessionIdFromLane,
} from './ames-wasm-effects.mjs';
import {
  atomFromBytesLE,
  cell,
  jamBytes,
  list,
  termAtom,
  tuple,
} from './urbit-noun.mjs';

function packetAtom() {
  return atomFromBytesLE(encodePeek({
    ship: 0x100n,
    path: '/~zod/0/1/c/x/1/base/sys/kelvin',
  }));
}

test('extractMesaEffects accepts nil effect lists', () => {
  assert.deepEqual(extractMesaEffects(jamBytes(0n)), {
    sends: [],
    pushes: [],
    binds: [],
    unknown: [],
  });
});

test('extractMesaEffects normalizes raw Ames %give %push moves', () => {
  const lane = mesaSessionLane(9n);
  const effect = cell(
    list(amesWire()),
    tuple(
      termAtom('give'),
      tuple(termAtom('push'), list(lane), packetAtom()),
    ),
  );

  const out = extractMesaEffects(jamBytes(list(effect)));

  assert.equal(out.pushes.length, 1);
  assert.equal(out.sends.length, 0);
  assert.equal(out.binds.length, 0);
  assert.equal(out.unknown.length, 0);
  assert.deepEqual(out.pushes[0].lanes, [lane]);
  assert.equal(out.pushes[0].packetAtom, packetAtom());
  assert.ok(out.pushes[0].packet.length > 0);
  assert.equal(isMesaSessionLane(out.pushes[0].lanes[0]), true);
  assert.equal(mesaSessionIdFromLane(out.pushes[0].lanes[0]), 9n);
});

test('extractMesaEffects normalizes lowered Ames %push cards', () => {
  const lane = mesaSessionLane(1n);
  const effect = cell(
    cell(0n, amesWire()),
    tuple(termAtom('push'), list(lane), packetAtom()),
  );

  const out = extractMesaEffects(jamBytes(list(effect)));

  assert.equal(out.pushes.length, 1);
  assert.equal(out.sends.length, 0);
  assert.equal(out.pushes[0].lanes[0], lane);
  assert.deepEqual(out.pushes[0].wire[1], amesWire());
});

test('extractMesaEffects normalizes Mesa %bind cards and structured lanes', () => {
  const lane = tuple(termAtom('if'), 0x7f000001n, 8443n);
  const effect = cell(
    amesWire(),
    tuple(termAtom('bind'), 0x100n, 2n, 3n, 4n, lane),
  );

  const out = extractMesaEffects(jamBytes(list(effect)));

  assert.equal(out.sends.length, 0);
  assert.equal(out.pushes.length, 0);
  assert.equal(out.binds.length, 1);
  assert.equal(out.unknown.length, 0);
  assert.deepEqual(out.binds[0].lane, {
    type: 'if',
    ip: 0x7f000001,
    port: 8443,
    noun: lane,
  });
  assert.equal(out.binds[0].ship, 0x100n);
  assert.equal(out.binds[0].rift, 2n);
  assert.equal(out.binds[0].bone, 3n);
  assert.equal(out.binds[0].sequence, 4n);
});

test('extractMesaEffects normalizes legacy Ames %send cards separately', () => {
  const lane = cell(0n, 0n);
  const effect = cell(
    amesWire(),
    tuple(termAtom('send'), lane, packetAtom()),
  );

  const out = extractMesaEffects(jamBytes(list(effect)));

  assert.equal(out.sends.length, 1);
  assert.equal(out.pushes.length, 0);
  assert.equal(out.binds.length, 0);
  assert.equal(out.unknown.length, 0);
  assert.deepEqual(out.sends[0].lane, {
    type: 'ames-ship',
    ship: 0n,
    noun: lane,
  });
  assert.equal(out.sends[0].packetAtom, packetAtom());
  assert.ok(out.sends[0].packet.length > 0);
});

test('extractMesaEffects leaves unrelated lowered cards unknown', () => {
  const effect = cell(
    cell(termAtom('behn'), 0n),
    tuple(termAtom('push'), 0n, 1n),
  );

  const out = extractMesaEffects(jamBytes(list(effect)));

  assert.equal(out.sends.length, 0);
  assert.equal(out.pushes.length, 0);
  assert.equal(out.binds.length, 0);
  assert.equal(out.unknown.length, 1);
  assert.equal(out.unknown[0].reason, 'not a Mesa card');
});
