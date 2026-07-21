import { lssRoot } from './ames-blake3.mjs';
import {
  mesaDecryptBytes,
  mesaEncryptBytes,
  mesaMacBinding,
  mesaOpenPath,
  mesaSealPath,
  scotUv,
  slawUv,
} from './ames-mesa-crypt.mjs';
import {
  canonicalPatp,
  mesaBeamPath,
  mesaBindingBytes,
  mesaFlowPath,
} from './ames-mesa-path.mjs';
import { patp, patpToAtom } from './ames-ship.mjs';
import { AUTH, decodePact, encodePact } from './mesa-pact.mjs';
import { atomFromBytesLE, cue, jamBytes } from './urbit-noun.mjs';

export { mesaBeamPath, mesaBindingBytes, mesaFlowPath };

function asAtom(value, name) {
  const atom = BigInt(value);
  if (atom < 0n) {
    throw new Error(`${name} must be non-negative`);
  }
  return atom;
}

function asShip(value, name) {
  const ship = asAtom(value, name);
  if (ship > ((1n << 128n) - 1n)) {
    throw new Error(`${name} must fit in 128 bits`);
  }
  return ship;
}

function asUint(value, name, max = 0xffffffff) {
  if (!Number.isSafeInteger(value) || value < 0 || value > max) {
    throw new Error(`${name} must be an unsigned integer <= ${max}`);
  }
  return value;
}

function asPact(packetOrPact) {
  if (packetOrPact instanceof Uint8Array) {
    return decodePact(packetOrPact);
  }
  return packetOrPact ?? null;
}

function safePact(packetOrPact) {
  try {
    return asPact(packetOrPact);
  }
  catch (_) {
    return null;
  }
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

function checkEqual(actual, expected, message) {
  if (expected !== undefined && actual !== expected) {
    throw new Error(message);
  }
}

export function makeChumPath({
  key,
  serverLife,
  client,
  clientPatp,
  clientLife,
  path,
}) {
  const who = clientPatp === undefined
    ? canonicalPatp(client, 'client')
    : canonicalPatp(clientPatp, 'clientPatp');
  const sealed = mesaSealPath(key, path);
  return {
    path: `/chum/${asAtom(serverLife, 'serverLife')}/${who}/${asAtom(clientLife, 'clientLife')}/${scotUv(sealed)}`,
    sealed,
  };
}

export function parseChumPath(path, { key } = {}) {
  const match = /^\/chum\/([0-9]+)\/(~[a-z-]+)\/([0-9]+)\/(~\.0v[0-9a-v.]+)$/.exec(path);
  if (!match) {
    throw new Error('path is not a %chum path');
  }

  const sealed = slawUv(match[4]);
  const out = {
    serverLife: BigInt(match[1]),
    clientPatp: patp(patpToAtom(match[2])),
    client: patpToAtom(match[2]),
    clientLife: BigInt(match[3]),
    sealedText: match[4],
    sealed,
  };
  if (key !== undefined) {
    out.innerPath = mesaOpenPath(key, sealed);
  }
  return out;
}

function payloadBytesFrom({ gage, plaintextBytes }) {
  if (gage !== undefined && plaintextBytes !== undefined) {
    throw new Error('provide either gage or plaintextBytes, not both');
  }
  if (gage !== undefined) {
    return jamBytes(gage);
  }
  if (plaintextBytes instanceof Uint8Array) {
    return plaintextBytes;
  }
  return Uint8Array.from(plaintextBytes ?? []);
}

export function buildChumPoke({
  key,
  sender,
  senderLife = 1n,
  senderRift = 0,
  receiver,
  receiverLife,
  receiverRift = 0,
  ackPath,
  payloadPath,
  gage,
  plaintextBytes,
  hop = 0,
} = {}) {
  const snd = asShip(sender, 'sender');
  const rcv = asShip(receiver, 'receiver');
  const sndPatp = patp(snd);
  const rcvPatp = patp(rcv);
  const sndLife = asAtom(senderLife, 'senderLife');
  const rcvLife = asAtom(receiverLife, 'receiverLife');
  const bytes = payloadBytesFrom({ gage, plaintextBytes });

  const ack = makeChumPath({
    key,
    serverLife: rcvLife,
    clientPatp: sndPatp,
    clientLife: sndLife,
    path: ackPath,
  });
  const payload = makeChumPath({
    key,
    serverLife: sndLife,
    clientPatp: rcvPatp,
    clientLife: rcvLife,
    path: payloadPath,
  });

  const ciphertext = mesaEncryptBytes(key, payload.sealed, bytes);
  if (ciphertext.length > 1024) {
    throw new Error('encrypted poke payload exceeded one Mesa fragment');
  }

  const rootBytes = lssRoot(ciphertext);
  const binding = mesaBindingBytes({
    publisherPatp: sndPatp,
    caseNumber: 1n,
    path: payload.path,
    root: rootBytes,
  });
  const mac = mesaMacBinding(key, binding);
  const pact = {
    type: 'poke',
    hop: asUint(hop, 'hop', 7),
    name: {
      ship: rcv,
      rift: asUint(receiverRift, 'receiverRift'),
      bloq: 13,
      auth: true,
      frag: 0n,
      path: ack.path,
    },
    payloadName: {
      ship: snd,
      rift: asUint(senderRift, 'senderRift'),
      bloq: 13,
      frag: 0n,
      path: payload.path,
    },
    data: {
      totalBytes: BigInt(ciphertext.length),
      auth: { type: AUTH.HMAC, mac },
      fragment: ciphertext,
    },
  };

  return {
    ackPath: ack.path,
    ackSealed: ack.sealed,
    payloadPath: payload.path,
    payloadSealed: payload.sealed,
    plaintextBytes: bytes,
    ciphertext,
    rootBytes,
    binding,
    mac,
    pact,
    packet: encodePact(pact),
  };
}

export function openChumPoke(packetOrPact, {
  key,
  local,
  localLife,
  localRift,
  remote,
  remoteLife,
  remoteRift,
  expectedAckPath,
  expectedPayloadPath,
  decodeGage = true,
} = {}) {
  if (key === undefined) {
    throw new Error('key is required');
  }

  const pact = asPact(packetOrPact);
  if (!pact || pact.type !== 'poke') {
    throw new Error('packet is not a %poke pact');
  }
  if (pact.data.auth.type !== AUTH.HMAC) {
    throw new Error('poke is not %hmac authenticated');
  }
  if (pact.data.totalBytes !== BigInt(pact.data.fragment.length)) {
    throw new Error('only one-fragment %poke payloads are supported');
  }

  const loc = local === undefined ? undefined : asShip(local, 'local');
  const rem = remote === undefined ? undefined : asShip(remote, 'remote');
  const locLife = localLife === undefined ? undefined : asAtom(localLife, 'localLife');
  const remLife = remoteLife === undefined ? undefined : asAtom(remoteLife, 'remoteLife');
  const locRift = localRift === undefined ? undefined : asUint(localRift, 'localRift');
  const remRift = remoteRift === undefined ? undefined : asUint(remoteRift, 'remoteRift');

  checkEqual(pact.name.ship, loc, 'ack name is not addressed to the local ship');
  checkEqual(pact.name.rift, locRift, 'ack name rift does not match local rift');
  checkEqual(pact.payloadName.ship, rem, 'payload name is not published by the remote ship');
  checkEqual(pact.payloadName.rift, remRift, 'payload name rift does not match remote rift');

  const ack = parseChumPath(pact.name.path, { key });
  const payload = parseChumPath(pact.payloadName.path, { key });

  checkEqual(ack.serverLife, locLife, 'ack path server life does not match local life');
  checkEqual(ack.client, rem, 'ack path client does not match remote ship');
  checkEqual(ack.clientLife, remLife, 'ack path client life does not match remote life');
  checkEqual(payload.serverLife, remLife, 'payload path server life does not match remote life');
  checkEqual(payload.client, loc, 'payload path client does not match local ship');
  checkEqual(payload.clientLife, locLife, 'payload path client life does not match local life');
  checkEqual(ack.innerPath, expectedAckPath, 'ack inner path did not match expectation');
  checkEqual(payload.innerPath, expectedPayloadPath, 'payload inner path did not match expectation');

  const rootBytes = lssRoot(pact.data.fragment);
  const binding = mesaBindingBytes({
    publisher: pact.payloadName.ship,
    caseNumber: 1n,
    path: pact.payloadName.path,
    root: rootBytes,
  });
  const mac = mesaMacBinding(key, binding);
  if (!constantBytesEqual(mac, pact.data.auth.mac)) {
    throw new Error('poke authentication failed');
  }

  const plaintextBytes = mesaDecryptBytes(key, payload.sealed, pact.data.fragment);
  let gage;
  if (decodeGage) {
    gage = cue(atomFromBytesLE(plaintextBytes));
  }

  return {
    pact,
    ack,
    payload,
    rootBytes,
    binding,
    mac,
    plaintextBytes,
    gage,
  };
}

export class AmesChumPeer {
  constructor({
    key,
    local,
    localLife = 1n,
    localRift = 0,
    remote,
    remoteLife,
    remoteRift = 0,
    send,
    onPoke = () => {},
    onError = () => {},
  } = {}) {
    if (key === undefined) {
      throw new Error('key is required');
    }
    this.key = key;
    this.local = asShip(local, 'local');
    this.localLife = asAtom(localLife, 'localLife');
    this.localRift = asUint(localRift, 'localRift');
    this.remote = asShip(remote, 'remote');
    this.remoteLife = asAtom(remoteLife, 'remoteLife');
    this.remoteRift = asUint(remoteRift, 'remoteRift');
    this.send = send;
    this.onPoke = onPoke;
    this.onError = onError;
  }

  buildPoke(input = {}) {
    return buildChumPoke({
      ...input,
      key: this.key,
      sender: this.local,
      senderLife: this.localLife,
      senderRift: this.localRift,
      receiver: this.remote,
      receiverLife: this.remoteLife,
      receiverRift: this.remoteRift,
    });
  }

  async sendPoke(input = {}) {
    if (typeof this.send !== 'function') {
      throw new Error('send function is required');
    }

    const built = this.buildPoke(input);
    const mode = await this.send(built.packet);
    return { ...built, mode };
  }

  handlePacket(eventOrPacket, options = {}) {
    const packetOrPact = eventOrPacket?.packet ?? eventOrPacket;
    const pact = safePact(packetOrPact);
    if (
      !pact ||
      pact.type !== 'poke' ||
      pact.data?.auth?.type !== AUTH.HMAC ||
      pact.name.ship !== this.local ||
      pact.payloadName.ship !== this.remote
    ) {
      return null;
    }

    try {
      const opened = openChumPoke(pact, {
        key: this.key,
        local: this.local,
        localLife: this.localLife,
        localRift: this.localRift,
        remote: this.remote,
        remoteLife: this.remoteLife,
        remoteRift: this.remoteRift,
        ...options,
      });
      this.onPoke({ ...opened, event: eventOrPacket });
      return opened;
    }
    catch (error) {
      this.onError({ type: 'chum-poke-open', error, event: eventOrPacket, pact });
      return null;
    }
  }
}
