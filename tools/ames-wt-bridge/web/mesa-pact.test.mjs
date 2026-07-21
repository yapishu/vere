import assert from 'node:assert/strict';
import test from 'node:test';

import {
  AUTH,
  HOP,
  bytesToHex,
  decodePact,
  encodePact,
  encodePeek,
  hexToBytes,
  mugBytes,
} from './mesa-pact.mjs';

const CURRENT_PEEK =
  '10e1d1790002e0670000010000001f00' +
  '2f7e7a6f642f302f312f632f782f312f' +
  '626173652f7379732f6b656c76696e';

const STALE_PEEK =
  '08301cf2110200000000010000001f00' +
  '2f7e7a6f642f302f312f632f782f312f' +
  '626173652f7379732f6b656c76696e';

test('mugBytes matches Vere spot checks', () => {
  assert.equal(mugBytes(new Uint8Array()), 0x79ff04e8);
  assert.equal(mugBytes(Uint8Array.of(1)), 0x715c2a60);
  assert.equal(mugBytes(Uint8Array.of(2)), 0x718b9468);
});

test('encodePeek emits the current pact.c vector', () => {
  const packet = encodePeek({
    ship: 0x100n,
    path: '/~zod/0/1/c/x/1/base/sys/kelvin',
  });

  assert.equal(packet.length, 47);
  assert.equal(bytesToHex(packet), CURRENT_PEEK);

  const decoded = decodePact(packet);
  assert.equal(decoded.type, 'peek');
  assert.equal(decoded.next, HOP.NONE);
  assert.equal(decoded.hop, 0);
  assert.equal(decoded.name.ship, 0x100n);
  assert.equal(decoded.name.rift, 0);
  assert.equal(decoded.name.bloq, 0);
  assert.equal(decoded.name.frag, 0n);
  assert.equal(decoded.name.path, '/~zod/0/1/c/x/1/base/sys/kelvin');
});

test('old captured vector is rejected by the current codec', () => {
  assert.throws(() => decodePact(hexToBytes(STALE_PEEK)), /bad protocol/);
});

test('decodePact rejects bad mugs', () => {
  const packet = encodePeek({
    ship: 0x100n,
    path: '/~zod/0/1/c/x/1/base/sys/kelvin',
  });
  packet[packet.length - 1] ^= 1;
  assert.throws(() => decodePact(packet), /bad mug/);
});

test('page pact round-trips with auth-none and long-hop data', () => {
  const pact = {
    type: 'page',
    next: HOP.LONG,
    name: {
      ship: 0x123456789abcn,
      rift: 15,
      bloq: 13,
      frag: 54n,
      path: 'foo/bar',
    },
    data: {
      totalBytes: 4n,
      auth: { type: AUTH.NONE },
      fragment: Uint8Array.of(1, 2, 3, 4),
    },
    longHop: Uint8Array.of(0x01, 127, 0, 0, 1, 0xfb, 0x20),
  };

  const packet = encodePact(pact);
  const decoded = decodePact(packet);
  assert.equal(bytesToHex(encodePact(decoded)), bytesToHex(packet));
  assert.equal(decoded.type, 'page');
  assert.equal(decoded.name.ship, pact.name.ship);
  assert.equal(decoded.name.path, pact.name.path);
  assert.equal(bytesToHex(decoded.data.fragment), '01020304');
  assert.equal(bytesToHex(decoded.longHop), '017f000001fb20');
});

test('poke pact round-trips with signature auth data', () => {
  const signature = Uint8Array.from({ length: 64 }, (_, i) => i);
  const fragment = new TextEncoder().encode('poke-fragment');
  const pact = {
    type: 'poke',
    name: {
      ship: 0x100n,
      rift: 2,
      bloq: 13,
      auth: true,
      frag: 1n,
      path: '/poke/name',
    },
    payloadName: {
      ship: 0x100n,
      rift: 2,
      bloq: 13,
      frag: 0n,
      path: '/poke/payload',
    },
    data: {
      totalBytes: BigInt(fragment.length),
      auth: { type: AUTH.SIGN, signature },
      fragment,
    },
  };

  const packet = encodePact(pact);
  const decoded = decodePact(packet);
  assert.equal(bytesToHex(encodePact(decoded)), bytesToHex(packet));
  assert.equal(decoded.type, 'poke');
  assert.equal(decoded.name.auth, true);
  assert.equal(decoded.payloadName.path, '/poke/payload');
  assert.equal(decoded.data.auth.type, AUTH.SIGN);
  assert.equal(bytesToHex(decoded.data.auth.signature), bytesToHex(signature));
  assert.equal(new TextDecoder().decode(decoded.data.fragment), 'poke-fragment');
});
