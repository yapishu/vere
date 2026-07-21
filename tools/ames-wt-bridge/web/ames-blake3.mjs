const IV = Uint32Array.of(
  0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
  0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
);

const MSG_PERMUTATION = Uint8Array.of(
  2, 6, 3, 10, 7, 0, 4, 13,
  1, 11, 12, 5, 9, 14, 15, 8,
);

const CHUNK_LEN = 1024;
const BLOCK_LEN = 64;

const FLAG_CHUNK_START = 1 << 0;
const FLAG_CHUNK_END = 1 << 1;
const FLAG_PARENT = 1 << 2;
const FLAG_ROOT = 1 << 3;
const FLAG_KEYED_HASH = 1 << 4;
const FLAG_DERIVE_KEY_CONTEXT = 1 << 5;
const FLAG_DERIVE_KEY_MATERIAL = 1 << 6;

function asBytes(value) {
  return value instanceof Uint8Array ? value : Uint8Array.from(value ?? []);
}

function rotr32(value, shift) {
  return ((value >>> shift) | (value << (32 - shift))) >>> 0;
}

function g(state, a, b, c, d, mx, my) {
  state[a] = (state[a] + state[b] + mx) >>> 0;
  state[d] = rotr32(state[d] ^ state[a], 16);
  state[c] = (state[c] + state[d]) >>> 0;
  state[b] = rotr32(state[b] ^ state[c], 12);
  state[a] = (state[a] + state[b] + my) >>> 0;
  state[d] = rotr32(state[d] ^ state[a], 8);
  state[c] = (state[c] + state[d]) >>> 0;
  state[b] = rotr32(state[b] ^ state[c], 7);
}

function round(state, msg) {
  g(state, 0, 4, 8, 12, msg[0], msg[1]);
  g(state, 1, 5, 9, 13, msg[2], msg[3]);
  g(state, 2, 6, 10, 14, msg[4], msg[5]);
  g(state, 3, 7, 11, 15, msg[6], msg[7]);
  g(state, 0, 5, 10, 15, msg[8], msg[9]);
  g(state, 1, 6, 11, 12, msg[10], msg[11]);
  g(state, 2, 7, 8, 13, msg[12], msg[13]);
  g(state, 3, 4, 9, 14, msg[14], msg[15]);
}

function permute(words) {
  const out = new Uint32Array(16);
  for (let i = 0; i < 16; i++) {
    out[i] = words[MSG_PERMUTATION[i]];
  }
  return out;
}

function wordsFromBlock(block) {
  const words = new Uint32Array(16);
  for (let i = 0; i < block.length; i++) {
    words[i >> 2] |= block[i] << ((i & 3) * 8);
  }
  return words;
}

function writeWordLE(out, off, word) {
  out[off] = word & 0xff;
  out[off + 1] = (word >>> 8) & 0xff;
  out[off + 2] = (word >>> 16) & 0xff;
  out[off + 3] = (word >>> 24) & 0xff;
}

function compress(cv, blockWords, counter, blockLen, flags) {
  const state = new Uint32Array(16);
  state.set(cv, 0);
  state.set(IV.subarray(0, 4), 8);
  state[12] = Number(counter & 0xffffffffn);
  state[13] = Number((counter >> 32n) & 0xffffffffn);
  state[14] = blockLen;
  state[15] = flags;

  let words = blockWords;
  for (let i = 0; i < 7; i++) {
    round(state, words);
    words = permute(words);
  }

  const out = new Uint32Array(16);
  for (let i = 0; i < 8; i++) {
    out[i] = (state[i] ^ state[i + 8]) >>> 0;
    out[i + 8] = (state[i + 8] ^ cv[i]) >>> 0;
  }
  return out;
}

function outputBytes(output, outLen = 32) {
  const out = new Uint8Array(outLen);
  let written = 0;
  let counter = 0n;

  while (written < outLen) {
    const block = compress(
      output.cv,
      output.blockWords,
      counter,
      output.blockLen,
      output.flags | FLAG_ROOT,
    );
    const blockBytes = new Uint8Array(64);
    for (let i = 0; i < 16; i++) {
      writeWordLE(blockBytes, i * 4, block[i]);
    }
    const take = Math.min(blockBytes.length, outLen - written);
    out.set(blockBytes.subarray(0, take), written);
    written += take;
    counter++;
  }

  return out;
}

function cvFromOutput(output) {
  return compress(output.cv, output.blockWords, output.counter, output.blockLen, output.flags).subarray(0, 8);
}

function keyWordsFromBytes(key) {
  const bytes = asBytes(key);
  if (bytes.length !== 32) {
    throw new Error('blake3 key must be 32 bytes');
  }

  return wordsFromBlock(bytes);
}

function chunkOutput(chunk, chunkCounter, { keyWords = IV, flags = 0 } = {}) {
  let cv = keyWords;
  let blockFlags = flags | FLAG_CHUNK_START;
  let block = new Uint8Array(BLOCK_LEN);
  let blockLen = 0;

  if (chunk.length === 0) {
    return {
      cv,
      blockWords: wordsFromBlock(block),
      counter: chunkCounter,
      blockLen: 0,
      flags: blockFlags | FLAG_CHUNK_END,
    };
  }

  for (let off = 0; off < chunk.length; off += BLOCK_LEN) {
    block = chunk.subarray(off, Math.min(off + BLOCK_LEN, chunk.length));
    blockLen = block.length;
    const last = off + BLOCK_LEN >= chunk.length;
    const blockWords = wordsFromBlock(block);
    if (last) {
      return {
        cv,
        blockWords,
        counter: chunkCounter,
        blockLen,
        flags: blockFlags | FLAG_CHUNK_END,
      };
    }

    cv = compress(cv, blockWords, chunkCounter, blockLen, blockFlags).subarray(0, 8);
    blockFlags = flags;
  }

  throw new Error('unreachable blake3 chunk state');
}

function parentOutput(leftCv, rightCv, { keyWords = IV, flags = 0 } = {}) {
  const blockWords = new Uint32Array(16);
  blockWords.set(leftCv, 0);
  blockWords.set(rightCv, 8);
  return {
    cv: keyWords,
    blockWords,
    counter: 0n,
    blockLen: 64,
    flags: flags | FLAG_PARENT,
  };
}

function rootOutput(outputs, options = {}) {
  if (outputs.length === 0) {
    return chunkOutput(new Uint8Array(), 0n, options);
  }
  if (outputs.length === 1) {
    return outputs[0];
  }

  const mid = 1 << (Math.ceil(Math.log2(outputs.length)) - 1);
  return parentOutput(
    cvFromOutput(rootOutput(outputs.slice(0, mid), options)),
    cvFromOutput(rootOutput(outputs.slice(mid), options)),
    options,
  );
}

function blake3HashWithOptions(input, outLen = 32, options = {}) {
  if (!Number.isSafeInteger(outLen) || outLen <= 0) {
    throw new Error('blake3 output length must be a positive safe integer');
  }

  const bytes = asBytes(input);
  const outputs = [];
  for (let off = 0, chunkCounter = 0n; off < bytes.length || outputs.length === 0; off += CHUNK_LEN, chunkCounter++) {
    outputs.push(chunkOutput(
      bytes.subarray(off, Math.min(off + CHUNK_LEN, bytes.length)),
      chunkCounter,
      options,
    ));
    if (bytes.length === 0) {
      break;
    }
  }

  return outputBytes(rootOutput(outputs, options), outLen);
}

export function blake3Hash(input, outLen = 32) {
  return blake3HashWithOptions(input, outLen);
}

export function blake3KeyedHash(key, input, outLen = 32) {
  return blake3HashWithOptions(input, outLen, {
    keyWords: keyWordsFromBytes(key),
    flags: FLAG_KEYED_HASH,
  });
}

export function blake3DeriveKey(context, seed, outLen = 32) {
  const contextBytes = typeof context === 'string'
    ? new TextEncoder().encode(context)
    : asBytes(context);
  const contextKey = blake3HashWithOptions(contextBytes, 32, {
    flags: FLAG_DERIVE_KEY_CONTEXT,
  });

  return blake3HashWithOptions(seed, outLen, {
    keyWords: keyWordsFromBytes(contextKey),
    flags: FLAG_DERIVE_KEY_MATERIAL,
  });
}

export function lssRoot(input) {
  return blake3Hash(input, 32);
}
