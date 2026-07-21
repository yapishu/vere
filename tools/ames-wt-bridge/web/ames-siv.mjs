function subtleCrypto(subtle) {
  const out = subtle ?? globalThis.crypto?.subtle;
  if (!out) {
    throw new Error('WebCrypto subtle crypto is not available');
  }
  return out;
}

function asBytes(value) {
  return value instanceof Uint8Array ? value : Uint8Array.from(value);
}

function asAtom(value) {
  const atom = BigInt(value);
  if (atom < 0n) {
    throw new Error('atom must be non-negative');
  }
  return atom;
}

function atomByteLength(value) {
  let atom = asAtom(value);
  let length = 0;
  while (atom > 0n) {
    length++;
    atom >>= 8n;
  }
  return length;
}

export function bytesFromAtomBE(value, length = atomByteLength(value)) {
  let atom = asAtom(value);
  const bytes = new Uint8Array(length);

  for (let i = length - 1; i >= 0; i--) {
    bytes[i] = Number(atom & 0xffn);
    atom >>= 8n;
  }
  if (atom !== 0n) {
    throw new Error(`atom does not fit in ${length} bytes`);
  }
  return bytes;
}

export function atomFromBytesBE(bytes) {
  let out = 0n;
  for (const byte of asBytes(bytes)) {
    out = (out << 8n) | BigInt(byte);
  }
  return out;
}

function xorBytes(a, b) {
  const out = new Uint8Array(a.length);
  for (let i = 0; i < out.length; i++) {
    out[i] = a[i] ^ b[i];
  }
  return out;
}

function doubleBlock(block) {
  const out = new Uint8Array(16);
  let carry = 0;

  for (let i = 15; i >= 0; i--) {
    const val = (block[i] << 1) | carry;
    out[i] = val & 0xff;
    carry = (block[i] >>> 7) & 1;
  }
  if ((block[0] & 0x80) !== 0) {
    out[15] ^= 0x87;
  }
  return out;
}

function cmacPad(bytes) {
  const out = new Uint8Array(16);
  out.set(bytes);
  out[bytes.length] = 0x80;
  return out;
}

async function aesCbcFirstBlock(key, block, { subtle } = {}) {
  if (block.length !== 16) {
    throw new Error('AES block must be 16 bytes');
  }

  const crypto = subtleCrypto(subtle);
  const aesKey = await crypto.importKey('raw', key, 'AES-CBC', false, ['encrypt']);
  const encrypted = new Uint8Array(await crypto.encrypt(
    { name: 'AES-CBC', iv: new Uint8Array(16) },
    aesKey,
    block,
  ));
  return encrypted.slice(0, 16);
}

async function aesCtr(key, counter, input, { subtle } = {}) {
  if (counter.length !== 16) {
    throw new Error('AES-CTR counter must be 16 bytes');
  }

  const crypto = subtleCrypto(subtle);
  const aesKey = await crypto.importKey('raw', key, 'AES-CTR', false, ['encrypt', 'decrypt']);
  return new Uint8Array(await crypto.encrypt(
    { name: 'AES-CTR', counter, length: 32 },
    aesKey,
    input,
  ));
}

export async function aes256CmacBytes(keyBytes, messageBytes, { subtle } = {}) {
  const key = asBytes(keyBytes);
  const message = asBytes(messageBytes);
  if (key.length !== 32) {
    throw new Error('AES-256 CMAC key must be 32 bytes');
  }

  const zero = new Uint8Array(16);
  const subkey1 = doubleBlock(await aesCbcFirstBlock(key, zero, { subtle }));
  const subkey2 = doubleBlock(subkey1);
  let blockCount = Math.ceil(message.length / 16);
  let lastBlock;

  if (blockCount > 0 && (message.length % 16) === 0) {
    lastBlock = xorBytes(message.slice((blockCount - 1) * 16, blockCount * 16), subkey1);
  }
  else {
    if (blockCount === 0) {
      blockCount = 1;
    }
    lastBlock = xorBytes(cmacPad(message.slice((blockCount - 1) * 16)), subkey2);
  }

  let state = zero;
  for (let i = 0; i < blockCount - 1; i++) {
    state = await aesCbcFirstBlock(
      key,
      xorBytes(state, message.slice(i * 16, (i + 1) * 16)),
      { subtle },
    );
  }
  return aesCbcFirstBlock(key, xorBytes(state, lastBlock), { subtle });
}

export async function aes256CmacAtom(key, message, { length, subtle } = {}) {
  const keyBytes = key instanceof Uint8Array ? key : bytesFromAtomBE(key, 32);
  const msgBytes = bytesFromAtomBE(message, length ?? atomByteLength(message));
  return atomFromBytesBE(await aes256CmacBytes(keyBytes, msgBytes, { subtle }));
}

function atomToS2vBytes(value) {
  return bytesFromAtomBE(value);
}

export async function aes256S2v(key, associatedData, { subtle } = {}) {
  const keyBytes = key instanceof Uint8Array ? key : bytesFromAtomBE(key, 32);
  const items = associatedData.map(atomToS2vBytes);

  if (items.length === 0) {
    return atomFromBytesBE(await aes256CmacBytes(keyBytes, bytesFromAtomBE(1n, 16), { subtle }));
  }

  let state = await aes256CmacBytes(keyBytes, bytesFromAtomBE(0n, 16), { subtle });
  for (let i = 0; i < items.length - 1; i++) {
    state = xorBytes(
      doubleBlock(state),
      await aes256CmacBytes(keyBytes, items[i], { subtle }),
    );
  }

  const last = items[items.length - 1];
  let finalInput;
  if (last.length >= 16) {
    finalInput = new Uint8Array(last);
    const off = finalInput.length - 16;
    for (let i = 0; i < 16; i++) {
      finalInput[off + i] ^= state[i];
    }
  }
  else {
    finalInput = xorBytes(doubleBlock(state), cmacPad(last));
  }

  return atomFromBytesBE(await aes256CmacBytes(keyBytes, finalInput, { subtle }));
}

function splitSivcKey(key) {
  const keyBytes = key instanceof Uint8Array ? asBytes(key) : bytesFromAtomBE(key, 64);
  if (keyBytes.length !== 64) {
    throw new Error('AES-256 SIV key must be 64 bytes');
  }
  return {
    macKey: keyBytes.slice(0, 32),
    ctrKey: keyBytes.slice(32, 64),
  };
}

function sivCtrCounter(iv) {
  const bytes = bytesFromAtomBE(iv, 16);
  // RFC 5297 clears the 31st and 63rd bits before using the SIV as the CTR IV.
  bytes[8] &= 0x7f;
  bytes[12] &= 0x7f;
  return bytes;
}

export async function aes256SivEncrypt(key, associatedData, plaintext, { subtle } = {}) {
  const { macKey, ctrKey } = splitSivcKey(key);
  const plainLength = atomByteLength(plaintext);
  const plainBytes = bytesFromAtomBE(plaintext, plainLength);
  const iv = await aes256S2v(macKey, [...associatedData, plaintext], { subtle });
  const ciphertext = atomFromBytesBE(
    await aesCtr(ctrKey, sivCtrCounter(iv), plainBytes, { subtle }),
  );

  return {
    iv,
    length: plainLength,
    ciphertext,
  };
}

export async function aes256SivDecrypt(key, associatedData, {
  iv,
  length,
  ciphertext,
}, { subtle } = {}) {
  const { macKey, ctrKey } = splitSivcKey(key);
  const cipherBytes = bytesFromAtomBE(ciphertext, length);
  const plainBytes = await aesCtr(ctrKey, sivCtrCounter(iv), cipherBytes, { subtle });
  const plaintext = atomFromBytesBE(plainBytes);
  const check = await aes256S2v(macKey, [...associatedData, plaintext], { subtle });

  if (check !== BigInt(iv)) {
    return null;
  }
  return plaintext;
}
