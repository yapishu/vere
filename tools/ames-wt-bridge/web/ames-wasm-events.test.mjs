import assert from 'node:assert/strict';
import test from 'node:test';

import { encodePeek } from './mesa-pact.mjs';
import {
  bornOvumJam,
  dearOvumJam,
  keenOvumJam,
  loadMesaOvumJam,
  mateOvumJam,
  mesaHeerOvumJam,
  mesaSessionLane,
  oldAmesAddressLane,
  pathNoun,
} from './ames-wasm-events.mjs';
import {
  atomFromBytesLE,
  bytesFromAtomLE,
  cell,
  cue,
  termAtom,
  tuple,
} from './urbit-noun.mjs';

function sameNoun(a, b) {
  if (Array.isArray(a) || Array.isArray(b)) {
    assert.equal(Array.isArray(a), true);
    assert.equal(Array.isArray(b), true);
    assert.equal(a.length, 2);
    assert.equal(b.length, 2);
    sameNoun(a[0], b[0]);
    sameNoun(a[1], b[1]);
    return;
  }

  assert.equal(a, b);
}

test('loadMesaOvumJam builds the Ames runtime %load %mesa ovum', () => {
  sameNoun(
    cue(atomFromBytesLE(loadMesaOvumJam())),
    cell(
      cell(termAtom('a'), cell(termAtom('ames'), 0n)),
      cell(termAtom('load'), termAtom('mesa')),
    ),
  );
});

test('bornOvumJam builds the native Mesa driver %born ovum', () => {
  sameNoun(
    cue(atomFromBytesLE(bornOvumJam())),
    cell(
      cell(termAtom('a'), cell(termAtom('ames'), 0n)),
      cell(termAtom('born'), 0n),
    ),
  );
});

test('keenOvumJam builds an Ames %keen ovum with a Hoon path noun', () => {
  const ovum = cue(atomFromBytesLE(keenOvumJam({
    ship: 0x100n,
    path: '/c/x/1/kids/sys/kelvin',
  })));

  sameNoun(
    ovum[1],
    tuple(
      termAtom('keen'),
      0n,
      0x100n,
      pathNoun('/c/x/1/kids/sys/kelvin'),
    ),
  );
});

test('mateOvumJam builds a real per-peer Mesa migration task', () => {
  const ovum = cue(atomFromBytesLE(mateOvumJam({ ship: 0n })));

  sameNoun(
    ovum[1],
    tuple(
      termAtom('mate'),
      cell(0n, 0n),
      1n,
    ),
  );
});

test('mateOvumJam can build a dry migration test task', () => {
  const ovum = cue(atomFromBytesLE(mateOvumJam({ ship: 0x100n, dry: true })));

  sameNoun(
    ovum[1],
    tuple(
      termAtom('mate'),
      cell(0n, 0x100n),
      0n,
    ),
  );
});

test('dearOvumJam wraps a session atom as an old-Ames address lane', () => {
  const lane = oldAmesAddressLane(mesaSessionLane(3n));
  const ovum = cue(atomFromBytesLE(dearOvumJam({ ship: 0x100n, lane })));

  sameNoun(
    ovum[1],
    tuple(
      termAtom('dear'),
      0x100n,
      cell(1n, (1n << 63n) | 3n),
    ),
  );
});

test('mesaHeerOvumJam builds a session-lane Mesa packet ovum', () => {
  const packet = encodePeek({
    ship: 0x100n,
    path: '/~zod/0/1/c/x/1/base/sys/kelvin',
  });
  const lane = mesaSessionLane(7n);
  const ovum = cue(atomFromBytesLE(mesaHeerOvumJam({ lane, packet })));
  const task = ovum[1];
  const packetAtom = task[1][1];

  assert.equal(lane, (1n << 63n) | 7n);
  assert.equal(task[0], termAtom('heer'));
  assert.equal(task[1][0], lane);
  assert.deepEqual(
    Array.from(bytesFromAtomLE(packetAtom, packet.length)),
    Array.from(packet),
  );
});
