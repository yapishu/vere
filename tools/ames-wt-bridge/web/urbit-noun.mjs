const textEncoder = new TextEncoder();
const BYTE_ATOM = Symbol('urbit.byteAtom');

function asAtom(value) {
  const atom = BigInt(value);
  if (atom < 0n) {
    throw new Error('atom must be non-negative');
  }
  return atom;
}

function bitLength(value) {
  let atom = asAtom(value);
  let bits = 0;
  while (atom > 0n) {
    bits++;
    atom >>= 1n;
  }
  return bits;
}

function byteLength(value) {
  const bits = bitLength(value);
  return Math.ceil(bits / 8);
}

export function atomFromBytesLE(bytes) {
  let out = 0n;
  for (let i = bytes.length - 1; i >= 0; i--) {
    out = (out << 8n) | BigInt(bytes[i]);
  }
  return out;
}

export function atomBytesLE(bytes) {
  return {
    [BYTE_ATOM]: true,
    bytes: Uint8Array.from(bytes),
  };
}

export function bytesFromAtomLE(atom, length = isByteAtom(atom) ? atom.bytes.length : byteLength(atom)) {
  if (isByteAtom(atom)) {
    if (atom.bytes.length > length) {
      for (let i = length; i < atom.bytes.length; i++) {
        if (atom.bytes[i] !== 0) {
          throw new Error(`atom does not fit in ${length} bytes`);
        }
      }
    }
    const out = new Uint8Array(length);
    out.set(atom.bytes.subarray(0, Math.min(length, atom.bytes.length)));
    return out;
  }

  let value = asAtom(atom);
  if ((value >> BigInt(length * 8)) !== 0n) {
    throw new Error(`atom does not fit in ${length} bytes`);
  }

  const out = new Uint8Array(length);
  for (let i = 0; i < length; i++) {
    out[i] = Number(value & 0xffn);
    value >>= 8n;
  }
  return out;
}

export function termAtom(text) {
  return atomFromBytesLE(textEncoder.encode(text));
}

export function cell(head, tail) {
  return [head, tail];
}

export function tuple(...items) {
  if (items.length < 2) {
    throw new Error('tuple requires at least two items');
  }

  let out = items.at(-1);
  for (let i = items.length - 2; i >= 0; i--) {
    out = cell(items[i], out);
  }
  return out;
}

export function list(...items) {
  let out = 0n;
  for (let i = items.length - 1; i >= 0; i--) {
    out = cell(items[i], out);
  }
  return out;
}

function isCell(noun) {
  return Array.isArray(noun) && noun.length === 2;
}

function isByteAtom(noun) {
  return Boolean(noun?.[BYTE_ATOM]);
}

function bitLengthBytesLE(bytes) {
  for (let i = bytes.length - 1; i >= 0; i--) {
    const byte = bytes[i];
    if (byte !== 0) {
      return (i * 8) + (32 - Math.clz32(byte));
    }
  }
  return 0;
}

function bitLengthNumber(value) {
  if (!Number.isSafeInteger(value) || value < 0) {
    throw new Error('bit length input must be a non-negative safe integer');
  }
  return value === 0 ? 0 : value.toString(2).length;
}

class BitWriter {
  constructor() {
    this.bytes = [];
    this.current = 0;
    this.offset = 0;
  }

  writeBit(bit) {
    if (bit) {
      this.current |= 1 << this.offset;
    }
    this.offset++;
    if (this.offset === 8) {
      this.bytes.push(this.current);
      this.current = 0;
      this.offset = 0;
    }
  }

  writeBitsBigInt(value, width) {
    let atom = asAtom(value);
    for (let i = 0; i < width; i++) {
      this.writeBit(Number(atom & 1n));
      atom >>= 1n;
    }
  }

  writeZeroBits(width) {
    for (let i = 0; i < width; i++) {
      this.writeBit(0);
    }
  }

  writeBitsBytesLE(bytes, width) {
    for (let i = 0; i < width; i++) {
      this.writeBit((bytes[i >> 3] >> (i & 7)) & 1);
    }
  }

  finish() {
    if (this.offset > 0) {
      this.bytes.push(this.current);
      this.current = 0;
      this.offset = 0;
    }
    return Uint8Array.from(this.bytes);
  }
}

function mat(atom) {
  const value = asAtom(atom);
  if (value === 0n) {
    return { bits: 1, value: 1n };
  }

  const len = bitLength(value);
  const lenBits = bitLength(BigInt(len));
  const lowerLenMask = (1n << BigInt(lenBits - 1)) - 1n;
  const lowerLen = BigInt(len) & lowerLenMask;
  const rest = lowerLen | (value << BigInt(lenBits - 1));
  const prefix = 1n << BigInt(lenBits);

  return {
    bits: (2 * lenBits) + len,
    value: prefix | (rest << BigInt(lenBits + 1)),
  };
}

function jamBits(noun) {
  if (!isCell(noun)) {
    const encoded = mat(noun);
    return {
      bits: encoded.bits + 1,
      value: encoded.value << 1n,
    };
  }

  const head = jamBits(noun[0]);
  const tail = jamBits(noun[1]);
  const body = head.value | (tail.value << BigInt(head.bits));

  return {
    bits: 2 + head.bits + tail.bits,
    value: 1n | (body << 2n),
  };
}

export function jam(noun) {
  return jamBits(noun).value;
}

function writeMatBigInt(writer, atom) {
  const value = asAtom(atom);
  if (value === 0n) {
    writer.writeBit(1);
    return;
  }

  const len = bitLength(value);
  const lenBits = bitLength(BigInt(len));
  const lowerLenMask = (1n << BigInt(lenBits - 1)) - 1n;
  const lowerLen = BigInt(len) & lowerLenMask;

  writer.writeZeroBits(lenBits);
  writer.writeBit(1);
  writer.writeBitsBigInt(lowerLen, lenBits - 1);
  writer.writeBitsBigInt(value, len);
}

function writeMatBytes(writer, bytes) {
  const len = bitLengthBytesLE(bytes);
  if (len === 0) {
    writer.writeBit(1);
    return;
  }

  const lenBits = bitLengthNumber(len);
  const lowerLenMask = (1n << BigInt(lenBits - 1)) - 1n;
  const lowerLen = BigInt(len) & lowerLenMask;

  writer.writeZeroBits(lenBits);
  writer.writeBit(1);
  writer.writeBitsBigInt(lowerLen, lenBits - 1);
  writer.writeBitsBytesLE(bytes, len);
}

function writeJamBytes(writer, noun) {
  if (isByteAtom(noun)) {
    writer.writeBit(0);
    writeMatBytes(writer, noun.bytes);
    return;
  }
  if (!isCell(noun)) {
    writer.writeBit(0);
    writeMatBigInt(writer, noun);
    return;
  }

  writer.writeBit(1);
  writer.writeBit(0);
  writeJamBytes(writer, noun[0]);
  writeJamBytes(writer, noun[1]);
}

export function jamBytes(noun) {
  const writer = new BitWriter();
  writeJamBytes(writer, noun);
  return writer.finish();
}

class BitReader {
  constructor(atom) {
    this.atom = asAtom(atom);
    this.pos = 0;
  }

  readBit() {
    const out = Number((this.atom >> BigInt(this.pos)) & 1n);
    this.pos++;
    return out;
  }

  readBits(width) {
    let out = 0n;
    for (let i = 0; i < width; i++) {
      out |= BigInt(this.readBit()) << BigInt(i);
    }
    return out;
  }

  rub() {
    let zeros = 0;
    while (this.readBit() === 0) {
      zeros++;
    }

    if (zeros === 0) {
      return 0n;
    }

    const lower = zeros === 1 ? 0n : this.readBits(zeros - 1);
    const len = Number(lower | (1n << BigInt(zeros - 1)));
    return this.readBits(len);
  }
}

class ByteBitReader {
  constructor(bytes, {
    byteAtomBitThreshold = Infinity,
  } = {}) {
    this.bytes = bytes instanceof Uint8Array
      ? bytes
      : new Uint8Array(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    this.byteAtomBitThreshold = byteAtomBitThreshold;
    this.pos = 0;
  }

  readBit() {
    const out = (this.bytes[this.pos >> 3] >> (this.pos & 7)) & 1;
    this.pos++;
    return out;
  }

  readBits(width) {
    let out = 0n;
    for (let i = 0; i < width; i++) {
      out |= BigInt(this.readBit()) << BigInt(i);
    }
    return out;
  }

  readBitsBytesLE(width) {
    const length = Math.ceil(width / 8);
    const out = new Uint8Array(length);
    const byteOffset = this.pos >> 3;
    const bitOffset = this.pos & 7;

    if (bitOffset === 0) {
      out.set(this.bytes.subarray(byteOffset, byteOffset + length));
    }
    else {
      for (let i = 0; i < length; i++) {
        const lower = this.bytes[byteOffset + i] >> bitOffset;
        const upper = (this.bytes[byteOffset + i + 1] ?? 0) << (8 - bitOffset);
        out[i] = (lower | upper) & 0xff;
      }
    }

    const extraBits = width & 7;
    if (extraBits !== 0) {
      out[length - 1] &= (1 << extraBits) - 1;
    }
    this.pos += width;
    return out;
  }

  rub() {
    let zeros = 0;
    while (this.readBit() === 0) {
      zeros++;
    }

    if (zeros === 0) {
      return 0n;
    }

    const lower = zeros === 1 ? 0n : this.readBits(zeros - 1);
    const len = Number(lower | (1n << BigInt(zeros - 1)));
    if (len >= this.byteAtomBitThreshold) {
      return atomBytesLE(this.readBitsBytesLE(len));
    }
    return this.readBits(len);
  }
}

function cueWith(reader, refs) {
  const start = reader.pos;
  let noun;

  if (reader.readBit() === 0) {
    noun = reader.rub();
  }
  else if (reader.readBit() === 0) {
    noun = cell(cueWith(reader, refs), cueWith(reader, refs));
  }
  else {
    const ref = Number(reader.rub());
    if (!refs.has(ref)) {
      throw new Error(`cue invalid backref ${ref}`);
    }
    noun = refs.get(ref);
  }

  refs.set(start, noun);
  return noun;
}

export function cue(atom) {
  return cueWith(new BitReader(atom), new Map());
}

export function cueBytes(bytes, options = {}) {
  return cueWith(new ByteBitReader(bytes, options), new Map());
}
