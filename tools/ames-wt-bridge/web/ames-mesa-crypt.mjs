import { blake3DeriveKey, blake3KeyedHash } from './ames-blake3.mjs';
import { mesaPathBytes } from './ames-mesa-path.mjs';
import {
  atomFromBytesBE,
  bytesFromAtomBE,
} from './ames-siv.mjs';

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

function bytesFromAtomLE(value, length = atomByteLength(value)) {
  let atom = asAtom(value);
  const bytes = new Uint8Array(length);
  for (let i = 0; i < length; i++) {
    bytes[i] = Number(atom & 0xffn);
    atom >>= 8n;
  }
  if (atom !== 0n) {
    throw new Error(`atom does not fit in ${length} bytes`);
  }
  return bytes;
}

function atomFromBytesLE(bytes) {
  let out = 0n;
  for (let i = bytes.length - 1; i >= 0; i--) {
    out = (out << 8n) | BigInt(bytes[i]);
  }
  return out;
}

function u32(value) {
  return value >>> 0;
}

function rotl32(value, shift) {
  return ((value << shift) | (value >>> (32 - shift))) >>> 0;
}

function readWordLE(bytes, offset) {
  return (
    bytes[offset] |
    (bytes[offset + 1] << 8) |
    (bytes[offset + 2] << 16) |
    (bytes[offset + 3] << 24)
  ) >>> 0;
}

function writeWordLE(bytes, offset, value) {
  bytes[offset] = value & 0xff;
  bytes[offset + 1] = (value >>> 8) & 0xff;
  bytes[offset + 2] = (value >>> 16) & 0xff;
  bytes[offset + 3] = (value >>> 24) & 0xff;
}

function wordsFromBytesLE(bytes) {
  const out = [];
  for (let i = 0; i < bytes.length; i += 4) {
    out.push(readWordLE(bytes, i));
  }
  return out;
}

function bytesFromWordsLE(words) {
  const out = new Uint8Array(words.length * 4);
  for (let i = 0; i < words.length; i++) {
    writeWordLE(out, i * 4, words[i]);
  }
  return out;
}

const SIGMA = wordsFromBytesLE(new TextEncoder().encode('expand 32-byte k'));
const textDecoder = new TextDecoder();
const UV_DIGITS = '0123456789abcdefghijklmnopqrstuv';

function concatBytes(...chunks) {
  const total = chunks.reduce((acc, chunk) => acc + chunk.length, 0);
  const out = new Uint8Array(total);
  let off = 0;
  for (const chunk of chunks) {
    out.set(chunk, off);
    off += chunk.length;
  }
  return out;
}

function constantBytesEqual(a, b) {
  if (a.length !== b.length) {
    return false;
  }

  let diff = 0;
  for (let i = 0; i < a.length; i++) {
    diff |= a[i] ^ b[i];
  }
  return diff === 0;
}

function keyBytes32(key) {
  const keyBytes = key instanceof Uint8Array ? key : bytesFromAtomLE(key, 32);
  if (keyBytes.length !== 32) {
    throw new Error('Mesa key must be 32 bytes');
  }
  return keyBytes;
}

export function scotUv(value) {
  let atom = asAtom(value);
  let digits = '';
  do {
    digits = UV_DIGITS[Number(atom & 31n)] + digits;
    atom >>= 5n;
  } while (atom > 0n);

  const groups = [];
  for (let end = digits.length; end > 0; end -= 5) {
    groups.unshift(digits.slice(Math.max(0, end - 5), end));
  }
  return `~.0v${groups.join('.')}`;
}

export function slawUv(text) {
  if (typeof text !== 'string' || !text.startsWith('~.0v')) {
    throw new Error('uv atom must start with ~.0v');
  }

  const body = text.slice(4);
  if (body.length === 0 || body.startsWith('.') || body.endsWith('.') || body.includes('..')) {
    throw new Error('invalid uv atom');
  }

  let atom = 0n;
  for (const ch of body.replaceAll('.', '')) {
    const digit = UV_DIGITS.indexOf(ch);
    if (digit < 0) {
      throw new Error('invalid uv digit');
    }
    atom = (atom << 5n) | BigInt(digit);
  }
  return atom;
}

function quarterRound(state, a, b, c, d) {
  state[a] = u32(state[a] + state[b]);
  state[d] = rotl32(state[d] ^ state[a], 16);
  state[c] = u32(state[c] + state[d]);
  state[b] = rotl32(state[b] ^ state[c], 12);
  state[a] = u32(state[a] + state[b]);
  state[d] = rotl32(state[d] ^ state[a], 8);
  state[c] = u32(state[c] + state[d]);
  state[b] = rotl32(state[b] ^ state[c], 7);
}

function chachaRounds(inputState, rounds) {
  const state = inputState.slice();
  for (let i = 0; i < rounds; i += 2) {
    quarterRound(state, 0, 4, 8, 12);
    quarterRound(state, 1, 5, 9, 13);
    quarterRound(state, 2, 6, 10, 14);
    quarterRound(state, 3, 7, 11, 15);
    quarterRound(state, 0, 5, 10, 15);
    quarterRound(state, 1, 6, 11, 12);
    quarterRound(state, 2, 7, 8, 13);
    quarterRound(state, 3, 4, 9, 14);
  }
  return state;
}

function chachaBlock(keyBytes, nonceBytes, counter, rounds = 8) {
  if (keyBytes.length !== 32) {
    throw new Error('ChaCha key must be 32 bytes');
  }
  if (nonceBytes.length !== 8) {
    throw new Error('ChaCha nonce must be 8 bytes');
  }

  const keyWords = wordsFromBytesLE(keyBytes);
  const nonceWords = wordsFromBytesLE(nonceBytes);
  const state = [
    ...SIGMA,
    ...keyWords,
    Number(BigInt(counter) & 0xffffffffn),
    Number((BigInt(counter) >> 32n) & 0xffffffffn),
    ...nonceWords,
  ];
  const mixed = chachaRounds(state, rounds);
  const outWords = mixed.map((word, i) => u32(word + state[i]));
  return bytesFromWordsLE(outWords);
}

export function xchacha8(key, nonce) {
  const keyBytes = key instanceof Uint8Array ? key : bytesFromAtomLE(key, 32);
  const nonceBytes = nonce instanceof Uint8Array ? nonce : bytesFromAtomLE(nonce, 24);
  if (nonceBytes.length !== 24) {
    throw new Error('XChaCha nonce must be 24 bytes');
  }

  const keyWords = wordsFromBytesLE(keyBytes);
  const nonceWords = wordsFromBytesLE(nonceBytes.slice(0, 16));
  const state = [
    ...SIGMA,
    ...keyWords,
    ...nonceWords,
  ];
  const mixed = chachaRounds(state, 8);
  return {
    keyBytes: bytesFromWordsLE([
      mixed[0], mixed[1], mixed[2], mixed[3],
      mixed[12], mixed[13], mixed[14], mixed[15],
    ]),
    nonceBytes: nonceBytes.slice(16, 24),
  };
}

export function chacha8(key, nonce, counter, message) {
  const keyBytes = key instanceof Uint8Array ? key : bytesFromAtomLE(key, 32);
  const nonceBytes = nonce instanceof Uint8Array ? nonce : bytesFromAtomLE(nonce, 8);
  const msgBytes = message instanceof Uint8Array ? message : bytesFromAtomLE(message);
  const out = new Uint8Array(msgBytes.length);

  for (let off = 0, block = 0n; off < msgBytes.length; off += 64, block++) {
    const stream = chachaBlock(keyBytes, nonceBytes, BigInt(counter) + block, 8);
    const take = Math.min(64, msgBytes.length - off);
    for (let i = 0; i < take; i++) {
      out[off + i] = msgBytes[off + i] ^ stream[i];
    }
  }
  return out;
}

export function mesaCrypt(key, ivBytes, messageBytes) {
  const keyBytes = keyBytes32(key);
  const nonce = blake3DeriveKey('mesa-crypt-iv', ivBytes, 24);
  const expanded = xchacha8(keyBytes, nonce);
  return chacha8(expanded.keyBytes, expanded.nonceBytes, 0n, messageBytes);
}

export function mesaSealPath(key, path) {
  const keys = blake3DeriveKey('mesa-aead', keyBytes32(key), 64);
  const cryptKey = keys.slice(0, 32);
  const macKey = keys.slice(32, 64);
  const pathBytes = mesaPathBytes(path);
  const tag = blake3KeyedHash(macKey, pathBytes, 16);
  const cipher = mesaCrypt(cryptKey, tag, pathBytes);
  return atomFromBytesLE(concatBytes(tag, cipher, Uint8Array.of(0x01)));
}

export function mesaOpenPath(key, sealed) {
  const sealedBytes = sealed instanceof Uint8Array ? sealed : bytesFromAtomLE(sealed);
  if (sealedBytes.length < 17 || sealedBytes.at(-1) !== 0x01) {
    throw new Error('sealed path is missing marker byte');
  }

  const keys = blake3DeriveKey('mesa-aead', keyBytes32(key), 64);
  const cryptKey = keys.slice(0, 32);
  const macKey = keys.slice(32, 64);
  const tag = sealedBytes.slice(0, 16);
  const cipher = sealedBytes.slice(16, -1);
  const pathBytes = mesaCrypt(cryptKey, tag, cipher);
  const check = blake3KeyedHash(macKey, pathBytes, 16);
  if (!constantBytesEqual(tag, check)) {
    throw new Error('sealed path authentication failed');
  }

  return textDecoder.decode(pathBytes);
}

export function mesaEncryptBytes(key, iv, messageBytes) {
  const ivBytes = iv instanceof Uint8Array ? iv : bytesFromAtomLE(iv);
  return concatBytes(mesaCrypt(key, ivBytes, messageBytes), Uint8Array.of(0x01));
}

export function mesaDecryptBytes(key, iv, ciphertext) {
  const ivBytes = iv instanceof Uint8Array ? iv : bytesFromAtomLE(iv);
  const cipherBytes = ciphertext instanceof Uint8Array ? ciphertext : bytesFromAtomLE(ciphertext);
  if (cipherBytes.length === 0 || cipherBytes.at(-1) !== 0x01) {
    throw new Error('Mesa ciphertext is missing marker byte');
  }
  return mesaCrypt(key, ivBytes, cipherBytes.slice(0, -1));
}

export function mesaMacBinding(key, bindingBytes) {
  return blake3KeyedHash(keyBytes32(key), bindingBytes, 16);
}

export function mesaCryptAtom(key, iv, message) {
  const ivBytes = iv instanceof Uint8Array ? iv : bytesFromAtomBE(iv);
  const msgBytes = message instanceof Uint8Array ? message : bytesFromAtomBE(message);
  return atomFromBytesBE(mesaCrypt(key, ivBytes, msgBytes));
}

export function bytesFromHoonCryptoAtom(atom, length = atomByteLength(atom)) {
  return bytesFromAtomLE(atom, length);
}

export function hoonCryptoAtomFromBytes(bytes) {
  return atomFromBytesLE(bytes);
}
