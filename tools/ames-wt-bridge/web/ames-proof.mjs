import {
  atomFromBytesLE,
  bytesFromAtomLE,
  cell,
  jamBytes,
  cue,
  jam,
  termAtom,
  tuple,
} from './urbit-noun.mjs';
import {
  SUITE_B,
  parseSuiteBPassBytes,
  signBytes,
  suiteBFingerprintFromPassBytes,
  verifyBytes,
} from './ames-identity.mjs';
import { patp, patpToAtom } from './ames-ship.mjs';
import { lssRoot } from './ames-blake3.mjs';
import { canonicalPatp, mesaBeamPath, mesaBindingBytes } from './ames-mesa-path.mjs';
import { AUTH, decodePact, encodePact } from './mesa-pact.mjs';

export { mesaBeamPath, mesaBindingBytes };

export const MARK = Object.freeze({
  MESSAGE: termAtom('message'),
  PROOF: termAtom('proof'),
});

const COMET_PROOF_PATH_RE = /^\/publ\/1\/a\/x\/1\/\/pawn\/proof\/(~[a-z-]+)\/([0-9]+)$/;

function asShip(value, name) {
  const atom = BigInt(value);
  if (atom < 0n || atom > ((1n << 128n) - 1n)) {
    throw new Error(`${name} must be a 128-bit ship atom`);
  }
  return atom;
}

function asLife(value, name) {
  const life = BigInt(value);
  if (life < 0n) {
    throw new Error(`${name} must be non-negative`);
  }
  return life;
}

function passAtomFrom(identityOrPassBytes) {
  if (identityOrPassBytes.passBytes) {
    return atomFromBytesLE(identityOrPassBytes.passBytes);
  }
  if (identityOrPassBytes instanceof Uint8Array) {
    return atomFromBytesLE(identityOrPassBytes);
  }
  return BigInt(identityOrPassBytes);
}

export function openPacketNoun({
  pass,
  sender,
  senderLife = 1n,
  receiver,
  receiverLife,
}) {
  return tuple(
    passAtomFrom(pass),
    asShip(sender, 'sender'),
    asLife(senderLife, 'senderLife'),
    asShip(receiver, 'receiver'),
    asLife(receiverLife, 'receiverLife'),
  );
}

export function jamOpenPacket(input) {
  return jam(openPacketNoun(input));
}

export function matchCometProofPath(path) {
  const match = COMET_PROOF_PATH_RE.exec(path);
  if (!match) {
    return null;
  }

  return {
    receiverPatp: match[1],
    receiver: patpToAtom(match[1]),
    receiverLife: BigInt(match[2]),
  };
}

function openPacketFromNoun(noun) {
  if (
    !Array.isArray(noun) ||
    !Array.isArray(noun[1]) ||
    !Array.isArray(noun[1][1]) ||
    !Array.isArray(noun[1][1][1])
  ) {
    throw new Error('open packet is not a 5-tuple');
  }

  return {
    pass: noun[0],
    sender: asShip(noun[1][0], 'sender'),
    senderLife: asLife(noun[1][1][0], 'senderLife'),
    receiver: asShip(noun[1][1][1][0], 'receiver'),
    receiverLife: asLife(noun[1][1][1][1], 'receiverLife'),
    noun,
  };
}

export function decodeOpenPacket(signed) {
  return openPacketFromNoun(cue(signed));
}

export async function signOpenPacket(identity, input, { subtle } = {}) {
  const signed = jamOpenPacket({
    pass: input.pass ?? identity,
    sender: input.sender,
    senderLife: input.senderLife ?? 1n,
    receiver: input.receiver,
    receiverLife: input.receiverLife,
  });
  const signatureBytes = await signBytes(identity.signing, bytesFromAtomLE(signed), { subtle });
  const signature = atomFromBytesLE(signatureBytes);
  const proofPayload = jam(cell(signature, signed));
  const proofGage = tuple(MARK.MESSAGE, MARK.PROOF, proofPayload);
  const proof = jam(proofGage);

  return {
    openPacket: openPacketNoun({
      pass: input.pass ?? identity,
      sender: input.sender,
      senderLife: input.senderLife ?? 1n,
      receiver: input.receiver,
      receiverLife: input.receiverLife,
    }),
    signed,
    signature,
    signatureBytes,
    proofPayload,
    proofGage,
    proof,
    proofBytes: bytesFromAtomLE(proof),
  };
}

export async function signCometOpenPacket(identity, {
  receiver,
  receiverLife,
  subtle,
} = {}) {
  const sender = identity.comet ?? identity.ship;
  if (sender === undefined) {
    throw new Error('identity has no comet ship atom');
  }

  return signOpenPacket(identity, {
    sender,
    senderLife: 1n,
    receiver,
    receiverLife,
  }, { subtle });
}

export function decodeProofPayload(proofPayload) {
  const noun = cue(proofPayload);
  if (!Array.isArray(noun) || noun.length !== 2) {
    throw new Error('proof payload is not a cell');
  }
  return {
    signature: noun[0],
    signed: noun[1],
  };
}

export function decodeProofGage(proof) {
  const noun = cue(proof);
  if (
    !Array.isArray(noun) ||
    noun[0] !== MARK.MESSAGE ||
    !Array.isArray(noun[1]) ||
    noun[1][0] !== MARK.PROOF
  ) {
    throw new Error('proof is not a %message %proof gage');
  }

  return {
    proofPayload: noun[1][1],
    ...decodeProofPayload(noun[1][1]),
  };
}

export async function verifyOpenPacketSignature({ publicKey, signature, signed }, { subtle } = {}) {
  return verifyBytes(publicKey, bytesFromAtomLE(signature, 64), bytesFromAtomLE(signed), { subtle });
}

export function proofBytesFromParts(signature, signed) {
  return jamBytes(tuple(MARK.MESSAGE, MARK.PROOF, jam(cell(signature, signed))));
}

export function cometProofPath({ receiverPatp, receiverLife }) {
  return `/publ/1/a/x/1//pawn/proof/${canonicalPatp(receiverPatp, 'receiverPatp')}/${asLife(receiverLife, 'receiverLife')}`;
}

export async function signCometProofPage(identity, {
  receiver,
  receiverPatp,
  receiverLife,
  senderPatp,
  rift = 0,
  bloq = 0,
  subtle,
} = {}) {
  const sender = identity.comet ?? identity.ship;
  if (sender === undefined) {
    throw new Error('identity has no comet ship atom');
  }

  const rcvr = asShip(receiver, 'receiver');
  const rcvrLife = asLife(receiverLife, 'receiverLife');
  const publishedPath = cometProofPath({
    receiverPatp: receiverPatp ?? patp(rcvr),
    receiverLife: rcvrLife,
  });
  const publisherPatp = senderPatp ?? patp(sender);
  const proof = await signCometOpenPacket(identity, {
    receiver: rcvr,
    receiverLife: rcvrLife,
    subtle,
  });

  if (proof.proofBytes.length > 1024) {
    throw new Error('comet proof payload exceeded one Mesa fragment');
  }

  const rootBytes = lssRoot(proof.proofBytes);
  const binding = mesaBindingBytes({
    publisherPatp,
    caseNumber: 1n,
    path: publishedPath,
    root: rootBytes,
  });
  const authSignatureBytes = await signBytes(identity.signing, binding, { subtle });
  const pagePact = {
    type: 'page',
    name: {
      ship: sender,
      rift,
      bloq,
      auth: true,
      frag: 0n,
      path: publishedPath,
    },
    data: {
      totalBytes: BigInt(proof.proofBytes.length),
      auth: { type: AUTH.SIGN, signature: authSignatureBytes },
      fragment: proof.proofBytes,
    },
  };

  return {
    proof,
    path: publishedPath,
    bindingPath: mesaBeamPath({ publisherPatp, caseNumber: 1n, path: publishedPath }),
    binding,
    rootBytes,
    root: atomFromBytesLE(rootBytes),
    authSignatureBytes,
    pagePact,
    pagePacket: encodePact(pagePact),
  };
}

function asPact(packetOrPact) {
  if (packetOrPact instanceof Uint8Array) {
    return decodePact(packetOrPact);
  }
  return packetOrPact ?? null;
}

function checkEqual(actual, expected, message) {
  if (expected !== undefined && actual !== BigInt(expected)) {
    throw new Error(message);
  }
}

export async function verifyCometProofPage(packetOrPact, {
  receiver,
  receiverLife,
  subtle,
} = {}) {
  const pact = asPact(packetOrPact);
  if (!pact || pact.type !== 'page') {
    return null;
  }

  const pathMatch = matchCometProofPath(pact.name.path);
  if (!pathMatch) {
    return null;
  }
  if (!pact.name.auth || pact.data.auth.type !== AUTH.SIGN) {
    throw new Error('comet proof page is not signature authenticated');
  }
  if (pact.data.totalBytes !== BigInt(pact.data.fragment.length)) {
    throw new Error('comet proof page fragment length mismatch');
  }

  const proof = decodeProofGage(atomFromBytesLE(pact.data.fragment));
  const open = decodeOpenPacket(proof.signed);
  const passBytes = bytesFromAtomLE(open.pass, SUITE_B.passBytes);
  const pass = parseSuiteBPassBytes(passBytes);

  if (pact.name.ship !== open.sender) {
    throw new Error('comet proof page publisher does not match open packet sender');
  }
  if (pathMatch.receiver !== open.receiver || pathMatch.receiverLife !== open.receiverLife) {
    throw new Error('comet proof path does not match open packet receiver');
  }
  checkEqual(open.receiver, receiver, 'comet proof receiver does not match local ship');
  checkEqual(open.receiverLife, receiverLife, 'comet proof receiver life does not match local life');

  const comet = await suiteBFingerprintFromPassBytes(passBytes, { subtle });
  if (comet !== open.sender) {
    throw new Error('comet proof pass does not fingerprint to sender');
  }
  if (!await verifyOpenPacketSignature({
    publicKey: pass.signingPublicKey,
    signature: proof.signature,
    signed: proof.signed,
  }, { subtle })) {
    throw new Error('comet proof open-packet signature failed');
  }

  const rootBytes = lssRoot(pact.data.fragment);
  const binding = mesaBindingBytes({
    publisher: open.sender,
    caseNumber: 1n,
    path: pact.name.path,
    root: rootBytes,
  });
  if (!await verifyBytes(pass.signingPublicKey, pact.data.auth.signature, binding, { subtle })) {
    throw new Error('comet proof page binding signature failed');
  }

  return {
    pact,
    path: pathMatch,
    proof,
    open,
    pass,
    passBytes,
    rootBytes,
    binding,
    ship: open.sender,
    shipPatp: patp(open.sender),
    life: open.senderLife,
    receiver: open.receiver,
    receiverPatp: pathMatch.receiverPatp,
    receiverLife: open.receiverLife,
  };
}
