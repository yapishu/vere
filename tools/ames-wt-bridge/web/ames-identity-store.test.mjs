import assert from 'node:assert/strict';
import test from 'node:test';

import {
  AmesIdentityStore,
  MemoryIdentityBackend,
} from './ames-identity-store.mjs';
import {
  encodeSuiteBRingBytes,
  importSuiteBRing,
} from './ames-identity.mjs';
import { bytesToHex, hexToBytes } from './mesa-pact.mjs';

const RFC8032_SEED =
  '9d61b19deffd5a60ba844af492ec2cc4' +
  '4449c5697b326919703bac031cae7f60';

function deterministicRing() {
  const signingSeed = hexToBytes(RFC8032_SEED);
  const cryptoSeed = Uint8Array.from({ length: 32 }, (_, i) => 31 - i);
  return encodeSuiteBRingBytes(new Uint8Array([...signingSeed, ...cryptoSeed]));
}

test('AmesIdentityStore persists and reloads suite-B ring material', async () => {
  const backend = new MemoryIdentityBackend();
  const store = new AmesIdentityStore({ backend, clock: () => 1234 });
  const identity = await importSuiteBRing(deterministicRing(), { retainRing: true });

  const record = await store.save('default-comet', identity);
  const loaded = await store.load('default-comet');

  assert.equal(record.name, 'default-comet');
  assert.equal(record.updatedAt, 1234);
  assert.equal(record.ringHex, bytesToHex(identity.ringBytes));
  assert.equal(loaded.ship, identity.ship);
  assert.equal(loaded.comet, identity.ship);
  assert.equal(loaded.sponsor, 0x0773n);
  assert.equal(bytesToHex(loaded.passBytes), bytesToHex(identity.passBytes));
  assert.equal(bytesToHex(loaded.ringBytes), bytesToHex(identity.ringBytes));
  assert.deepEqual(await store.list(), ['default-comet']);
});

test('AmesIdentityStore forgets saved identities', async () => {
  const store = new AmesIdentityStore({ backend: new MemoryIdentityBackend() });

  assert.equal(await store.load('missing'), null);
  await store.save('a', deterministicRing());
  assert.deepEqual(await store.list(), ['a']);
  await store.forget('a');
  assert.equal(await store.load('a'), null);
  assert.deepEqual(await store.list(), []);
  await assert.rejects(() => store.save('bad', undefined), /ring bytes/);
});
