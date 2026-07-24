import { termAtom, tuple } from './urbit-noun.mjs';

const MAGIC = Uint8Array.of(0x41, 0x55, 0x44, 0x50); // "AUDP"
const HEADER_BYTES = 8;

export const UDP_FRAME = Object.freeze({
  SEND: 1,
  HEAR: 2,
});

export const UDP_LANE = Object.freeze({
  GALAXY: 1,
  IPV4: 2,
});

function asBytes(input) {
  if (input instanceof Uint8Array) return input;
  if (input instanceof ArrayBuffer) return new Uint8Array(input);
  if (ArrayBuffer.isView(input)) {
    return new Uint8Array(input.buffer, input.byteOffset, input.byteLength);
  }
  return Uint8Array.from(input);
}

function uint(value, max, name) {
  const number = Number(value);
  if (!Number.isInteger(number) || number < 0 || number > max) {
    throw new Error(`${name} must be an integer between 0 and ${max}`);
  }
  return number;
}

export function normalizeUdpLane(lane) {
  if (typeof lane === 'bigint' || typeof lane === 'number') {
    return {
      type: 'galaxy',
      ship: uint(lane, 0xff, 'galaxy ship'),
    };
  }
  if (lane?.type === 'galaxy') {
    return {
      type: 'galaxy',
      ship: uint(lane.ship, 0xff, 'galaxy ship'),
    };
  }
  if (lane?.type === 'if' || lane?.type === 'ipv4') {
    return {
      type: 'if',
      ip: uint(lane.ip, 0xffff_ffff, 'IPv4 address'),
      port: uint(lane.port, 0xffff, 'UDP port'),
    };
  }
  throw new Error('UDP lane must be a galaxy or IPv4 lane');
}

export function udpLaneNoun(lane) {
  const normalized = normalizeUdpLane(lane);
  if (normalized.type === 'galaxy') {
    return BigInt(normalized.ship);
  }
  return tuple(
    termAtom('if'),
    BigInt(normalized.ip),
    BigInt(normalized.port),
  );
}

export function encodeUdpFrame(type, lane, packet) {
  const frameType = uint(type, 0xff, 'frame type');
  if (frameType !== UDP_FRAME.SEND && frameType !== UDP_FRAME.HEAR) {
    throw new Error(`unknown UDP frame type ${frameType}`);
  }

  const normalized = normalizeUdpLane(lane);
  const payload = asBytes(packet);
  const laneBytes = normalized.type === 'galaxy' ? 1 : 6;
  const out = new Uint8Array(HEADER_BYTES + laneBytes + payload.length);
  out.set(MAGIC, 0);
  out[4] = 1; // protocol version
  out[5] = frameType;
  out[6] = normalized.type === 'galaxy' ? UDP_LANE.GALAXY : UDP_LANE.IPV4;
  out[7] = laneBytes;

  const view = new DataView(out.buffer, out.byteOffset, out.byteLength);
  if (normalized.type === 'galaxy') {
    out[HEADER_BYTES] = normalized.ship;
  }
  else {
    view.setUint32(HEADER_BYTES, normalized.ip, false);
    view.setUint16(HEADER_BYTES + 4, normalized.port, false);
  }
  out.set(payload, HEADER_BYTES + laneBytes);
  return out;
}

export function decodeUdpFrame(input) {
  const bytes = asBytes(input);
  if (bytes.length < HEADER_BYTES) {
    throw new Error('truncated UDP gateway frame');
  }
  for (let i = 0; i < MAGIC.length; i++) {
    if (bytes[i] !== MAGIC[i]) {
      throw new Error('invalid UDP gateway frame magic');
    }
  }
  if (bytes[4] !== 1) {
    throw new Error(`unsupported UDP gateway frame version ${bytes[4]}`);
  }
  const type = bytes[5];
  if (type !== UDP_FRAME.SEND && type !== UDP_FRAME.HEAR) {
    throw new Error(`unknown UDP gateway frame type ${type}`);
  }

  const laneType = bytes[6];
  const laneBytes = bytes[7];
  if (bytes.length < HEADER_BYTES + laneBytes) {
    throw new Error('truncated UDP gateway lane');
  }
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  let lane;
  if (laneType === UDP_LANE.GALAXY && laneBytes === 1) {
    lane = { type: 'galaxy', ship: bytes[HEADER_BYTES] };
  }
  else if (laneType === UDP_LANE.IPV4 && laneBytes === 6) {
    lane = {
      type: 'if',
      ip: view.getUint32(HEADER_BYTES, false),
      port: view.getUint16(HEADER_BYTES + 4, false),
    };
  }
  else {
    throw new Error(`invalid UDP gateway lane type=${laneType} length=${laneBytes}`);
  }

  return {
    type,
    lane,
    packet: bytes.slice(HEADER_BYTES + laneBytes),
  };
}

export function encodeUdpSend(lane, packet) {
  return encodeUdpFrame(UDP_FRAME.SEND, lane, packet);
}

export function encodeUdpHear(lane, packet) {
  return encodeUdpFrame(UDP_FRAME.HEAR, lane, packet);
}
