import assert from 'node:assert/strict';
import test from 'node:test';

import {
  MARK,
  cometProofPath,
  decodeOpenPacket,
  decodeProofGage,
  jamOpenPacket,
  matchCometProofPath,
  mesaBeamPath,
  openPacketNoun,
  proofBytesFromParts,
  signCometOpenPacket,
  signCometProofPage,
  verifyCometProofPage,
  verifyOpenPacketSignature,
} from './ames-proof.mjs';
import {
  encodeSuiteBRingBytes,
  importSuiteBRing,
  parseSuiteBPassBytes,
  verifyBytes,
} from './ames-identity.mjs';
import {
  atomFromBytesLE,
  cue,
  jam,
} from './urbit-noun.mjs';
import {
  AUTH,
  bytesToHex,
  decodePact,
  hexToBytes,
} from './mesa-pact.mjs';

const RFC8032_SEED =
  '9d61b19deffd5a60ba844af492ec2cc4' +
  '4449c5697b326919703bac031cae7f60';
const COMET = 0x8464_c4fd_83ad_dcf0_ad56_7d1b_c5d2_0773n;

function deterministicRing() {
  const signingSeed = hexToBytes(RFC8032_SEED);
  const cryptoSeed = Uint8Array.from({ length: 32 }, (_, i) => 31 - i);
  return encodeSuiteBRingBytes(new Uint8Array([...signingSeed, ...cryptoSeed]));
}

test('openPacketNoun matches Hoon tuple jam vector', () => {
  const noun = openPacketNoun({
    pass: 0n,
    sender: 1n,
    senderLife: 1n,
    receiver: 2n,
    receiverLife: 3n,
  });

  assert.equal(jam(noun), 0xd121_c719n);
  assert.equal(jamOpenPacket({
    pass: 0n,
    sender: 1n,
    senderLife: 1n,
    receiver: 2n,
    receiverLife: 3n,
  }), 0xd121_c719n);
});

test('signCometOpenPacket builds a signed open-packet proof gage', async () => {
  const identity = await importSuiteBRing(deterministicRing());
  const proof = await signCometOpenPacket(identity, {
    receiver: 0n,
    receiverLife: 1n,
  });
  const decoded = decodeProofGage(proof.proof);
  const pass = parseSuiteBPassBytes(identity.passBytes);

  assert.equal(identity.ship, COMET);
  assert.equal(proof.proofGage[0], MARK.MESSAGE);
  assert.equal(proof.proofGage[1][0], MARK.PROOF);
  assert.equal(decoded.signature, proof.signature);
  assert.equal(decoded.signed, proof.signed);
  assert.deepEqual(decodeOpenPacket(decoded.signed), {
    pass: atomFromBytesLE(identity.passBytes),
    sender: COMET,
    senderLife: 1n,
    receiver: 0n,
    receiverLife: 1n,
    noun: openPacketNoun({
      pass: identity,
      sender: COMET,
      senderLife: 1n,
      receiver: 0n,
      receiverLife: 1n,
    }),
  });
  assert.deepEqual(cue(proof.signed), openPacketNoun({
    pass: identity,
    sender: COMET,
    senderLife: 1n,
    receiver: 0n,
    receiverLife: 1n,
  }));
  assert.equal(await verifyOpenPacketSignature({
    publicKey: pass.signingPublicKey,
    signature: decoded.signature,
    signed: decoded.signed,
  }), true);
  assert.equal(await verifyBytes(
    pass.cryptoPublicKey,
    proof.signatureBytes,
    new Uint8Array(),
  ), false);
  assert.equal(bytesToHex(proofBytesFromParts(proof.signature, proof.signed)), bytesToHex(proof.proofBytes));
});

test('decodeProofGage rejects non-proof nouns', () => {
  assert.throws(() => decodeProofGage(jam(0n)), /not a %message %proof/);
  assert.throws(
    () => decodeProofGage(jam([MARK.MESSAGE, atomFromBytesLE(Uint8Array.of(1, 2, 3))])),
    /not a %message %proof/,
  );
});

test('comet proof page signs the Mesa one-fragment %auth binding', async () => {
  const identity = await importSuiteBRing(deterministicRing());
  const pass = parseSuiteBPassBytes(identity.passBytes);
  const page = await signCometProofPage(identity, {
    receiver: 0n,
    receiverLife: 1n,
  });
  const pathMatch = matchCometProofPath(page.path);

  assert.equal(
    cometProofPath({ receiverPatp: '~zod', receiverLife: 1n }),
    '/publ/1/a/x/1//pawn/proof/~zod/1',
  );
  assert.equal(pathMatch.receiverPatp, '~zod');
  assert.equal(pathMatch.receiver, 0n);
  assert.equal(pathMatch.receiverLife, 1n);
  assert.equal(matchCometProofPath('/base/sys/kelvin'), null);
  assert.equal(
    mesaBeamPath({
      publisherPatp: '~tobsyr-mitnel-rabmug-rislyr--pocmut-wolhec-hobrel-hidset',
      path: page.path,
    }),
    '/~tobsyr-mitnel-rabmug-rislyr--pocmut-wolhec-hobrel-hidset//1/publ/1/a/x/1//pawn/proof/~zod/1',
  );
  assert.equal(page.path, '/publ/1/a/x/1//pawn/proof/~zod/1');
  assert.equal(page.bindingPath, '/~tobsyr-mitnel-rabmug-rislyr--pocmut-wolhec-hobrel-hidset//1/publ/1/a/x/1//pawn/proof/~zod/1');
  assert.equal(page.proof.proofBytes.length <= 1024, true);
  assert.equal(await verifyBytes(pass.signingPublicKey, page.authSignatureBytes, page.binding), true);

  const decoded = decodePact(page.pagePacket);
  assert.equal(decoded.type, 'page');
  assert.equal(decoded.name.ship, COMET);
  assert.equal(decoded.name.auth, true);
  assert.equal(decoded.name.frag, 0n);
  assert.equal(decoded.name.path, page.path);
  assert.equal(decoded.data.auth.type, AUTH.SIGN);
  assert.equal(decoded.data.totalBytes, BigInt(page.proof.proofBytes.length));
  assert.equal(bytesToHex(decoded.data.fragment), bytesToHex(page.proof.proofBytes));
  assert.equal(bytesToHex(decoded.data.auth.signature), bytesToHex(page.authSignatureBytes));

  const proof = decodeProofGage(atomFromBytesLE(decoded.data.fragment));
  assert.equal(await verifyOpenPacketSignature({
    publicKey: pass.signingPublicKey,
    signature: proof.signature,
    signed: proof.signed,
  }), true);
});

test('verifyCometProofPage learns a verified comet peer from a proof page', async () => {
  const identity = await importSuiteBRing(deterministicRing());
  const page = await signCometProofPage(identity, {
    receiver: 0n,
    receiverLife: 1n,
  });
  const learned = await verifyCometProofPage(page.pagePacket, {
    receiver: 0n,
    receiverLife: 1n,
  });

  assert.equal(learned.ship, COMET);
  assert.equal(learned.shipPatp, '~tobsyr-mitnel-rabmug-rislyr--pocmut-wolhec-hobrel-hidset');
  assert.equal(learned.life, 1n);
  assert.equal(learned.receiver, 0n);
  assert.equal(learned.receiverLife, 1n);
  assert.equal(bytesToHex(learned.passBytes), bytesToHex(identity.passBytes));
  assert.equal(await verifyBytes(learned.pass.signingPublicKey, learned.pact.data.auth.signature, learned.binding), true);

  assert.equal(await verifyCometProofPage(decodePact(page.pagePacket), {
    receiver: 0n,
    receiverLife: 1n,
  }).then(result => result.ship), COMET);
  assert.equal(await verifyCometProofPage(0n), null);
  assert.equal(await verifyCometProofPage({
    ...decodePact(page.pagePacket),
    name: { ...decodePact(page.pagePacket).name, path: '/base/sys/kelvin' },
  }), null);
});

test('verifyCometProofPage rejects tampered or misaddressed proof pages', async () => {
  const identity = await importSuiteBRing(deterministicRing());
  const page = await signCometProofPage(identity, {
    receiver: 0n,
    receiverLife: 1n,
  });
  const tampered = decodePact(page.pagePacket);
  tampered.data.auth.signature = Uint8Array.from(tampered.data.auth.signature);
  tampered.data.auth.signature[0] ^= 1;

  await assert.rejects(
    () => verifyCometProofPage(tampered, { receiver: 0n, receiverLife: 1n }),
    /binding signature failed/,
  );
  await assert.rejects(
    () => verifyCometProofPage(page.pagePacket, { receiver: 1n, receiverLife: 1n }),
    /receiver does not match/,
  );
  await assert.rejects(
    () => verifyCometProofPage(page.pagePacket, { receiver: 0n, receiverLife: 2n }),
    /receiver life does not match/,
  );
});
