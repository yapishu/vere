import assert from 'node:assert/strict';
import test from 'node:test';

import {
  AmesWebTransportClient,
  base64ToBytes,
  readStreamBytes,
  webTransportOptions,
} from './ames-client.mjs';
import {
  decodeUdpFrame,
  encodeUdpHear,
  UDP_FRAME,
} from './ames-udp-frame.mjs';

const tick = () => new Promise(resolve => setTimeout(resolve, 0));

function makeReadableQueue() {
  const stream = new TransformStream();
  const writer = stream.writable.getWriter();
  let closed = false;

  return {
    readable: stream.readable,
    async push(value) {
      assert.equal(closed, false);
      await writer.write(value);
    },
    async close() {
      if (!closed) {
        closed = true;
        await writer.close();
      }
    },
    async abort(error) {
      if (!closed) {
        closed = true;
        await writer.abort(error);
      }
    },
  };
}

function streamFrom(chunks) {
  return new ReadableStream({
    start(controller) {
      for (const chunk of chunks) {
        controller.enqueue(Uint8Array.from(chunk));
      }
      controller.close();
    },
  });
}

function makeFakeWebTransportClass({ maxDatagramSize = 1200 } = {}) {
  return class FakeWebTransport {
    static instances = [];

    constructor(url, options) {
      this.url = url;
      this.options = options;
      this.ready = Promise.resolve();
      this.datagramsSent = [];
      this.streamsSent = [];
      this.#datagramQueue = makeReadableQueue();
      this.#streamQueue = makeReadableQueue();
      this.datagrams = {
        maxDatagramSize,
        readable: this.#datagramQueue.readable,
        writable: {
          getWriter: () => ({
            write: async packet => {
              if (packet.length > maxDatagramSize) {
                throw new Error('datagram too large');
              }
              this.datagramsSent.push(Uint8Array.from(packet));
            },
            releaseLock() {},
          }),
        },
      };
      this.incomingUnidirectionalStreams = this.#streamQueue.readable;
      this.closed = new Promise((resolve, reject) => {
        this.#resolveClosed = resolve;
        this.#rejectClosed = reject;
      });
      FakeWebTransport.instances.push(this);
    }

    #datagramQueue;
    #streamQueue;
    #resolveClosed;
    #rejectClosed;
    #closed = false;

    async createUnidirectionalStream() {
      const chunks = [];
      this.streamsSent.push(chunks);
      return new WritableStream({
        write(chunk) {
          chunks.push(Uint8Array.from(chunk));
        },
      });
    }

    async pushDatagram(packet) {
      await this.#datagramQueue.push(Uint8Array.from(packet));
    }

    async pushStream(chunks) {
      await this.#streamQueue.push(streamFrom(chunks));
    }

    async remoteClose() {
      if (this.#closed) {
        return;
      }
      this.#closed = true;
      this.#resolveClosed({ closeCode: 0, reason: 'remote' });
      await Promise.all([
        this.#datagramQueue.close(),
        this.#streamQueue.close(),
      ]);
    }

    async remoteError(error) {
      if (this.#closed) {
        return;
      }
      this.#closed = true;
      this.#rejectClosed(error);
      await Promise.all([
        this.#datagramQueue.abort(error),
        this.#streamQueue.abort(error),
      ]);
    }

    close({ closeCode = 0, reason = 'local' } = {}) {
      if (this.#closed) {
        return;
      }
      this.#closed = true;
      this.#resolveClosed({ closeCode, reason });
      this.#datagramQueue.close();
      this.#streamQueue.close();
    }
  };
}

test('base64ToBytes and webTransportOptions build the browser pin option', () => {
  assert.deepEqual(Array.from(base64ToBytes('AQIDBA==')), [1, 2, 3, 4]);

  const opts = webTransportOptions({ certificateHash: 'BQYH' });
  assert.equal(opts.serverCertificateHashes[0].algorithm, 'sha-256');
  assert.deepEqual(Array.from(opts.serverCertificateHashes[0].value), [5, 6, 7]);
});

test('readStreamBytes concatenates one-shot packet streams', async () => {
  const bytes = await readStreamBytes(streamFrom([[1, 2], [3], [4, 5]]));
  assert.deepEqual(Array.from(bytes), [1, 2, 3, 4, 5]);
});

test('client connects with cert pin options and receives datagrams', async () => {
  const FakeWebTransport = makeFakeWebTransportClass();
  const packets = [];
  const statuses = [];
  const client = new AmesWebTransportClient({
    url: 'https://127.0.0.1:8443/~_~/ames',
    certificateHash: 'AQID',
    WebTransport: FakeWebTransport,
    onPacket: packet => packets.push(packet),
    onStatus: status => statuses.push(status.type),
  });

  await client.connect();
  const session = FakeWebTransport.instances[0];

  assert.deepEqual(
    Array.from(session.options.serverCertificateHashes[0].value),
    [1, 2, 3],
  );
  assert.deepEqual(statuses, ['connecting', 'open']);
  assert.equal(client.sessionOpen, true);

  await session.pushDatagram(encodeUdpHear(
    { type: 'if', ip: 0x7f000001, port: 13337 },
    [8, 9],
  ));
  await tick();

  assert.equal(packets.length, 1);
  assert.equal(packets[0].mode, 'datagram');
  assert.deepEqual(packets[0].lane, {
    type: 'if',
    ip: 0x7f000001,
    port: 13337,
  });
  assert.deepEqual(Array.from(packets[0].packet), [8, 9]);
});

test('client sends through datagrams or one-shot streams', async () => {
  const FakeWebTransport = makeFakeWebTransportClass({ maxDatagramSize: 11 });
  const client = new AmesWebTransportClient({
    url: 'https://bridge/~_~/ames',
    WebTransport: FakeWebTransport,
  });

  assert.equal(await client.sendTo(0n, Uint8Array.of(1, 2)), 'datagram');
  assert.equal(await client.sendTo(0n, Uint8Array.of(3, 4, 5)), 'stream');

  const session = FakeWebTransport.instances[0];
  assert.equal(FakeWebTransport.instances.length, 1);
  assert.deepEqual(decodeUdpFrame(session.datagramsSent[0]), {
    type: UDP_FRAME.SEND,
    lane: { type: 'galaxy', ship: 0 },
    packet: Uint8Array.of(1, 2),
  });
  assert.deepEqual(decodeUdpFrame(session.streamsSent[0][0]), {
    type: UDP_FRAME.SEND,
    lane: { type: 'galaxy', ship: 0 },
    packet: Uint8Array.of(3, 4, 5),
  });
});

test('client receives one-shot stream packets', async () => {
  const FakeWebTransport = makeFakeWebTransportClass();
  const packets = [];
  const client = new AmesWebTransportClient({
    url: 'https://bridge/~_~/ames',
    WebTransport: FakeWebTransport,
    onPacket: packet => packets.push(packet),
  });

  await client.connect();
  const frame = encodeUdpHear(
    { type: 'if', ip: 0x7f000001, port: 13337 },
    [1, 2, 3],
  );
  await FakeWebTransport.instances[0].pushStream([
    frame.slice(0, 5),
    frame.slice(5),
  ]);
  await tick();

  assert.equal(packets.length, 1);
  assert.equal(packets[0].mode, 'stream');
  assert.deepEqual(Array.from(packets[0].packet), [1, 2, 3]);
});

test('client redials on next send after remote close', async () => {
  const FakeWebTransport = makeFakeWebTransportClass();
  const statuses = [];
  const client = new AmesWebTransportClient({
    url: 'https://bridge/~_~/ames',
    WebTransport: FakeWebTransport,
    onStatus: status => statuses.push(status.type),
  });

  await client.sendTo(0n, Uint8Array.of(1));
  const first = FakeWebTransport.instances[0];
  await first.remoteClose();
  await tick();

  assert.equal(client.sessionOpen, false);

  await client.sendTo(0n, Uint8Array.of(2));
  const second = FakeWebTransport.instances[1];

  assert.notEqual(first, second);
  assert.deepEqual(Array.from(decodeUdpFrame(second.datagramsSent[0]).packet), [2]);
  assert.deepEqual(statuses, ['connecting', 'open', 'closed', 'connecting', 'open']);
});

test('client close is terminal', async () => {
  const FakeWebTransport = makeFakeWebTransportClass();
  const statuses = [];
  const client = new AmesWebTransportClient({
    url: 'https://bridge/~_~/ames',
    WebTransport: FakeWebTransport,
    onStatus: status => statuses.push(status),
  });

  await client.connect();
  await client.close({ reason: 'done' });

  assert.equal(client.sessionOpen, false);
  assert.equal(statuses.at(-1).type, 'closed');
  assert.equal(statuses.at(-1).terminal, true);
  await assert.rejects(() => client.sendTo(0n, Uint8Array.of(1)), /client is closed/);
});
