import assert from 'node:assert/strict';
import test from 'node:test';

import {
  aes256CmacAtom,
  aes256S2v,
  aes256SivDecrypt,
  aes256SivEncrypt,
  atomFromBytesBE,
  bytesFromAtomBE,
} from './ames-siv.mjs';

test('big-endian atom byte helpers round-trip crypto atoms', () => {
  assert.deepEqual(Array.from(bytesFromAtomBE(0n)), []);
  assert.deepEqual(Array.from(bytesFromAtomBE(0x1234n)), [0x12, 0x34]);
  assert.deepEqual(Array.from(bytesFromAtomBE(0x1234n, 4)), [0x00, 0x00, 0x12, 0x34]);
  assert.equal(atomFromBytesBE(Uint8Array.of(0x12, 0x34)), 0x1234n);
  assert.throws(() => bytesFromAtomBE(0x1_0000n, 2), /does not fit/);
});

test('aes256CmacAtom matches Hoon macc:aes:crypto vectors', async () => {
  assert.equal(
    await aes256CmacAtom(0n, 0n),
    0x503d_eaf0_ec13_bdbb_8e7b_c881_06e6_a0d2n,
  );
  assert.equal(
    await aes256CmacAtom(0n, 0x1234n),
    0x316e_479a_cc15_aeda_2f4f_38a8_df2a_2232n,
  );
  assert.equal(
    await aes256CmacAtom(1n, 0x11_2233_4455n),
    0xeb48_7b11_a953_add1_2875_e0a3_f92a_78c7n,
  );
});

test('aes256S2v matches Hoon s2vc:aes:crypto vectors', async () => {
  assert.equal(
    await aes256S2v(0n, [0n, 1n, 2n, 0x1234n]),
    0x8960_bae6_d467_35d1_9db0_d8da_6b9e_10a8n,
  );
  assert.equal(
    await aes256S2v(1n, [0x102n, 0x3_0405n, 0x607_0809n]),
    0x0197_0402_29b9_3b7d_b827_b57e_e606_da59n,
  );
});

test('aes256SivEncrypt matches Hoon sivc:aes:crypto vectors', async () => {
  assert.deepEqual(
    await aes256SivEncrypt(0n, [0n, 1n, 2n], 0x1234n),
    {
      iv: 0x8960_bae6_d467_35d1_9db0_d8da_6b9e_10a8n,
      length: 2,
      ciphertext: 0x77f3n,
    },
  );
  assert.deepEqual(
    await aes256SivEncrypt(1n, [0x102n, 0x3_0405n], 0x607_0809n),
    {
      iv: 0x7977_e634_caf6_e113_5df6_43be_e066_c78en,
      length: 4,
      ciphertext: 0x79f0_2af9n,
    },
  );
});

test('aes256SivDecrypt verifies the SIV before returning plaintext', async () => {
  const encrypted = await aes256SivEncrypt(0n, [0n, 1n, 2n], 0x1234n);

  assert.equal(await aes256SivDecrypt(0n, [0n, 1n, 2n], encrypted), 0x1234n);
  assert.equal(await aes256SivDecrypt(0n, [0n, 1n, 3n], encrypted), null);
  assert.equal(await aes256SivDecrypt(0n, [0n, 1n, 2n], {
    ...encrypted,
    ciphertext: encrypted.ciphertext ^ 1n,
  }), null);
});
