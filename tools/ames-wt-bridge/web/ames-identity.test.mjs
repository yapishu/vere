import assert from 'node:assert/strict';
import test from 'node:test';

import {
  SUITE_B,
  atomFromBytesLE,
  bytesFromAtomLE,
  curve25519PublicKeyFromEd25519PublicKey,
  deriveAmesSymmetricKey,
  deriveAmesSymmetricKeyBytes,
  deriveAmesSymmetricKeyFromSuiteB,
  encodeSuiteBRingBytes,
  exportPublicKeyRaw,
  generateCometIdentity,
  generateSigningIdentity,
  generateSuiteBIdentity,
  importSeedKeypair,
  importSuiteBRing,
  importPrivateKeyPkcs8,
  importPublicKeyRaw,
  parseSuiteBPassBytes,
  parseSuiteBRingBytes,
  publicKeyFromSeed,
  sha256Atom,
  shaf,
  shipRank,
  shipSponsor,
  signBytes,
  suiteBFingerprint,
  verifyBytes,
} from './ames-identity.mjs';
import { bytesToHex, hexToBytes } from './mesa-pact.mjs';

const ED25519_PKCS8_SEED_PREFIX = '302e020100300506032b657004220420';
const RFC8032_SEED =
  '9d61b19deffd5a60ba844af492ec2cc4' +
  '4449c5697b326919703bac031cae7f60';
const RFC8032_PUBLIC =
  'd75a980182b10ab7d54bfed3c964073a' +
  '0ee172f3daa62325af021a68f707511a';
const RFC8032_SIGNATURE =
  'e5564300c360ac729086e2cc806e828a' +
  '84877f1eb8e5d974d873e06522490155' +
  '5fb8821590a33bacc61e39701cf9b46b' +
  'd25bf5f0595bbe24655141438e7a100b';
const BFIG = 0x67696662n;
const HOON_SHA256_EMPTY =
  '55b852781b9995a44c939b64e441ae27' +
  '24b96f99c8f4fb9a141cfc9842c4b0e3';
const HOON_SHAF_BFIG_ZERO = '101baae180d27694ff1d8be33542364d';
const HOON_SUITE_B_FINGERPRINT = '8464c4fd83addcf0ad567d1bc5d20773';
const HOON_SUITE_B_FINGERPRINT_DEC = 175_981_320_567_222_533_636_160_738_455_003_989_875n;
const HOON_SEED_A_PUBLIC =
  0xd76d_9622_93bc_9497_c812_fe0b_6d52_1e36_c70b_4dd3_941a_2eb2_f0ca_bc41_e1ba_ac29n;
const HOON_SEED_B_PUBLIC =
  0xb831_5512_6486_dc1d_5f0d_a59b_30d6_e467_99c0_4be7_18dd_701d_be10_cef3_bf07_a103n;
const HOON_SEED_A_B_SHARED =
  0x7d4b_603a_0b74_8d13_5334_3f51_eaaa_461f_00c5_84c9_a124_3368_ff5a_9432_fb2e_f9f6n;

function pkcs8FromSeedHex(seedHex) {
  return hexToBytes(ED25519_PKCS8_SEED_PREFIX + seedHex);
}

function sequentialBytes(start, length) {
  return Uint8Array.from({ length }, (_, i) => start + i);
}

test('generateSigningIdentity creates non-extractable Ed25519 signing keys', async () => {
  const identity = await generateSigningIdentity();
  const message = new TextEncoder().encode('ames browser identity smoke test');
  const signature = await signBytes(identity, message);
  const publicBytes = await exportPublicKeyRaw(identity);

  assert.equal(identity.type, 'ed25519');
  assert.equal(identity.privateKey.extractable, false);
  assert.equal(identity.publicKey.extractable, true);
  assert.equal(publicBytes.length, 32);
  assert.equal(signature.length, 64);
  assert.equal(await verifyBytes(identity, signature, message), true);
  assert.equal(await verifyBytes(publicBytes, signature, message), true);
  assert.equal(await verifyBytes(publicBytes, signature, Uint8Array.of(1, 2, 3)), false);

  await assert.rejects(
    () => crypto.subtle.exportKey('pkcs8', identity.privateKey),
    /key is not extractable/i,
  );
});

test('Ed25519 signing matches RFC 8032 test vector 1', async () => {
  const privateKey = await importPrivateKeyPkcs8(pkcs8FromSeedHex(RFC8032_SEED));
  const publicKey = await importPublicKeyRaw(hexToBytes(RFC8032_PUBLIC));
  const signature = await signBytes(privateKey, new Uint8Array());

  assert.equal(bytesToHex(await publicKeyFromSeed(hexToBytes(RFC8032_SEED))), RFC8032_PUBLIC);
  assert.equal(bytesToHex(signature), RFC8032_SIGNATURE);
  assert.equal(await verifyBytes(publicKey, signature, new Uint8Array()), true);
  assert.equal(
    await verifyBytes(hexToBytes(RFC8032_PUBLIC), hexToBytes(RFC8032_SIGNATURE), new Uint8Array()),
    true,
  );
});

test('Urbit SHA atom helpers match Hoon vectors', async () => {
  assert.equal((await sha256Atom(0n)).toString(16), HOON_SHA256_EMPTY);
  assert.equal((await shaf(BFIG, 0n)).toString(16), HOON_SHAF_BFIG_ZERO);
});

test('suite-B ring/pass bytes match Ames cric layout', async () => {
  const signingSeed = hexToBytes(RFC8032_SEED);
  const cryptoSeed = Uint8Array.from({ length: 32 }, (_, i) => 31 - i);
  const seed = new Uint8Array([...signingSeed, ...cryptoSeed]);
  const ringBytes = encodeSuiteBRingBytes(seed);
  const ring = parseSuiteBRingBytes(ringBytes);

  assert.equal(ringBytes.length, SUITE_B.ringBytes);
  assert.equal(ringBytes[0], 'B'.charCodeAt(0));
  assert.deepEqual(Array.from(ring.signingSeed), Array.from(signingSeed));
  assert.deepEqual(Array.from(ring.cryptoSeed), Array.from(cryptoSeed));

  const identity = await importSuiteBRing(ringBytes);
  const pass = parseSuiteBPassBytes(identity.passBytes);

  assert.equal(identity.type, 'suite-b');
  assert.equal(identity.suite, 1);
  assert.equal(identity.ship, HOON_SUITE_B_FINGERPRINT_DEC);
  assert.equal(identity.ship.toString(16), HOON_SUITE_B_FINGERPRINT);
  assert.equal((await suiteBFingerprint(identity)).toString(16), HOON_SUITE_B_FINGERPRINT);
  assert.equal(identity.ringBytes, undefined);
  assert.equal(identity.passBytes.length, SUITE_B.passBytes);
  assert.equal(identity.passBytes[0], 'b'.charCodeAt(0));
  assert.equal(bytesToHex(pass.signingPublicKey), RFC8032_PUBLIC);
  assert.equal(bytesToHex(pass.cryptoPublicKey), bytesToHex(await publicKeyFromSeed(cryptoSeed)));
  assert.equal(identity.signing.privateKey.extractable, false);
  assert.equal(identity.crypt.privateKey.extractable, false);
});

test('Ames symmetric-key derivation matches Hoon shar:ed vectors', async () => {
  const seedA = sequentialBytes(0x20, 32);
  const seedB = sequentialBytes(0x00, 32);
  const publicA = await publicKeyFromSeed(seedA);
  const publicB = await publicKeyFromSeed(seedB);
  const sharedAB = await deriveAmesSymmetricKeyBytes({
    cryptoSeed: seedA,
    peerCryptoPublicKey: publicB,
  });
  const sharedBA = await deriveAmesSymmetricKey({
    cryptoSeed: seedB,
    peerCryptoPublicKey: publicA,
  });

  assert.equal(bytesToHex(publicA), bytesToHex(bytesFromAtomLE(HOON_SEED_A_PUBLIC, 32)));
  assert.equal(bytesToHex(publicB), bytesToHex(bytesFromAtomLE(HOON_SEED_B_PUBLIC, 32)));
  assert.equal(bytesToHex(sharedAB), bytesToHex(bytesFromAtomLE(HOON_SEED_A_B_SHARED, 32)));
  assert.equal(sharedBA, HOON_SEED_A_B_SHARED);

  const curvePublicA = curve25519PublicKeyFromEd25519PublicKey(publicA);
  assert.equal(curvePublicA.length, 32);
  assert.notEqual(bytesToHex(curvePublicA), bytesToHex(publicA));
});

test('Ames symmetric-key derivation accepts suite-B identity ring/pass inputs', async () => {
  const signingSeedA = sequentialBytes(0x80, 32);
  const signingSeedB = sequentialBytes(0xa0, 32);
  const cryptoSeedA = sequentialBytes(0x20, 32);
  const cryptoSeedB = sequentialBytes(0x00, 32);
  const identityA = await importSuiteBRing(
    encodeSuiteBRingBytes(new Uint8Array([...signingSeedA, ...cryptoSeedA])),
    { retainRing: true },
  );
  const identityB = await importSuiteBRing(
    encodeSuiteBRingBytes(new Uint8Array([...signingSeedB, ...cryptoSeedB])),
    { retainRing: true },
  );

  assert.equal(
    await deriveAmesSymmetricKeyFromSuiteB(identityA, identityB),
    HOON_SEED_A_B_SHARED,
  );
  assert.equal(
    await deriveAmesSymmetricKeyFromSuiteB(identityA.ringBytes, identityB.passBytes),
    HOON_SEED_A_B_SHARED,
  );

  await assert.rejects(
    () => deriveAmesSymmetricKeyBytes({
      cryptoSeed: cryptoSeedA.slice(0, 31),
      peerCryptoPublicKey: parseSuiteBPassBytes(identityB.passBytes).cryptoPublicKey,
    }),
    /32 bytes/,
  );
  assert.throws(
    () => curve25519PublicKeyFromEd25519PublicKey(Uint8Array.of(1, 2, 3)),
    /32 bytes/,
  );
});

test('ship rank and sponsor helpers mirror Ames title rules', async () => {
  assert.equal(shipRank(0n), 'czar');
  assert.equal(shipRank(255n), 'czar');
  assert.equal(shipRank(256n), 'king');
  assert.equal(shipRank(0xffffn), 'king');
  assert.equal(shipRank(0x1_0000n), 'duke');
  assert.equal(shipRank(0x1_0000_0000n), 'earl');
  assert.equal(shipRank(0x1_0000_0000_0000_0000n), 'pawn');

  assert.equal(shipSponsor(0x1234n), 0x34n);
  assert.equal(shipSponsor(0x1234_5678n), 0x5678n);
  assert.equal(shipSponsor(HOON_SUITE_B_FINGERPRINT_DEC), 0x0773n);
  assert.equal(shipRank(shipSponsor(HOON_SUITE_B_FINGERPRINT_DEC)), 'king');
});

test('suite-B ring/pass bytes round-trip through Urbit atom order', async () => {
  const seed = Uint8Array.from({ length: 64 }, (_, i) => i + 1);
  const ringBytes = encodeSuiteBRingBytes(seed);
  const identity = await importSuiteBRing(ringBytes);
  const ringAtom = atomFromBytesLE(ringBytes);
  const passAtom = atomFromBytesLE(identity.passBytes);

  assert.equal(Number(ringAtom & 0xffn), SUITE_B.ringTag);
  assert.equal(Number(passAtom & 0xffn), SUITE_B.passTag);
  assert.deepEqual(Array.from(bytesFromAtomLE(ringAtom, SUITE_B.ringBytes)), Array.from(ringBytes));
  assert.deepEqual(
    Array.from(bytesFromAtomLE(passAtom, SUITE_B.passBytes)),
    Array.from(identity.passBytes),
  );

  assert.throws(() => bytesFromAtomLE(-1n, 1), /non-negative/);
  assert.throws(() => bytesFromAtomLE(0x100n, 1), /does not fit/);
});

test('generateCometIdentity mines a comet-shaped suite-B identity', async () => {
  const signingSeed = hexToBytes(RFC8032_SEED);
  const cryptoSeed = Uint8Array.from({ length: 32 }, (_, i) => 31 - i);
  const seed = new Uint8Array([...signingSeed, ...cryptoSeed]);
  const deterministic = {
    getRandomValues(bytes) {
      bytes.set(seed);
      return bytes;
    },
  };
  const identity = await generateCometIdentity({
    crypto: deterministic,
    retainRing: true,
  });

  assert.equal(identity.attempts, 1);
  assert.equal(identity.comet, HOON_SUITE_B_FINGERPRINT_DEC);
  assert.equal(identity.sponsor, 0x0773n);
  assert.equal(shipRank(identity.comet), 'pawn');
  assert.equal(shipRank(identity.sponsor), 'king');
  assert.deepEqual(Array.from(identity.ringBytes), Array.from(encodeSuiteBRingBytes(seed)));
});

test('suite-B identity signs with the signing seed public key', async () => {
  const seed = new Uint8Array([
    ...hexToBytes(RFC8032_SEED),
    ...Uint8Array.from({ length: 32 }, (_, i) => i),
  ]);
  const identity = await importSuiteBRing(encodeSuiteBRingBytes(seed), { retainRing: true });
  const message = new TextEncoder().encode('suite-b signing material');
  const signature = await signBytes(identity.signing, message);
  const pass = parseSuiteBPassBytes(identity.passBytes);

  assert.deepEqual(Array.from(identity.ringBytes), Array.from(encodeSuiteBRingBytes(seed)));
  assert.equal(await verifyBytes(pass.signingPublicKey, signature, message), true);
  assert.equal(await verifyBytes(pass.cryptoPublicKey, signature, message), false);
});

test('generateSuiteBIdentity uses random suite-B seed material', async () => {
  const deterministic = {
    getRandomValues(bytes) {
      for (let i = 0; i < bytes.length; i++) {
        bytes[i] = i;
      }
      return bytes;
    },
  };
  const identity = await generateSuiteBIdentity({
    crypto: deterministic,
    retainRing: true,
  });
  const ring = parseSuiteBRingBytes(identity.ringBytes);
  const pass = parseSuiteBPassBytes(identity.passBytes);

  assert.equal(identity.ringBytes[0], 'B'.charCodeAt(0));
  assert.deepEqual(Array.from(ring.signingSeed), Array.from({ length: 32 }, (_, i) => i));
  assert.deepEqual(Array.from(ring.cryptoSeed), Array.from({ length: 32 }, (_, i) => i + 32));
  assert.equal(bytesToHex(pass.signingPublicKey), bytesToHex(await publicKeyFromSeed(ring.signingSeed)));
});

test('importSeedKeypair rejects malformed seeds', async () => {
  await assert.rejects(() => importSeedKeypair(Uint8Array.of(1, 2, 3)), /32 bytes/);
  assert.throws(() => parseSuiteBPassBytes(Uint8Array.of(0x62)), /65 bytes/);
  assert.throws(
    () => parseSuiteBRingBytes(Uint8Array.from({ length: 65 }, () => 0)),
    /bad suite-B ring tag/,
  );
});
