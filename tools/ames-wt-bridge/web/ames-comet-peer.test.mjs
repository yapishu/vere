import assert from 'node:assert/strict';
import test from 'node:test';

import {
  AmesCometProofResponder,
  matchCometProofPeek,
  proofPageForPeek,
} from './ames-comet-peer.mjs';
import {
  decodeProofGage,
  verifyOpenPacketSignature,
} from './ames-proof.mjs';
import {
  encodeSuiteBRingBytes,
  importSuiteBRing,
  parseSuiteBPassBytes,
  verifyBytes,
} from './ames-identity.mjs';
import { atomFromBytesLE } from './urbit-noun.mjs';
import {
  AUTH,
  bytesToHex,
  decodePact,
  encodePeek,
  hexToBytes,
} from './mesa-pact.mjs';

const RFC8032_SEED =
  '9d61b19deffd5a60ba844af492ec2cc4' +
  '4449c5697b326919703bac031cae7f60';
const COMET = 0x8464_c4fd_83ad_dcf0_ad56_7d1b_c5d2_0773n;
const COMET_PATP = '~tobsyr-mitnel-rabmug-rislyr--pocmut-wolhec-hobrel-hidset';
const PROOF_PATH = '/publ/1/a/x/1//pawn/proof/~zod/1';

function deterministicRing() {
  const signingSeed = hexToBytes(RFC8032_SEED);
  const cryptoSeed = Uint8Array.from({ length: 32 }, (_, i) => 31 - i);
  return encodeSuiteBRingBytes(new Uint8Array([...signingSeed, ...cryptoSeed]));
}

async function deterministicIdentity() {
  return importSuiteBRing(deterministicRing());
}

test('matchCometProofPeek recognizes proof peeks addressed to the comet', () => {
  const packet = encodePeek({ ship: COMET, path: PROOF_PATH });
  const match = matchCometProofPeek(packet, { comet: COMET });

  assert.equal(match.receiverPatp, '~zod');
  assert.equal(match.receiver, 0n);
  assert.equal(match.receiverLife, 1n);
  assert.equal(match.request.name.ship, COMET);
  assert.equal(match.request.name.path, PROOF_PATH);
  assert.equal(
    matchCometProofPeek(encodePeek({ ship: COMET, path: '/base/sys/kelvin' }), { comet: COMET }),
    null,
  );
  assert.equal(matchCometProofPeek(packet, { comet: 0x100n }), null);
});

test('proofPageForPeek builds a signed one-fragment proof page', async () => {
  const identity = await deterministicIdentity();
  const pass = parseSuiteBPassBytes(identity.passBytes);
  const response = await proofPageForPeek(identity, encodePeek({ ship: COMET, path: PROOF_PATH }));

  assert.equal(response.match.receiver, 0n);
  assert.equal(response.page.path, PROOF_PATH);
  assert.equal(
    response.page.bindingPath,
    `/${COMET_PATP}//1${PROOF_PATH}`,
  );

  const decoded = decodePact(response.page.pagePacket);
  assert.equal(decoded.type, 'page');
  assert.equal(decoded.name.ship, COMET);
  assert.equal(decoded.name.auth, true);
  assert.equal(decoded.name.frag, 0n);
  assert.equal(decoded.name.path, PROOF_PATH);
  assert.equal(decoded.data.auth.type, AUTH.SIGN);
  assert.equal(decoded.data.totalBytes, BigInt(response.page.proof.proofBytes.length));
  assert.equal(bytesToHex(decoded.data.fragment), bytesToHex(response.page.proof.proofBytes));
  assert.equal(await verifyBytes(
    pass.signingPublicKey,
    decoded.data.auth.signature,
    response.page.binding,
  ), true);

  const proof = decodeProofGage(atomFromBytesLE(decoded.data.fragment));
  assert.equal(await verifyOpenPacketSignature({
    publicKey: pass.signingPublicKey,
    signature: proof.signature,
    signed: proof.signed,
  }), true);
});

test('AmesCometProofResponder answers proof peeks and passes other packets through', async () => {
  const identity = await deterministicIdentity();
  const sent = [];
  const seenPackets = [];
  const proofs = [];
  const errors = [];
  const responder = new AmesCometProofResponder({
    identity,
    send: async packet => {
      sent.push(packet);
      return 'datagram';
    },
    onPacket: event => seenPackets.push(event),
    onProof: event => proofs.push(event),
    onError: event => errors.push(event),
  });

  const proofPeek = encodePeek({ ship: COMET, path: PROOF_PATH });
  const response = await responder.handlePacket({ mode: 'datagram', packet: proofPeek });

  assert.notEqual(response, null);
  assert.equal(sent.length, 1);
  assert.equal(seenPackets.length, 1);
  assert.equal(proofs.length, 1);
  assert.equal(proofs[0].mode, 'datagram');
  assert.equal(errors.length, 0);
  assert.equal(decodePact(sent[0]).type, 'page');

  const otherPeek = encodePeek({ ship: COMET, path: '/base/sys/kelvin' });
  assert.equal(await responder.handlePacket({ mode: 'datagram', packet: otherPeek }), null);
  assert.equal(sent.length, 1);
  assert.equal(seenPackets.length, 2);
  assert.equal(proofs.length, 1);
  assert.equal(errors.length, 0);
});
