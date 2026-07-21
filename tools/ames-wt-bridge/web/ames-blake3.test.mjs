import assert from 'node:assert/strict';
import test from 'node:test';

import {
  blake3DeriveKey,
  blake3Hash,
  lssRoot,
} from './ames-blake3.mjs';
import { bytesToHex } from './mesa-pact.mjs';

test('blake3Hash matches standard and Hoon root:lss vectors', () => {
  assert.equal(
    bytesToHex(blake3Hash(new Uint8Array())),
    'af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262',
  );
  assert.equal(
    bytesToHex(blake3Hash(new TextEncoder().encode('abc'))),
    '6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85',
  );

  const seq65 = Uint8Array.from({ length: 65 }, (_, i) => i);
  assert.equal(
    bytesToHex(lssRoot(seq65)),
    'de1e5fa0be70df6d2be8fffd0e99ceaa8eb6e8c93a63f2d8d1c30ecb6b263dee',
  );

  const seq128 = Uint8Array.from({ length: 128 }, (_, i) => i);
  assert.equal(
    bytesToHex(lssRoot(seq128)),
    'f17e570564b26578c33bb7f44643f539624b05df1a76c81f30acd548c44b45ef',
  );
});

test('blake3DeriveKey matches Hoon kdf:blake3 vectors', () => {
  const seed32 = new Uint8Array(32);
  seed32[0] = 0x34;
  seed32[1] = 0x12;

  assert.equal(
    bytesToHex(blake3DeriveKey('mesa-aead', seed32, 64)),
    '377eceba64a30c5c89dbd5e5f87e659ee25295495410ca1677a4df1a50754220deae02ea44b4dcbba52b1b4e336cde22b02a7438c0f797e9420ba78c105ead71',
  );
  assert.equal(
    bytesToHex(blake3DeriveKey('mesa-crypt-iv', Uint8Array.of(0x55), 24)),
    'dc66a39c6817664a0d3da5c3b01f684efcf5bcbe54060cc5',
  );
});
