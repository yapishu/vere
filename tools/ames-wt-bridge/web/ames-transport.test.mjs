import assert from 'node:assert/strict';
import test from 'node:test';

import { sendPacket } from './ames-transport.mjs';

function fakeWebTransport({ maxDatagramSize, rejectDatagrams = false } = {}) {
  const datagrams = [];
  const streams = [];

  const wt = {
    datagrams: {
      writable: {
        getWriter() {
          return {
            async write(packet) {
              if (rejectDatagrams) {
                throw new Error('datagram rejected');
              }
              if ((typeof maxDatagramSize === 'number') && (packet.length > maxDatagramSize)) {
                throw new Error('datagram too large');
              }
              datagrams.push(Uint8Array.from(packet));
            },
            releaseLock() {},
          };
        },
      },
    },
    async createUnidirectionalStream() {
      const chunks = [];
      streams.push(chunks);
      return new WritableStream({
        write(chunk) {
          chunks.push(Uint8Array.from(chunk));
        },
      });
    },
    datagramsSent: datagrams,
    streamsSent: streams,
  };

  if (maxDatagramSize !== undefined) {
    wt.datagrams.maxDatagramSize = maxDatagramSize;
  }

  return wt;
}

test('sendPacket uses datagrams when the packet fits', async () => {
  const wt = fakeWebTransport({ maxDatagramSize: 8 });
  const mode = await sendPacket(wt, Uint8Array.of(1, 2, 3));

  assert.equal(mode, 'datagram');
  assert.equal(wt.datagramsSent.length, 1);
  assert.deepEqual(Array.from(wt.datagramsSent[0]), [1, 2, 3]);
  assert.equal(wt.streamsSent.length, 0);
});

test('sendPacket uses a unidirectional stream when the packet is oversized', async () => {
  const wt = fakeWebTransport({ maxDatagramSize: 2 });
  const mode = await sendPacket(wt, Uint8Array.of(1, 2, 3));

  assert.equal(mode, 'stream');
  assert.equal(wt.datagramsSent.length, 0);
  assert.equal(wt.streamsSent.length, 1);
  assert.deepEqual(Array.from(wt.streamsSent[0][0]), [1, 2, 3]);
});

test('sendPacket falls back to stream when datagram size is unknown and write rejects', async () => {
  const wt = fakeWebTransport({ rejectDatagrams: true });
  const mode = await sendPacket(wt, Uint8Array.of(9, 8, 7));

  assert.equal(mode, 'stream');
  assert.equal(wt.datagramsSent.length, 0);
  assert.equal(wt.streamsSent.length, 1);
  assert.deepEqual(Array.from(wt.streamsSent[0][0]), [9, 8, 7]);
});

test('sendPacket surfaces datagram errors for packets that should fit', async () => {
  const wt = fakeWebTransport({ maxDatagramSize: 8, rejectDatagrams: true });
  await assert.rejects(() => sendPacket(wt, Uint8Array.of(1, 2, 3)), /datagram rejected/);
  assert.equal(wt.streamsSent.length, 0);
});
