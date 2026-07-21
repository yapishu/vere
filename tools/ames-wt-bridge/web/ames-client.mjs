import { sendPacket } from './ames-transport.mjs';

function asBytes(packet) {
  return packet instanceof Uint8Array ? packet : Uint8Array.from(packet);
}

export function base64ToBytes(text) {
  const str = text.trim();
  if (!str) {
    return undefined;
  }

  if (typeof atob === 'function') {
    return Uint8Array.from(atob(str), c => c.charCodeAt(0));
  }
  if (typeof Buffer !== 'undefined') {
    return Uint8Array.from(Buffer.from(str, 'base64'));
  }

  throw new Error('no base64 decoder available');
}

export function webTransportOptions({ certificateHash } = {}) {
  const opts = {};
  const value =
    (typeof certificateHash === 'string') ? base64ToBytes(certificateHash) : certificateHash;

  if (value !== undefined) {
    opts.serverCertificateHashes = [{ algorithm: 'sha-256', value }];
  }

  return opts;
}

export async function readStreamBytes(stream) {
  const reader = stream.getReader();
  const chunks = [];
  let total = 0;

  try {
    for (;;) {
      const { value, done } = await reader.read();
      if (done) {
        break;
      }

      const chunk = asBytes(value);
      chunks.push(chunk);
      total += chunk.length;
    }
  }
  finally {
    reader.releaseLock?.();
  }

  const out = new Uint8Array(total);
  let off = 0;
  for (const chunk of chunks) {
    out.set(chunk, off);
    off += chunk.length;
  }

  return out;
}

export class AmesWebTransportClient {
  #WebTransport;
  #connectPromise = null;
  #closed = false;
  #session = null;
  #token = 0;

  constructor({
    url,
    certificateHash,
    options,
    WebTransport = globalThis.WebTransport,
    onPacket = () => {},
    onStatus = () => {},
    onError = () => {},
  } = {}) {
    if (!url) {
      throw new Error('url is required');
    }
    if (typeof WebTransport !== 'function') {
      throw new Error('WebTransport is not available');
    }

    this.url = url;
    this.options = options ?? webTransportOptions({ certificateHash });
    this.onPacket = onPacket;
    this.onStatus = onStatus;
    this.onError = onError;
    this.#WebTransport = WebTransport;
  }

  get sessionOpen() {
    return this.#session !== null;
  }

  async connect() {
    if (this.#closed) {
      throw new Error('client is closed');
    }
    if (this.#session !== null) {
      return this.#session;
    }
    if (this.#connectPromise !== null) {
      return this.#connectPromise;
    }

    const token = ++this.#token;
    this.onStatus({ type: 'connecting', url: this.url });

    const promise = this.#open(token);
    this.#connectPromise = promise;
    try {
      return await promise;
    }
    finally {
      if (this.#connectPromise === promise) {
        this.#connectPromise = null;
      }
    }
  }

  async send(packet) {
    const session = await this.connect();
    return sendPacket(session, packet);
  }

  async close({ closeCode = 0, reason = 'closed' } = {}) {
    this.#closed = true;
    this.#token++;

    const session = this.#session;
    this.#session = null;
    this.#connectPromise = null;

    if (session?.close) {
      session.close({ closeCode, reason });
    }

    try {
      await session?.closed;
    }
    catch (_) {
      // Closing locally is terminal for this client; transport close errors do
      // not change the caller-visible result.
    }

    this.onStatus({ type: 'closed', url: this.url, terminal: true });
  }

  async #open(token) {
    let session;
    try {
      session = new this.#WebTransport(this.url, this.options);
      await session.ready;
    }
    catch (error) {
      this.onError({ type: 'connect', error });
      throw error;
    }

    if (this.#closed || token !== this.#token) {
      session.close?.({ closeCode: 0, reason: 'superseded' });
      throw new Error('connection superseded');
    }

    this.#session = session;
    this.onStatus({ type: 'open', url: this.url });
    this.#receiveDatagrams(session);
    this.#receiveStreams(session);
    this.#watchClosed(session);
    return session;
  }

  #markClosed(session) {
    if (this.#session !== session) {
      return;
    }

    this.#session = null;
    this.onStatus({ type: 'closed', url: this.url, terminal: false });
  }

  #emitPacket(session, mode, packet) {
    if (this.#session === session) {
      this.onPacket({ mode, packet: asBytes(packet) });
    }
  }

  #emitError(session, type, error) {
    if (this.#session === session) {
      this.onError({ type, error });
    }
  }

  async #receiveDatagrams(session) {
    const readable = session.datagrams?.readable;
    if (!readable) {
      return;
    }

    const reader = readable.getReader();
    try {
      for (;;) {
        const { value, done } = await reader.read();
        if (done) {
          break;
        }
        this.#emitPacket(session, 'datagram', value);
      }
    }
    catch (error) {
      this.#emitError(session, 'receive-datagram', error);
    }
    finally {
      reader.releaseLock?.();
      this.#markClosed(session);
    }
  }

  async #receiveStreams(session) {
    const incoming = session.incomingUnidirectionalStreams;
    if (!incoming) {
      return;
    }

    const reader = incoming.getReader();
    try {
      for (;;) {
        const { value: stream, done } = await reader.read();
        if (done) {
          break;
        }

        try {
          this.#emitPacket(session, 'stream', await readStreamBytes(stream));
        }
        catch (error) {
          this.#emitError(session, 'receive-stream', error);
        }
      }
    }
    catch (error) {
      this.#emitError(session, 'receive-stream-list', error);
    }
    finally {
      reader.releaseLock?.();
      this.#markClosed(session);
    }
  }

  async #watchClosed(session) {
    try {
      await session.closed;
    }
    catch (error) {
      this.#emitError(session, 'session-closed', error);
    }
    finally {
      this.#markClosed(session);
    }
  }
}
