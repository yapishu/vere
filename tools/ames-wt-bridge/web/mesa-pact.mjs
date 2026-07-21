const MESA_VER = 1;
const MESA_COOKIE = 0x67e00200;

export const PACT = Object.freeze({
  PAGE: 1,
  PEEK: 2,
  POKE: 3,
});

export const HOP = Object.freeze({
  NONE: 0,
  SHORT: 1,
  LONG: 2,
  MANY: 3,
});

export const AUTH = Object.freeze({
  SIGN: 0,
  NONE: 1,
  HMAC: 2,
  PAIR: 3,
});

const textEncoder = new TextEncoder();
const textDecoder = new TextDecoder();

export function hexToBytes(hex) {
  const clean = hex.replace(/\s+/g, '');
  if (clean.length % 2 !== 0) {
    throw new Error('hex string has odd length');
  }

  const out = new Uint8Array(clean.length / 2);
  for (let i = 0; i < out.length; i++) {
    const byte = Number.parseInt(clean.slice(i * 2, i * 2 + 2), 16);
    if (Number.isNaN(byte)) {
      throw new Error('invalid hex byte');
    }
    out[i] = byte;
  }
  return out;
}

export function bytesToHex(bytes) {
  return Array.from(bytes, byte => byte.toString(16).padStart(2, '0')).join('');
}

function toBig(value, name) {
  if (typeof value === 'bigint') {
    return value;
  }
  if (typeof value === 'number' && Number.isSafeInteger(value)) {
    return BigInt(value);
  }
  if (typeof value === 'string' && value.trim()) {
    return BigInt(value.trim());
  }
  throw new Error(`${name} must be an unsigned integer`);
}

function toUint(value, name, max = 0xffffffff) {
  if (!Number.isSafeInteger(value) || value < 0 || value > max) {
    throw new Error(`${name} must be an unsigned integer <= ${max}`);
  }
  return value;
}

function met3(value) {
  let val = toBig(value, 'value');
  if (val < 0n) {
    throw new Error('value must be unsigned');
  }

  let bytes = 0;
  while (val) {
    bytes++;
    val >>= 8n;
  }
  return bytes;
}

function safeDec(value) {
  return value === 0 ? 0 : value - 1;
}

function chubTag(value) {
  const val = toBig(value, 'value');
  if (val <= 0xffn) {
    return 0;
  }
  if (val <= 0xffffn) {
    return 1;
  }
  if (val <= 0xffffffffn) {
    return 2;
  }
  if (val <= 0xffffffffffffffffn) {
    return 3;
  }
  throw new Error('value does not fit in c3_d');
}

function chubTagBytes(tag) {
  return 1 << tag;
}

function normalizeBytes(value, len, name) {
  if (value == null) {
    return new Uint8Array(len);
  }
  const out = value instanceof Uint8Array ? value : Uint8Array.from(value);
  if (out.length !== len) {
    throw new Error(`${name} must be ${len} bytes`);
  }
  return out;
}

function shipRankTag(ship) {
  if (ship >> 64n) {
    return 3;
  }
  if (ship >> 32n) {
    return 2;
  }
  if (ship >> 16n) {
    return 1;
  }
  return 0;
}

function normalizeName(name) {
  const ship = toBig(name.ship ?? 0n, 'ship');
  if (ship < 0n || ship > ((1n << 128n) - 1n)) {
    throw new Error('ship must fit in 128 bits');
  }

  const rift = toUint(name.rift ?? 0, 'rift');
  const bloq = toUint(name.bloq ?? name.boq ?? 0, 'bloq', 0xff);
  const init = Boolean(name.init ?? name.nit ?? false);
  const auth = Boolean(name.auth ?? false);
  const frag = toBig(name.frag ?? 0n, 'frag');
  if (frag < 0n || frag > 0xffffffffffffffffn) {
    throw new Error('frag must fit in 64 bits');
  }

  const pathBytes = typeof name.path === 'string'
    ? textEncoder.encode(name.path)
    : Uint8Array.from(name.path ?? []);
  if (pathBytes.length > 0xffff) {
    throw new Error('path is too long for mesa pact');
  }

  return { ship, rift, bloq, init, auth, frag, pathBytes };
}

function normalizeData(data) {
  const totalBytes = toBig(data.totalBytes ?? data.tob ?? 0n, 'totalBytes');
  if (totalBytes < 0n || totalBytes > 0xffffffffffffffffn) {
    throw new Error('totalBytes must fit in 64 bits');
  }

  const fragment = data.fragment instanceof Uint8Array
    ? data.fragment
    : Uint8Array.from(data.fragment ?? []);
  if (fragment.length > 0xffffffff) {
    throw new Error('fragment is too large');
  }

  const authType = data.auth?.type ?? data.authType ?? AUTH.NONE;
  switch (authType) {
    case AUTH.SIGN:
      return {
        totalBytes,
        auth: { type: AUTH.SIGN, signature: normalizeBytes(data.auth?.signature, 64, 'signature') },
        fragment,
      };
    case AUTH.HMAC:
      return {
        totalBytes,
        auth: { type: AUTH.HMAC, mac: normalizeBytes(data.auth?.mac, 16, 'mac') },
        fragment,
      };
    case AUTH.NONE:
      return { totalBytes, auth: { type: AUTH.NONE }, fragment };
    case AUTH.PAIR:
      return {
        totalBytes,
        auth: {
          type: AUTH.PAIR,
          hashes: [
            normalizeBytes(data.auth?.hashes?.[0], 32, 'hash[0]'),
            normalizeBytes(data.auth?.hashes?.[1], 32, 'hash[1]'),
          ],
        },
        fragment,
      };
    default:
      throw new Error(`unknown auth type ${authType}`);
  }
}

function pactType(pact) {
  if (typeof pact.type === 'number') {
    return pact.type;
  }
  switch (pact.type) {
    case 'page': return PACT.PAGE;
    case 'peek': return PACT.PEEK;
    case 'poke': return PACT.POKE;
    default: throw new Error(`unknown pact type ${pact.type}`);
  }
}

function pactTypeName(type) {
  switch (type) {
    case PACT.PAGE: return 'page';
    case PACT.PEEK: return 'peek';
    case PACT.POKE: return 'poke';
    default: return 'unknown';
  }
}

class Writer {
  constructor() {
    this.bytes = [];
    this.bitBuf = 0n;
    this.bitOff = 0;
  }

  ensureByteAligned() {
    if (this.bitOff !== 0) {
      throw new Error('writer is not byte-aligned');
    }
  }

  writeBits(width, value) {
    let val = toBig(value, 'bits');
    const mask = (1n << BigInt(width)) - 1n;
    this.bitBuf |= (val & mask) << BigInt(this.bitOff);
    this.bitOff += width;
    while (this.bitOff >= 8) {
      this.bytes.push(Number(this.bitBuf & 0xffn));
      this.bitBuf >>= 8n;
      this.bitOff -= 8;
    }
  }

  writeByte(value) {
    this.ensureByteAligned();
    this.bytes.push(value & 0xff);
  }

  writeShort(value) {
    this.ensureByteAligned();
    this.bytes.push(value & 0xff, (value >>> 8) & 0xff);
  }

  writeWord(value) {
    this.ensureByteAligned();
    const word = value >>> 0;
    this.bytes.push(
      word & 0xff,
      (word >>> 8) & 0xff,
      (word >>> 16) & 0xff,
      (word >>> 24) & 0xff,
    );
  }

  writeVarBig(value, len) {
    this.ensureByteAligned();
    let val = toBig(value, 'value');
    for (let i = 0; i < len; i++) {
      this.bytes.push(Number(val & 0xffn));
      val >>= 8n;
    }
  }

  writeBytes(bytes) {
    this.ensureByteAligned();
    this.bytes.push(...bytes);
  }

  finish() {
    this.ensureByteAligned();
    return new Uint8Array(this.bytes);
  }
}

class Reader {
  constructor(bytes) {
    this.bytes = bytes instanceof Uint8Array ? bytes : Uint8Array.from(bytes);
    this.pos = 0;
    this.bitBuf = 0n;
    this.bitOff = 0;
  }

  ensureByteAligned() {
    if (this.bitOff !== 0) {
      throw new Error('reader is not byte-aligned');
    }
  }

  readBits(width) {
    while (this.bitOff < width) {
      if (this.pos >= this.bytes.length) {
        throw new Error('unexpected end of packet');
      }
      this.bitBuf |= BigInt(this.bytes[this.pos++]) << BigInt(this.bitOff);
      this.bitOff += 8;
    }

    const mask = (1n << BigInt(width)) - 1n;
    const out = this.bitBuf & mask;
    this.bitBuf >>= BigInt(width);
    this.bitOff -= width;
    return Number(out);
  }

  readByte() {
    this.ensureByteAligned();
    if (this.pos >= this.bytes.length) {
      throw new Error('unexpected eof');
    }
    return this.bytes[this.pos++];
  }

  readShort() {
    const bytes = this.readBytes(2);
    return bytes[0] | (bytes[1] << 8);
  }

  readWord() {
    const bytes = this.readBytes(4);
    return (bytes[0]
      | (bytes[1] << 8)
      | (bytes[2] << 16)
      | (bytes[3] << 24)) >>> 0;
  }

  readVarBig(len) {
    this.ensureByteAligned();
    const bytes = this.readBytes(len);
    let out = 0n;
    for (let i = bytes.length - 1; i >= 0; i--) {
      out = (out << 8n) | BigInt(bytes[i]);
    }
    return out;
  }

  readBytes(len) {
    this.ensureByteAligned();
    if (this.pos + len > this.bytes.length) {
      throw new Error('unexpected eof');
    }
    const out = this.bytes.subarray(this.pos, this.pos + len);
    this.pos += len;
    return out;
  }
}

function writeHead(writer, head) {
  writer.writeBits(2, 0);
  writer.writeBits(2, head.next);
  writer.writeBits(3, MESA_VER);
  writer.writeBits(2, head.type);
  writer.writeBits(3, head.hop);
  writer.writeBits(20, head.mug);
  writer.writeWord(MESA_COOKIE);
}

function readHead(reader) {
  reader.readBits(2);
  const next = reader.readBits(2);
  const version = reader.readBits(3);
  const type = reader.readBits(2);
  const hop = reader.readBits(3);
  const mug = reader.readBits(20);
  if (version !== MESA_VER) {
    throw new Error('bad protocol');
  }
  if (reader.readWord() !== MESA_COOKIE) {
    throw new Error('bad cookie');
  }
  return { next, version, type, hop, mug };
}

function writeShip(writer, ship, len) {
  writer.writeVarBig(ship, len);
}

function readShip(reader, len) {
  return reader.readVarBig(len);
}

function writeName(writer, input) {
  const name = normalizeName(input);
  const rank = shipRankTag(name.ship);
  const riftLenTag = safeDec(met3(name.rift));
  const init = name.init ? 1 : 0;
  const auth = name.init ? 0 : (name.auth ? 1 : 0);
  const fragLenTag = name.init ? 0 : chubTag(name.frag);

  writer.writeBits(2, rank);
  writer.writeBits(2, riftLenTag);
  writer.writeBits(1, init);
  writer.writeBits(1, auth);
  writer.writeBits(2, fragLenTag);

  writeShip(writer, name.ship, 2 << rank);
  writer.writeVarBig(BigInt(name.rift), riftLenTag + 1);
  writer.writeByte(name.bloq);

  if (!name.init) {
    writer.writeVarBig(name.frag, chubTagBytes(fragLenTag));
  }

  writer.writeShort(name.pathBytes.length);
  writer.writeBytes(name.pathBytes);
}

function readName(reader) {
  const rank = reader.readBits(2);
  const riftLenTag = reader.readBits(2);
  const init = reader.readBits(1) === 1;
  const auth = reader.readBits(1) === 1;
  const fragLenTag = reader.readBits(2);

  if (init && (auth || fragLenTag !== 0)) {
    throw new Error('bad init name metadata');
  }

  const ship = readShip(reader, 2 << rank);
  const rift = Number(reader.readVarBig(riftLenTag + 1));
  const bloq = reader.readByte();
  const frag = init ? 0n : reader.readVarBig(chubTagBytes(fragLenTag));
  const pathBytes = reader.readBytes(reader.readShort());

  return {
    ship,
    rift,
    bloq,
    init,
    auth,
    frag,
    path: textDecoder.decode(pathBytes),
    pathBytes,
  };
}

function writeData(writer, input) {
  const data = normalizeData(input);
  const totalLenTag = chubTag(data.totalBytes);
  const isAuthValue = data.auth.type === AUTH.SIGN || data.auth.type === AUTH.HMAC;
  const authSelector = data.auth.type === AUTH.SIGN || data.auth.type === AUTH.NONE;
  const fragmentLenBytes = met3(data.fragment.length);
  const fragmentLenTag = Math.min(fragmentLenBytes, 3);

  writer.writeBits(2, totalLenTag);
  writer.writeBits(1, isAuthValue ? 1 : 0);
  writer.writeBits(1, authSelector ? 1 : 0);
  writer.writeBits(2, 0);
  writer.writeBits(2, fragmentLenTag);
  writer.writeVarBig(data.totalBytes, chubTagBytes(totalLenTag));

  switch (data.auth.type) {
    case AUTH.SIGN:
      writer.writeBytes(data.auth.signature);
      break;
    case AUTH.HMAC:
      writer.writeBytes(data.auth.mac);
      break;
    case AUTH.NONE:
      break;
    case AUTH.PAIR:
      writer.writeBytes(data.auth.hashes[0]);
      writer.writeBytes(data.auth.hashes[1]);
      break;
    default:
      throw new Error(`unknown auth type ${data.auth.type}`);
  }

  if (fragmentLenTag === 3) {
    writer.writeByte(fragmentLenBytes);
  }
  writer.writeVarBig(BigInt(data.fragment.length), fragmentLenBytes);
  writer.writeBytes(data.fragment);
}

function readData(reader) {
  const totalLenTag = reader.readBits(2);
  const isAuthValue = reader.readBits(1) === 1;
  const authSelector = reader.readBits(1) === 1;
  reader.readBits(2);
  const fragmentLenTag = reader.readBits(2);

  const totalBytes = reader.readVarBig(chubTagBytes(totalLenTag));
  let auth;
  if (isAuthValue) {
    auth = authSelector
      ? { type: AUTH.SIGN, signature: reader.readBytes(64) }
      : { type: AUTH.HMAC, mac: reader.readBytes(16) };
  }
  else {
    auth = authSelector
      ? { type: AUTH.NONE }
      : { type: AUTH.PAIR, hashes: [reader.readBytes(32), reader.readBytes(32)] };
  }

  const fragmentLenBytes = fragmentLenTag === 3 ? reader.readByte() : fragmentLenTag;
  const fragmentLen = Number(reader.readVarBig(fragmentLenBytes));
  const fragment = reader.readBytes(fragmentLen);

  return { totalBytes, auth, fragment };
}

function writeHops(writer, pact) {
  switch (pact.next ?? HOP.NONE) {
    case HOP.NONE:
      return;
    case HOP.SHORT:
      writer.writeBytes(normalizeBytes(pact.shortHop, 6, 'shortHop'));
      return;
    case HOP.LONG: {
      const hop = Uint8Array.from(pact.longHop ?? []);
      if (hop.length > 0xff) {
        throw new Error('longHop is too long');
      }
      writer.writeByte(hop.length);
      writer.writeBytes(hop);
      return;
    }
    case HOP.MANY: {
      const hops = pact.manyHops ?? [];
      if (hops.length > 0xff) {
        throw new Error('too many hops');
      }
      writer.writeByte(hops.length);
      for (const hopInput of hops) {
        const hop = Uint8Array.from(hopInput);
        if (hop.length > 0xff) {
          throw new Error('hop is too long');
        }
        writer.writeByte(hop.length);
        writer.writeBytes(hop);
      }
      return;
    }
    default:
      throw new Error(`unknown hop kind ${pact.next}`);
  }
}

function readHops(reader, next) {
  switch (next) {
    case HOP.NONE:
      return {};
    case HOP.SHORT:
      return { shortHop: reader.readBytes(6) };
    case HOP.LONG: {
      const len = reader.readByte();
      return { longHop: reader.readBytes(len) };
    }
    case HOP.MANY:
      throw new Error('mesa: sift invalid hop many');
    default:
      throw new Error(`unknown hop kind ${next}`);
  }
}

function writeBody(writer, pact, type) {
  switch (type) {
    case PACT.PEEK:
      writeName(writer, pact.name);
      return;
    case PACT.PAGE:
      writeName(writer, pact.name);
      writeData(writer, pact.data);
      writeHops(writer, pact);
      return;
    case PACT.POKE:
      writeName(writer, pact.name);
      writeName(writer, pact.payloadName);
      writeData(writer, pact.data);
      return;
    default:
      throw new Error(`unknown pact type ${type}`);
  }
}

function readBody(reader, type, next) {
  switch (type) {
    case PACT.PEEK:
      return { name: readName(reader) };
    case PACT.PAGE:
      return { name: readName(reader), data: readData(reader), ...readHops(reader, next) };
    case PACT.POKE:
      return { name: readName(reader), payloadName: readName(reader), data: readData(reader) };
    default:
      throw new Error(`unknown pact type ${type}`);
  }
}

export function encodePact(pact) {
  const type = pactType(pact);
  const bodyWriter = new Writer();
  writeBody(bodyWriter, pact, type);
  const body = bodyWriter.finish();

  const head = {
    next: toUint(pact.next ?? pact.head?.next ?? HOP.NONE, 'next', 3),
    type,
    hop: toUint(pact.hop ?? pact.head?.hop ?? 0, 'hop', 7),
    mug: mugBytes(body) & 0xfffff,
  };

  const writer = new Writer();
  writeHead(writer, head);
  writer.writeBytes(body);
  return writer.finish();
}

export function decodePact(bytes) {
  const input = bytes instanceof Uint8Array ? bytes : Uint8Array.from(bytes);
  const reader = new Reader(input);
  const head = readHead(reader);
  const bodyStart = reader.pos;
  const body = readBody(reader, head.type, head.next);

  if (reader.pos !== input.length) {
    throw new Error('trailing bytes');
  }

  const mug = mugBytes(input.subarray(bodyStart, reader.pos)) & 0xfffff;
  if (mug !== head.mug) {
    throw new Error('bad mug');
  }

  return {
    type: pactTypeName(head.type),
    typeCode: head.type,
    next: head.next,
    hop: head.hop,
    mug: head.mug,
    ...body,
  };
}

export function encodePeek({ ship = 0x100n, path, rift = 0, bloq = 0, frag = 0n, hop = 0 } = {}) {
  if (!path) {
    throw new Error('path is required');
  }
  return encodePact({
    type: 'peek',
    hop,
    name: { ship, rift, bloq, frag, path },
  });
}

export function mugBytes(bytes) {
  let seed = 0xcafebabe;
  for (let i = 0; i < 8; i++) {
    const raw = murmurHash3x86_32(bytes, seed);
    const mug = ((raw >>> 31) ^ (raw & 0x7fffffff)) >>> 0;
    if (mug !== 0) {
      return mug;
    }
    seed = (seed + 1) >>> 0;
  }
  return 0x7fff;
}

function rotl32(value, shift) {
  return ((value << shift) | (value >>> (32 - shift))) >>> 0;
}

function fmix32(value) {
  let h = value >>> 0;
  h ^= h >>> 16;
  h = Math.imul(h, 0x85ebca6b) >>> 0;
  h ^= h >>> 13;
  h = Math.imul(h, 0xc2b2ae35) >>> 0;
  h ^= h >>> 16;
  return h >>> 0;
}

export function murmurHash3x86_32(input, seed = 0) {
  const bytes = input instanceof Uint8Array ? input : Uint8Array.from(input ?? []);
  let h1 = seed >>> 0;
  const c1 = 0xcc9e2d51;
  const c2 = 0x1b873593;
  const blocks = Math.floor(bytes.length / 4);

  for (let i = 0; i < blocks; i++) {
    const off = i * 4;
    let k1 = (bytes[off]
      | (bytes[off + 1] << 8)
      | (bytes[off + 2] << 16)
      | (bytes[off + 3] << 24)) >>> 0;

    k1 = Math.imul(k1, c1) >>> 0;
    k1 = rotl32(k1, 15);
    k1 = Math.imul(k1, c2) >>> 0;

    h1 ^= k1;
    h1 = rotl32(h1, 13);
    h1 = (Math.imul(h1, 5) + 0xe6546b64) >>> 0;
  }

  let k1 = 0;
  const tail = blocks * 4;
  switch (bytes.length & 3) {
    case 3:
      k1 ^= bytes[tail + 2] << 16;
    case 2:
      k1 ^= bytes[tail + 1] << 8;
    case 1:
      k1 ^= bytes[tail];
      k1 = Math.imul(k1, c1) >>> 0;
      k1 = rotl32(k1, 15);
      k1 = Math.imul(k1, c2) >>> 0;
      h1 ^= k1;
  }

  h1 ^= bytes.length;
  return fmix32(h1);
}
