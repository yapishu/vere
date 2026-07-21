import assert from 'node:assert/strict';
import test from 'node:test';

import { lssRoot } from './ames-blake3.mjs';
import {
  AmesChumPeer,
  buildChumPoke,
  makeChumPath,
  mesaBindingBytes,
  mesaFlowPath,
  openChumPoke,
  parseChumPath,
} from './ames-mesa-chum.mjs';
import {
  mesaDecryptBytes,
  mesaMacBinding,
  mesaOpenPath,
  mesaSealPath,
} from './ames-mesa-crypt.mjs';
import { AUTH, bytesToHex, decodePact } from './mesa-pact.mjs';
import { termAtom, tuple } from './urbit-noun.mjs';

test('mesaFlowPath renders Ames flow paths', () => {
  assert.equal(
    mesaFlowPath({
      bone: 7n,
      load: '%poke',
      direction: '%for',
      receiver: '~dozzod',
      sequence: 9n,
    }),
    '/a/x/1//flow/7/poke/for/~zod/9',
  );
  assert.equal(
    mesaFlowPath({
      bone: 7n,
      load: 'cork',
      direction: 'bak',
      receiver: 0x100n,
    }),
    '/a/x/1//flow/7/cork/bak/~marzod',
  );
});

test('makeChumPath matches Hoon make-space-path shape', () => {
  const chum = makeChumPath({
    key: 0x1234n,
    serverLife: 1n,
    clientPatp: '~dozzod',
    clientLife: 2n,
    path: '/a/x/1/base/sys/kelvin',
  });

  assert.equal(
    chum.path,
    '/chum/1/~zod/2/~.0vq.pnv1l.9hirn.aou99.2h8rs.0fotc.onj90.hn3p8.pikcm.f87o1.2iupa.e27o9.lrsfd',
  );
  assert.equal(chum.sealed, mesaSealPath(0x1234n, '/a/x/1/base/sys/kelvin'));

  const parsed = parseChumPath(
    '/chum/1/~dozzod/2/~.0vq.pnv1l.9hirn.aou99.2h8rs.0fotc.onj90.hn3p8.pikcm.f87o1.2iupa.e27o9.lrsfd',
    { key: 0x1234n },
  );
  assert.equal(parsed.clientPatp, '~zod');
  assert.equal(parsed.client, 0n);
  assert.equal(parsed.innerPath, '/a/x/1/base/sys/kelvin');
});

test('buildChumPoke emits an encrypted one-fragment %hmac poke', () => {
  const key = 0x1234n;
  const gage = tuple(termAtom('message'), termAtom('ack'), 0n);
  const ackInnerPath = mesaFlowPath({
    bone: 3n,
    load: 'ack',
    direction: 'bak',
    receiver: 0x100n,
    sequence: 1n,
  });
  const payloadInnerPath = mesaFlowPath({
    bone: 3n,
    load: 'poke',
    direction: 'for',
    receiver: 0n,
    sequence: 1n,
  });

  const built = buildChumPoke({
    key,
    sender: 0x100n,
    senderLife: 7n,
    senderRift: 2,
    receiver: 0n,
    receiverLife: 1n,
    receiverRift: 4,
    ackPath: ackInnerPath,
    payloadPath: payloadInnerPath,
    gage,
  });
  const decoded = decodePact(built.packet);

  assert.equal(decoded.type, 'poke');
  assert.equal(decoded.name.ship, 0n);
  assert.equal(decoded.name.rift, 4);
  assert.equal(decoded.name.auth, true);
  assert.equal(decoded.name.path, built.ackPath);
  assert.equal(decoded.payloadName.ship, 0x100n);
  assert.equal(decoded.payloadName.rift, 2);
  assert.equal(decoded.payloadName.path, built.payloadPath);
  assert.equal(decoded.data.auth.type, AUTH.HMAC);
  assert.equal(decoded.data.totalBytes, BigInt(decoded.data.fragment.length));
  assert.equal(decoded.data.fragment.length, built.plaintextBytes.length + 1);

  assert.equal(mesaOpenPath(key, built.ackSealed), ackInnerPath);
  assert.equal(mesaOpenPath(key, built.payloadSealed), payloadInnerPath);
  assert.deepEqual(
    Array.from(mesaDecryptBytes(key, built.payloadSealed, decoded.data.fragment)),
    Array.from(built.plaintextBytes),
  );

  const root = lssRoot(decoded.data.fragment);
  const binding = mesaBindingBytes({
    publisher: 0x100n,
    path: decoded.payloadName.path,
    root,
  });
  assert.equal(bytesToHex(root), bytesToHex(built.rootBytes));
  assert.equal(bytesToHex(binding), bytesToHex(built.binding));
  assert.equal(bytesToHex(mesaMacBinding(key, binding)), bytesToHex(decoded.data.auth.mac));

  const opened = openChumPoke(decoded, {
    key,
    local: 0n,
    localLife: 1n,
    remote: 0x100n,
    remoteLife: 7n,
    expectedAckPath: ackInnerPath,
    expectedPayloadPath: payloadInnerPath,
  });
  assert.deepEqual(opened.gage, gage);
  assert.equal(opened.ack.innerPath, ackInnerPath);
  assert.equal(opened.payload.innerPath, payloadInnerPath);
  assert.equal(bytesToHex(opened.mac), bytesToHex(decoded.data.auth.mac));
});

test('openChumPoke rejects bad authentication and wrong peer binding', () => {
  const built = buildChumPoke({
    key: 0x1234n,
    sender: 0x100n,
    senderLife: 7n,
    receiver: 0n,
    receiverLife: 1n,
    ackPath: '/a/x/1//flow/3/ack/bak/~marzod/1',
    payloadPath: '/a/x/1//flow/3/poke/for/~zod/1',
    gage: tuple(termAtom('message'), termAtom('ack'), 0n),
  });
  const decoded = decodePact(built.packet);

  assert.throws(
    () => openChumPoke(decoded, {
      key: 0x1234n,
      local: 0x100n,
      remote: 0n,
    }),
    /ack name is not addressed/,
  );

  decoded.data.auth.mac[0] ^= 1;
  assert.throws(
    () => openChumPoke(decoded, {
      key: 0x1234n,
      local: 0n,
      localLife: 1n,
      remote: 0x100n,
      remoteLife: 7n,
      decodeGage: false,
    }),
    /authentication failed/,
  );
});

test('AmesChumPeer sends and opens matching encrypted poke packets', async () => {
  const sent = [];
  const openedEvents = [];
  const errors = [];
  const peer = new AmesChumPeer({
    key: 0x1234n,
    local: 0x100n,
    localLife: 7n,
    localRift: 2,
    remote: 0n,
    remoteLife: 1n,
    remoteRift: 4,
    send: async (packet) => {
      sent.push(packet);
      return 'datagram';
    },
    onPoke: event => openedEvents.push(event),
    onError: event => errors.push(event),
  });

  const outbound = await peer.sendPoke({
    ackPath: '/a/x/1//flow/3/ack/bak/~marzod/1',
    payloadPath: '/a/x/1//flow/3/poke/for/~zod/1',
    gage: tuple(termAtom('message'), termAtom('ack'), 0n),
  });
  assert.equal(outbound.mode, 'datagram');
  assert.equal(sent.length, 1);
  assert.deepEqual(sent[0], outbound.packet);

  const inbound = buildChumPoke({
    key: 0x1234n,
    sender: 0n,
    senderLife: 1n,
    senderRift: 4,
    receiver: 0x100n,
    receiverLife: 7n,
    receiverRift: 2,
    ackPath: '/a/x/1//flow/3/ack/for/~zod/1',
    payloadPath: '/a/x/1//flow/3/poke/bak/~marzod/1',
    gage: tuple(termAtom('message'), termAtom('ack'), 0n),
  });
  const opened = peer.handlePacket({ mode: 'datagram', packet: inbound.packet });
  assert.deepEqual(opened.gage, tuple(termAtom('message'), termAtom('ack'), 0n));
  assert.equal(openedEvents.length, 1);
  assert.equal(errors.length, 0);

  assert.equal(peer.handlePacket(outbound.packet), null);
  assert.equal(errors.length, 0);

  const tampered = decodePact(inbound.packet);
  tampered.data.fragment[0] ^= 1;
  assert.equal(peer.handlePacket(tampered), null);
  assert.equal(errors.length, 1);
  assert.equal(errors[0].type, 'chum-poke-open');
});

test('buildChumPoke rejects oversized direct payloads', () => {
  assert.throws(
    () => buildChumPoke({
      key: 0x1234n,
      sender: 0x100n,
      receiver: 0n,
      receiverLife: 1n,
      ackPath: '/a/x/1//flow/1/ack/bak/~marzod/1',
      payloadPath: '/a/x/1//flow/1/poke/for/~zod/1',
      plaintextBytes: new Uint8Array(1024),
    }),
    /exceeded one Mesa fragment/,
  );
});
