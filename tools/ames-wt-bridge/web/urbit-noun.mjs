const textEncoder = new TextEncoder();

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

export function bytesFromAtomLE(atom, length = byteLength(atom)) {
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

export function jamBytes(noun) {
  return bytesFromAtomLE(jam(noun));
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
