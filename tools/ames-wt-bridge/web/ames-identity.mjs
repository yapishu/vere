const ED25519 = { name: 'Ed25519' };
const ED25519_PKCS8_SEED_PREFIX = '302e020100300506032b657004220420';
const ED25519_P = (1n << 255n) - 19n;
const ED25519_D =
  37095705934669439343138083508754565189542113879843219016388785533085940283555n;
const ED25519_BASE = [
  15112221349535400772501151409588531511454012693041857206046113283949847762202n,
  46316835694926478169428394003475163141307993866256225615783033603165251855960n,
];
const ED25519_SQRT_M1 =
  19681161376707505956807079304988542015446066515923890162744021073123829784752n;
const MASK_128 = (1n << 128n) - 1n;
const BFIG = 0x67696662n; // %bfig

export const SUITE_B = {
  number: 1,
  ringTag: 0x42, // 'B'
  passTag: 0x62, // 'b'
  seedBytes: 64,
  keyBytes: 32,
  ringBytes: 65,
  passBytes: 65,
};

function asBytes(value) {
  return value instanceof Uint8Array ? value : Uint8Array.from(value);
}

function subtleCrypto(subtle) {
  const out = subtle ?? globalThis.crypto?.subtle;
  if (!out) {
    throw new Error('WebCrypto subtle crypto is not available');
  }
  return out;
}

function privateKey(identityOrKey) {
  return identityOrKey.privateKey ?? identityOrKey;
}

function publicKey(identityOrKey) {
  return identityOrKey.publicKey ?? identityOrKey;
}

function assertLength(bytes, length, label) {
  if (bytes.length !== length) {
    throw new Error(`${label} must be ${length} bytes`);
  }
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

function hexToBytes(hex) {
  const clean = hex.replace(/\s+/g, '');
  if ((clean.length % 2) !== 0) {
    throw new Error('hex input must have an even length');
  }

  const out = new Uint8Array(clean.length / 2);
  for (let i = 0; i < out.length; i++) {
    out[i] = Number.parseInt(clean.slice(i * 2, (i * 2) + 2), 16);
  }
  return out;
}

function randomBytes(length, cryptoObject) {
  const crypto = cryptoObject ?? globalThis.crypto;
  if (!crypto?.getRandomValues) {
    throw new Error('WebCrypto random source is not available');
  }

  const bytes = new Uint8Array(length);
  crypto.getRandomValues(bytes);
  return bytes;
}

function atomByteLength(atom) {
  let value = BigInt(atom);
  if (value < 0n) {
    throw new Error('atom must be non-negative');
  }

  let length = 0;
  while (value > 0n) {
    length++;
    value >>= 8n;
  }
  return length;
}

function pkcs8FromSeed(seed) {
  assertLength(seed, SUITE_B.keyBytes, 'Ed25519 seed');
  return concatBytes(hexToBytes(ED25519_PKCS8_SEED_PREFIX), seed);
}

function mod(n) {
  const r = n % ED25519_P;
  return r >= 0n ? r : r + ED25519_P;
}

function modPow(base, exp) {
  let b = mod(base);
  let e = exp;
  let out = 1n;

  while (e > 0n) {
    if ((e & 1n) === 1n) {
      out = mod(out * b);
    }
    b = mod(b * b);
    e >>= 1n;
  }

  return out;
}

function modInv(n) {
  return modPow(n, ED25519_P - 2n);
}

function modSqrtRatio(u, v) {
  const maybeRoot = mod(
    u * modPow(v, 3n) * modPow(mod(u * modPow(v, 7n)), (ED25519_P - 5n) / 8n),
  );
  if (mod(v * maybeRoot * maybeRoot) === mod(u)) {
    return maybeRoot;
  }

  const root = mod(maybeRoot * ED25519_SQRT_M1);
  if (mod(v * root * root) === mod(u)) {
    return root;
  }

  throw new Error('invalid Ed25519 point');
}

function bytesToBigIntLE(bytes) {
  let out = 0n;
  for (let i = bytes.length - 1; i >= 0; i--) {
    out = (out << 8n) | BigInt(bytes[i]);
  }
  return out;
}

export function atomFromBytesLE(bytes) {
  return bytesToBigIntLE(asBytes(bytes));
}

function bigIntToBytesLE(value, length) {
  let n = value;
  const out = new Uint8Array(length);
  for (let i = 0; i < length; i++) {
    out[i] = Number(n & 0xffn);
    n >>= 8n;
  }
  return out;
}

export function bytesFromAtomLE(atom, length) {
  const value = BigInt(atom);
  if (value < 0n) {
    throw new Error('atom must be non-negative');
  }

  const bytes = bigIntToBytesLE(value, length);
  if ((value >> BigInt(length * 8)) !== 0n) {
    throw new Error(`atom does not fit in ${length} bytes`);
  }
  return bytes;
}

export async function sha256Atom(atom, { subtle } = {}) {
  const value = BigInt(atom);
  const length = atomByteLength(value);
  const digest = await subtleCrypto(subtle).digest(
    'SHA-256',
    bytesFromAtomLE(value, length),
  );

  return atomFromBytesLE(new Uint8Array(digest));
}

export async function shas(salt, atom, { subtle } = {}) {
  const saltAtom = BigInt(salt);
  const length = Math.max(SUITE_B.keyBytes, atomByteLength(saltAtom));
  const salted = saltAtom ^ await sha256Atom(atom, { subtle });

  return atomFromBytesLE(new Uint8Array(await subtleCrypto(subtle).digest(
    'SHA-256',
    bytesFromAtomLE(salted, length),
  )));
}

export async function shaf(salt, atom, { subtle } = {}) {
  const hash = await shas(salt, atom, { subtle });
  return (hash & MASK_128) ^ (hash >> 128n);
}

export async function suiteBFingerprintFromPassBytes(passBytes, { subtle } = {}) {
  const pass = asBytes(passBytes);
  parseSuiteBPassBytes(pass);
  return shaf(BFIG, atomFromBytesLE(pass), { subtle });
}

export async function suiteBFingerprint(identityOrPassBytes, { subtle } = {}) {
  const passBytes = identityOrPassBytes.passBytes ?? identityOrPassBytes;
  return suiteBFingerprintFromPassBytes(passBytes, { subtle });
}

export function shipRank(ship) {
  const length = atomByteLength(ship);
  if (length <= 1) {
    return 'czar';
  }
  if (length === 2) {
    return 'king';
  }
  if (length <= 4) {
    return 'duke';
  }
  if (length <= 8) {
    return 'earl';
  }
  if (length <= 16) {
    return 'pawn';
  }

  throw new Error('ship atom exceeds 128 bits');
}

export function shipSponsor(ship) {
  const who = BigInt(ship);
  switch (shipRank(who)) {
    case 'czar':
      return who;
    case 'king':
      return who & 0xffn;
    case 'duke':
    case 'pawn':
      return who & 0xffffn;
    case 'earl':
      return who & 0xffffffffn;
    default:
      throw new Error('unknown ship rank');
  }
}

export function isCometShip(ship) {
  return shipRank(ship) === 'pawn';
}

function pointAdd(a, b) {
  const [x1, y1] = a;
  const [x2, y2] = b;
  const dxxyy = mod(ED25519_D * x1 * x2 * y1 * y2);

  return [
    mod((x1 * y2 + x2 * y1) * modInv(1n + dxxyy)),
    mod((y1 * y2 + x1 * x2) * modInv(1n - dxxyy)),
  ];
}

function pointMultiply(point, scalar) {
  let n = scalar;
  let acc = [0n, 1n];
  let cur = point;

  while (n > 0n) {
    if ((n & 1n) === 1n) {
      acc = pointAdd(acc, cur);
    }
    cur = pointAdd(cur, cur);
    n >>= 1n;
  }

  return acc;
}

function encodePoint(point) {
  const [x, y] = point;
  const out = bigIntToBytesLE(y, SUITE_B.keyBytes);
  out[31] |= Number((x & 1n) << 7n);
  return out;
}

function isPointOnCurve(point) {
  const [x, y] = point;
  const xx = mod(x * x);
  const yy = mod(y * y);
  return mod(yy - xx) === mod(1n + (ED25519_D * xx * yy));
}

function decodePoint(bytes) {
  const point = asBytes(bytes);
  assertLength(point, SUITE_B.keyBytes, 'Ed25519 public key');

  const yBytes = Uint8Array.from(point);
  const sign = BigInt(yBytes[31] >> 7);
  yBytes[31] &= 0x7f;

  const y = bytesToBigIntLE(yBytes);
  if (y >= ED25519_P) {
    throw new Error('invalid Ed25519 point');
  }

  const yy = mod(y * y);
  let x = modSqrtRatio(yy - 1n, (ED25519_D * yy) + 1n);
  if ((x & 1n) !== sign) {
    x = mod(-x);
  }

  const decoded = [x, y];
  if (!isPointOnCurve(decoded)) {
    throw new Error('invalid Ed25519 point');
  }
  return decoded;
}

function edPublicKeyToMontgomeryU(publicKeyBytes) {
  const [, y] = decodePoint(publicKeyBytes);
  const den = mod(1n - y);
  if (den === 0n) {
    throw new Error('Ed25519 point cannot be converted to Curve25519');
  }
  return mod((1n + y) * modInv(den));
}

async function scalarBytesFromSeed(seed, subtle) {
  const hash = new Uint8Array(await subtleCrypto(subtle).digest('SHA-512', seed));
  const scalar = hash.slice(0, 32);
  scalar[0] &= 248;
  scalar[31] &= 63;
  scalar[31] |= 64;
  return scalar;
}

async function scalarFromSeed(seed, subtle) {
  const scalar = await scalarBytesFromSeed(seed, subtle);
  return bytesToBigIntLE(scalar);
}

function curve25519ScalarMult(scalarBytes, u) {
  const scalar = bytesToBigIntLE(scalarBytes);
  const x1 = mod(u);
  let x2 = 1n;
  let z2 = 0n;
  let x3 = x1;
  let z3 = 1n;
  let swap = 0n;

  const cswap = bit => {
    if (bit === 0n) {
      return;
    }
    [x2, x3] = [x3, x2];
    [z2, z3] = [z3, z2];
  };

  for (let bit = 254; bit >= 0; bit--) {
    const k = (scalar >> BigInt(bit)) & 1n;
    swap ^= k;
    cswap(swap);
    swap = k;

    const a = mod(x2 + z2);
    const aa = mod(a * a);
    const b = mod(x2 - z2);
    const bb = mod(b * b);
    const e = mod(aa - bb);
    const c = mod(x3 + z3);
    const d = mod(x3 - z3);
    const da = mod(d * a);
    const cb = mod(c * b);

    x3 = mod((da + cb) * (da + cb));
    z3 = mod(x1 * mod((da - cb) * (da - cb)));
    x2 = mod(aa * bb);
    z2 = mod(e * (aa + (121665n * e)));
  }

  cswap(swap);
  return mod(x2 * modInv(z2));
}

export async function generateSigningIdentity({
  subtle,
  extractable = false,
} = {}) {
  const keys = await subtleCrypto(subtle).generateKey(
    ED25519,
    extractable,
    ['sign', 'verify'],
  );

  return {
    type: 'ed25519',
    privateKey: keys.privateKey,
    publicKey: keys.publicKey,
  };
}

export async function importSigningIdentityFromSeed(seed, {
  subtle,
  extractable = false,
} = {}) {
  const bytes = asBytes(seed);
  const privateKey = await importPrivateKeyPkcs8(
    pkcs8FromSeed(bytes),
    { subtle, extractable },
  );
  const publicKey = await importPublicKeyRaw(await publicKeyFromSeed(bytes, { subtle }), { subtle });

  return { type: 'ed25519', privateKey, publicKey };
}

export async function publicKeyFromSeed(seed, { subtle } = {}) {
  const bytes = asBytes(seed);
  assertLength(bytes, SUITE_B.keyBytes, 'Ed25519 seed');
  return encodePoint(pointMultiply(ED25519_BASE, await scalarFromSeed(bytes, subtle)));
}

export function curve25519PublicKeyFromEd25519PublicKey(publicKeyBytes) {
  return bigIntToBytesLE(edPublicKeyToMontgomeryU(publicKeyBytes), SUITE_B.keyBytes);
}

export async function deriveAmesSymmetricKeyBytes({
  cryptoSeed,
  peerCryptoPublicKey,
  subtle,
} = {}) {
  const seed = asBytes(cryptoSeed);
  assertLength(seed, SUITE_B.keyBytes, 'suite-B crypto seed');
  const peer = asBytes(peerCryptoPublicKey);
  assertLength(peer, SUITE_B.keyBytes, 'suite-B peer crypto public key');

  const scalar = await scalarBytesFromSeed(seed, subtle);
  const shared = curve25519ScalarMult(scalar, edPublicKeyToMontgomeryU(peer));
  return bigIntToBytesLE(shared, SUITE_B.keyBytes);
}

export async function deriveAmesSymmetricKey({
  cryptoSeed,
  peerCryptoPublicKey,
  subtle,
} = {}) {
  return atomFromBytesLE(await deriveAmesSymmetricKeyBytes({
    cryptoSeed,
    peerCryptoPublicKey,
    subtle,
  }));
}

function identityRingBytes(identityOrRingBytes) {
  return identityOrRingBytes?.ringBytes ?? identityOrRingBytes;
}

function identityPassBytes(identityOrPassBytes) {
  return identityOrPassBytes?.passBytes ?? identityOrPassBytes;
}

export async function deriveAmesSymmetricKeyBytesFromSuiteB(
  localIdentityOrRingBytes,
  peerIdentityOrPassBytes,
  { subtle } = {},
) {
  const ring = parseSuiteBRingBytes(identityRingBytes(localIdentityOrRingBytes));
  const pass = parseSuiteBPassBytes(identityPassBytes(peerIdentityOrPassBytes));
  return deriveAmesSymmetricKeyBytes({
    cryptoSeed: ring.cryptoSeed,
    peerCryptoPublicKey: pass.cryptoPublicKey,
    subtle,
  });
}

export async function deriveAmesSymmetricKeyFromSuiteB(
  localIdentityOrRingBytes,
  peerIdentityOrPassBytes,
  { subtle } = {},
) {
  return atomFromBytesLE(await deriveAmesSymmetricKeyBytesFromSuiteB(
    localIdentityOrRingBytes,
    peerIdentityOrPassBytes,
    { subtle },
  ));
}

export async function importSeedKeypair(seed, {
  subtle,
  extractable = false,
} = {}) {
  const bytes = asBytes(seed);
  assertLength(bytes, SUITE_B.keyBytes, 'Ed25519 seed');

  return {
    privateKey: await importPrivateKeyPkcs8(pkcs8FromSeed(bytes), { subtle, extractable }),
    publicKey: await importPublicKeyRaw(await publicKeyFromSeed(bytes, { subtle }), { subtle }),
    seed: extractable ? Uint8Array.from(bytes) : undefined,
  };
}

export async function importSuiteBRing(bytes, {
  subtle,
  extractable = false,
  retainRing = false,
} = {}) {
  const ring = asBytes(bytes);
  assertLength(ring, SUITE_B.ringBytes, 'suite-B ring');
  if (ring[0] !== SUITE_B.ringTag) {
    throw new Error('bad suite-B ring tag');
  }

  const signingSeed = ring.slice(1, 33);
  const cryptoSeed = ring.slice(33, 65);
  const signing = await importSeedKeypair(signingSeed, { subtle, extractable });
  const crypt = await importSeedKeypair(cryptoSeed, { subtle, extractable });
  const passBytes = encodeSuiteBPassBytes({
    signingPublicKey: await exportPublicKeyRaw(signing, { subtle }),
    cryptoPublicKey: await exportPublicKeyRaw(crypt, { subtle }),
  });
  const ship = await suiteBFingerprintFromPassBytes(passBytes, { subtle });

  return {
    type: 'suite-b',
    suite: SUITE_B.number,
    signing,
    crypt,
    passBytes,
    ship,
    ringBytes: retainRing ? Uint8Array.from(ring) : undefined,
  };
}

export async function generateSuiteBIdentity({
  subtle,
  crypto: cryptoObject,
  extractable = false,
  retainRing = false,
} = {}) {
  const seed = randomBytes(SUITE_B.seedBytes, cryptoObject);
  const ringBytes = encodeSuiteBRingBytes(seed);
  return importSuiteBRing(ringBytes, { subtle, extractable, retainRing });
}

export async function generateCometIdentity({
  subtle,
  crypto: cryptoObject,
  sponsors,
  maxAttempts = sponsors ? 1_000_000 : 1_000,
  extractable = false,
  retainRing = false,
} = {}) {
  const sponsorSet = sponsors
    ? new Set(Array.from(sponsors, sponsor => BigInt(sponsor).toString()))
    : null;

  for (let attempt = 1; attempt <= maxAttempts; attempt++) {
    const identity = await generateSuiteBIdentity({
      subtle,
      crypto: cryptoObject,
      extractable,
      retainRing,
    });
    const sponsor = shipSponsor(identity.ship);
    const validSponsor = shipRank(sponsor) === 'king';

    if (
      isCometShip(identity.ship) &&
      validSponsor &&
      (sponsorSet === null || sponsorSet.has(sponsor.toString()))
    ) {
      return {
        ...identity,
        comet: identity.ship,
        sponsor,
        attempts: attempt,
      };
    }
  }

  throw new Error('failed to mine comet identity within maxAttempts');
}

export function encodeSuiteBRingBytes(seed) {
  const bytes = asBytes(seed);
  assertLength(bytes, SUITE_B.seedBytes, 'suite-B seed');
  return concatBytes(Uint8Array.of(SUITE_B.ringTag), bytes);
}

export function parseSuiteBRingBytes(bytes) {
  const ring = asBytes(bytes);
  assertLength(ring, SUITE_B.ringBytes, 'suite-B ring');
  if (ring[0] !== SUITE_B.ringTag) {
    throw new Error('bad suite-B ring tag');
  }

  return {
    signingSeed: ring.slice(1, 33),
    cryptoSeed: ring.slice(33, 65),
  };
}

export function encodeSuiteBPassBytes({ signingPublicKey, cryptoPublicKey }) {
  const signing = asBytes(signingPublicKey);
  const crypt = asBytes(cryptoPublicKey);
  assertLength(signing, SUITE_B.keyBytes, 'suite-B signing public key');
  assertLength(crypt, SUITE_B.keyBytes, 'suite-B crypto public key');
  return concatBytes(Uint8Array.of(SUITE_B.passTag), signing, crypt);
}

export function parseSuiteBPassBytes(bytes) {
  const pass = asBytes(bytes);
  assertLength(pass, SUITE_B.passBytes, 'suite-B pass');
  if (pass[0] !== SUITE_B.passTag) {
    throw new Error('bad suite-B pass tag');
  }

  return {
    signingPublicKey: pass.slice(1, 33),
    cryptoPublicKey: pass.slice(33, 65),
  };
}

export async function importPublicKeyRaw(bytes, { subtle } = {}) {
  return subtleCrypto(subtle).importKey(
    'raw',
    asBytes(bytes),
    ED25519,
    true,
    ['verify'],
  );
}

export async function importPrivateKeyPkcs8(bytes, {
  subtle,
  extractable = false,
} = {}) {
  return subtleCrypto(subtle).importKey(
    'pkcs8',
    asBytes(bytes),
    ED25519,
    extractable,
    ['sign'],
  );
}

export async function exportPublicKeyRaw(identityOrKey, { subtle } = {}) {
  return new Uint8Array(await subtleCrypto(subtle).exportKey(
    'raw',
    publicKey(identityOrKey),
  ));
}

export async function signBytes(identityOrKey, bytes, { subtle } = {}) {
  return new Uint8Array(await subtleCrypto(subtle).sign(
    ED25519,
    privateKey(identityOrKey),
    asBytes(bytes),
  ));
}

export async function verifyBytes(publicIdentityOrKeyOrBytes, signature, bytes, {
  subtle,
} = {}) {
  const crypto = subtleCrypto(subtle);
  const key = (publicIdentityOrKeyOrBytes instanceof Uint8Array)
    ? await importPublicKeyRaw(publicIdentityOrKeyOrBytes, { subtle: crypto })
    : publicKey(publicIdentityOrKeyOrBytes);

  return crypto.verify(
    ED25519,
    key,
    asBytes(signature),
    asBytes(bytes),
  );
}
