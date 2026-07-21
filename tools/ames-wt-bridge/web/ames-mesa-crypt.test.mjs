import assert from 'node:assert/strict';
import test from 'node:test';

import {
  bytesFromHoonCryptoAtom,
  chacha8,
  hoonCryptoAtomFromBytes,
  mesaDecryptBytes,
  mesaCrypt,
  mesaEncryptBytes,
  mesaMacBinding,
  mesaOpenPath,
  mesaSealPath,
  scotUv,
  slawUv,
  xchacha8,
} from './ames-mesa-crypt.mjs';
import { mesaPathBytes } from './ames-mesa-path.mjs';
import { bytesToHex } from './mesa-pact.mjs';

test('chacha8 matches Hoon crypt:chacha:crypto vector', () => {
  const out = chacha8(
    0n,
    0n,
    0n,
    Uint8Array.of(0x44, 0x33, 0x22, 0x11),
  );

  assert.equal(bytesToHex(out), '7a33cd3e');
  assert.equal(hoonCryptoAtomFromBytes(out), 1_053_635_450n);
});

test('xchacha8 matches Hoon xchacha:chacha:crypto vector', () => {
  const out = xchacha8(0n, 1n);

  assert.equal(
    bytesToHex(out.keyBytes),
    '6d368aac7c41a11e8ddd78f38d8c85429f8abf251deb21bb4b56e5d8821e68aa',
  );
  assert.equal(bytesToHex(out.nonceBytes), '0000000000000000');
});

test('mesaCrypt matches Hoon Mesa crypt helper vector', () => {
  const out = mesaCrypt(
    0x1234n,
    Uint8Array.of(0x55),
    Uint8Array.of(0x34, 0x12),
  );

  assert.equal(bytesToHex(out), '19b1');
  assert.equal(hoonCryptoAtomFromBytes(out), 45_337n);
  assert.deepEqual(
    Array.from(mesaCrypt(0x1234n, Uint8Array.of(0x55), out)),
    [0x34, 0x12],
  );
});

test('scotUv renders and parses Hoon scot %uv vectors', () => {
  const vectors = [
    [0n, '~.0v0'],
    [1n, '~.0v1'],
    [10n, '~.0va'],
    [31n, '~.0vv'],
    [32n, '~.0v10'],
    [255n, '~.0v7v'],
    [256n, '~.0v80'],
    [4_294_967_295n, '~.0v3v.vvvvv'],
    [4_294_967_296n, '~.0v40.00000'],
  ];

  for (const [atom, text] of vectors) {
    assert.equal(scotUv(atom), text);
    assert.equal(slawUv(text), atom);
  }
});

test('mesaSealPath matches Hoon seal-path vector', () => {
  const sealed = mesaSealPath(0x1234n, '/a/x/1/base/sys/kelvin');

  assert.equal(
    sealed,
    0x1_acdf_e1aa_632d_dd58_f252_28a3_7c03_f1d6_62f3_4823_71e5_1995_1967_a0f8_08a5_eca9_c23e_135d_f1edn,
  );
  assert.equal(
    scotUv(sealed),
    '~.0vq.pnv1l.9hirn.aou99.2h8rs.0fotc.onj90.hn3p8.pikcm.f87o1.2iupa.e27o9.lrsfd',
  );
  assert.equal(mesaOpenPath(0x1234n, sealed), '/a/x/1/base/sys/kelvin');
  assert.equal(mesaOpenPath(0x1234n, slawUv(scotUv(sealed))), '/a/x/1/base/sys/kelvin');
  assert.throws(() => mesaOpenPath(0x1235n, sealed), /authentication failed/);
});

test('mesaEncryptBytes matches Hoon encrypt helper vector', () => {
  const ciphertext = mesaEncryptBytes(
    0x1234n,
    0x55n,
    Uint8Array.of(0x34, 0x12),
  );

  assert.equal(bytesToHex(ciphertext), '19b101');
  assert.equal(hoonCryptoAtomFromBytes(ciphertext), 0x1_b119n);
  assert.deepEqual(
    Array.from(mesaDecryptBytes(0x1234n, 0x55n, ciphertext)),
    [0x34, 0x12],
  );
});

test('mesaMacBinding matches Hoon mac helper vector', () => {
  const root = bytesFromHoonCryptoAtom(
    0x1111_1111_1111_1111_1111_1111_1111_1111_2222_2222_2222_2222_2222_2222_2222_2222n,
    32,
  );
  const binding = new Uint8Array(mesaPathBytes('/~zod//1/a/x/1/base/sys/kelvin').length + 32);
  binding.set(mesaPathBytes('/~zod//1/a/x/1/base/sys/kelvin'), 0);
  binding.set(root, binding.length - 32);

  assert.equal(bytesToHex(mesaMacBinding(0x1234n, binding)), 'bbe06b59e17df2d54c2250ce122d9559');
});
