import assert from 'node:assert/strict';
import test from 'node:test';

import {
  decodeUdpFrame,
  encodeUdpHear,
  encodeUdpSend,
  UDP_FRAME,
  udpLaneNoun,
} from './ames-udp-frame.mjs';
import { termAtom, tuple } from './urbit-noun.mjs';

test('UDP gateway frames round-trip galaxy destinations', () => {
  const decoded = decodeUdpFrame(encodeUdpSend(0n, Uint8Array.of(1, 2, 3)));
  assert.deepEqual(decoded, {
    type: UDP_FRAME.SEND,
    lane: { type: 'galaxy', ship: 0 },
    packet: Uint8Array.of(1, 2, 3),
  });
});

test('UDP gateway frames round-trip IPv4 source lanes', () => {
  const lane = { type: 'if', ip: 0x7f000001, port: 13337 };
  const decoded = decodeUdpFrame(encodeUdpHear(lane, Uint8Array.of(4, 5)));
  assert.deepEqual(decoded, {
    type: UDP_FRAME.HEAR,
    lane,
    packet: Uint8Array.of(4, 5),
  });
  assert.deepEqual(
    udpLaneNoun(lane),
    tuple(termAtom('if'), 0x7f000001n, 13337n),
  );
});

test('UDP gateway frame decoder rejects malformed input', () => {
  assert.throws(() => decodeUdpFrame(Uint8Array.of(1, 2, 3)), /truncated/);
  const frame = encodeUdpSend(0n, Uint8Array.of(1));
  frame[0] = 0;
  assert.throws(() => decodeUdpFrame(frame), /magic/);
});
