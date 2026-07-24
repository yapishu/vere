import assert from 'node:assert/strict';
import test from 'node:test';

import {
  atomBytesLE,
  atomFromBytesLE,
  bytesFromAtomLE,
  cell,
  cue,
  cueBytes,
  jam,
  jamBytes,
  termAtom,
  tuple,
} from './urbit-noun.mjs';
import { bytesToHex } from './mesa-pact.mjs';

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

test('jam matches Hoon vectors for atoms and cells', () => {
  assert.equal(jam(0n), 0x2n);
  assert.equal(jam(1n), 0xcn);
  assert.equal(jam(42n), 0x1550n);
  assert.equal(jam(cell(1n, 2n)), 0x1231n);
  assert.equal(jam(tuple(1n, 2n, 3n)), 0x344871n);
  assert.equal(jam(cell(cell(1n, 2n), 3n)), 0x3448c5n);
});

test('termAtom and tuple match Hoon term and proof tuple vectors', () => {
  assert.equal(termAtom('message'), 0x65_6761_7373_656dn);
  assert.equal(termAtom('proof'), 0x66_6f6f_7270n);
  assert.equal(
    jam(tuple(termAtom('message'), termAtom('proof'), 0n)),
    0x2ccd_edee_4e07_8072_b3b0_b9b9_b2b6_de01n,
  );
});

test('cue round-trips jammed nouns without backrefs', () => {
  const nouns = [
    0n,
    1n,
    42n,
    cell(1n, 2n),
    tuple(1n, 2n, 3n),
    cell(cell(1n, 2n), 3n),
    tuple(termAtom('message'), termAtom('proof'), 0n),
  ];

  for (const noun of nouns) {
    sameNoun(cue(jam(noun)), noun);
  }
});

test('cue handles Hoon jam backreferences', () => {
  sameNoun(cue(0x49_c8c5n), cell(cell(1n, 2n), cell(1n, 2n)));
  sameNoun(
    cue(0x24_ed6c_6c2f_01ca_cec2_e6e6_cadb_7805n),
    cell(
      tuple(termAtom('message'), termAtom('ack'), 0n),
      tuple(termAtom('message'), termAtom('ack'), 0n),
    ),
  );
});

test('atom byte helpers are little-endian', () => {
  assert.equal(atomFromBytesLE(Uint8Array.of(0x62, 0xd7, 0x5a)), 0x5ad762n);
  assert.deepEqual(Array.from(bytesFromAtomLE(0x5ad762n, 3)), [0x62, 0xd7, 0x5a]);
  assert.equal(bytesToHex(jamBytes(42n)), '5015');
  assert.deepEqual(
    Array.from(jamBytes(atomBytesLE(Uint8Array.of(0x62, 0xd7, 0x5a)))),
    Array.from(jamBytes(0x5ad762n)),
  );
  assert.throws(() => bytesFromAtomLE(-1n, 1), /non-negative/);
  assert.throws(() => bytesFromAtomLE(0x100n, 1), /does not fit/);
});

test('cueBytes can preserve large atoms as byte-backed atoms', () => {
  const bytes = Uint8Array.from({ length: 1024 }, (_, i) => i & 0xff);
  const noun = cueBytes(jamBytes(atomBytesLE(bytes)), {
    byteAtomBitThreshold: 1,
  });

  assert.deepEqual([...bytesFromAtomLE(noun, bytes.length)], [...bytes]);
});
