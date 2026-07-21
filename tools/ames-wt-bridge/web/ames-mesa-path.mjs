import { patp, patpToAtom } from './ames-ship.mjs';

const textEncoder = new TextEncoder();

function asAtom(value, name = 'atom') {
  const atom = BigInt(value);
  if (atom < 0n) {
    throw new Error(`${name} must be non-negative`);
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

function asShip(value, name) {
  const ship = asAtom(value, name);
  if (ship > ((1n << 128n) - 1n)) {
    throw new Error(`${name} must fit in 128 bits`);
  }
  return ship;
}

function stripPercent(value, name) {
  const text = String(value ?? '');
  const out = text.startsWith('%') ? text.slice(1) : text;
  if (!/^[a-z][a-z0-9-]*$/.test(out)) {
    throw new Error(`${name} must be a term-like string`);
  }
  return out;
}

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

export function normalizePath(path) {
  if (typeof path !== 'string' || path.length === 0) {
    throw new Error('path is required');
  }
  return path.startsWith('/') ? path : `/${path}`;
}

export function canonicalPatp(value, name = 'ship') {
  if (typeof value === 'string') {
    return patp(patpToAtom(value));
  }
  return patp(asShip(value, name));
}

export function mesaPathBytes(path) {
  return textEncoder.encode(normalizePath(path));
}

export function mesaFlowPath({
  bone,
  load,
  direction,
  receiver,
  sequence,
} = {}) {
  const loadText = stripPercent(load, 'load');
  const directionText = stripPercent(direction, 'direction');
  if (directionText !== 'for' && directionText !== 'bak') {
    throw new Error('direction must be %for or %bak');
  }

  const prefix = `/a/x/1//flow/${asAtom(bone ?? 0n, 'bone')}/${loadText}/${directionText}/${canonicalPatp(receiver, 'receiver')}`;
  if (loadText === 'cork') {
    return prefix;
  }
  return `${prefix}/${asAtom(sequence ?? 1n, 'sequence')}`;
}

export function mesaBeamPath({
  publisher,
  publisherPatp,
  caseNumber = 1n,
  path,
}) {
  const who = publisherPatp === undefined
    ? canonicalPatp(publisher, 'publisher')
    : canonicalPatp(publisherPatp, 'publisherPatp');
  return `/${who}//${asAtom(caseNumber, 'caseNumber')}${normalizePath(path)}`;
}

export function mesaBindingBytes({ publisher, publisherPatp, caseNumber = 1n, path, root }) {
  const rootBytes = root instanceof Uint8Array ? root : bytesFromAtomLE(root, 32);
  if (rootBytes.length !== 32) {
    throw new Error('binding root must be 32 bytes');
  }
  return concatBytes(
    mesaPathBytes(mesaBeamPath({ publisher, publisherPatp, caseNumber, path })),
    rootBytes,
  );
}
