import {
  atomFromBytesLE,
  cell,
  jamBytes,
  list,
  termAtom,
  tuple,
} from './urbit-noun.mjs';

export const MESA_SESSION_LANE_TAG = 1n << 63n;

export function amesWire() {
  return cell(termAtom('ames'), 0n);
}

export function amesRuntimeOvum(task) {
  return cell(cell(termAtom('a'), amesWire()), task);
}

export function amesRuntimeOvumJam(task) {
  return jamBytes(amesRuntimeOvum(task));
}

export function loadMesaTask() {
  return cell(termAtom('load'), termAtom('mesa'));
}

export function loadMesaOvumJam() {
  return amesRuntimeOvumJam(loadMesaTask());
}

export function bornTask() {
  return cell(termAtom('born'), 0n);
}

export function bornOvumJam() {
  return amesRuntimeOvumJam(bornTask());
}

function unitShip(ship) {
  if (ship == null) {
    return 0n;
  }
  return cell(0n, BigInt(ship));
}

function loobean(value) {
  return value ? 0n : 1n;
}

export function oldAmesAddressLane(address) {
  return cell(1n, BigInt(address));
}

export function oldAmesShipLane(ship) {
  return cell(0n, BigInt(ship));
}

export function mateTask({ ship = null, dry = false } = {}) {
  return tuple(
    termAtom('mate'),
    unitShip(ship),
    loobean(dry),
  );
}

export function mateOvumJam({ ship = null, dry = false } = {}) {
  return amesRuntimeOvumJam(mateTask({ ship, dry }));
}

export function dearTask({
  ship,
  lane = oldAmesAddressLane(mesaSessionLane()),
}) {
  if (ship == null) {
    throw new Error('ship is required');
  }

  return tuple(termAtom('dear'), BigInt(ship), lane);
}

export function dearOvumJam(input) {
  return amesRuntimeOvumJam(dearTask(input));
}

export function pathNoun(path) {
  if (typeof path !== 'string' || !path.startsWith('/')) {
    throw new Error('path must be an absolute Hoon path string');
  }

  return list(...path.split('/').filter(Boolean).map(termAtom));
}

export function keenTask({ ship, path, security = 0n }) {
  if (ship == null) {
    throw new Error('ship is required');
  }

  return tuple(
    termAtom('keen'),
    security,
    BigInt(ship),
    pathNoun(path),
  );
}

export function keenOvumJam({ ship, path, security = 0n }) {
  return amesRuntimeOvumJam(keenTask({ ship, path, security }));
}

export function mesaSessionLane(sessionId = 1n) {
  const sid = BigInt(sessionId);
  if (sid < 0n || sid >= MESA_SESSION_LANE_TAG) {
    throw new Error('session id must fit below bit 63');
  }
  return MESA_SESSION_LANE_TAG | sid;
}

export function mesaHeerTask({ lane = mesaSessionLane(), packet }) {
  if (packet == null) {
    throw new Error('packet is required');
  }

  const packetBytes = packet instanceof Uint8Array
    ? packet
    : Uint8Array.from(packet);

  return cell(
    termAtom('heer'),
    cell(BigInt(lane), atomFromBytesLE(packetBytes)),
  );
}

export function mesaHeerOvumJam({ lane = mesaSessionLane(), packet }) {
  return amesRuntimeOvumJam(mesaHeerTask({ lane, packet }));
}
