import {
  atomFromBytesLE,
  bytesFromAtomLE,
  cell,
  cue,
  jamBytes,
  list,
  termAtom,
  tuple,
} from './urbit-noun.mjs';

const textDecoder = new TextDecoder();
const textEncoder = new TextEncoder();

export const DEFAULT_HTTP_SERVER_SERVICE = '0v1n.2m9vh';
export const LOOPBACK_IPV4_ATOM = 0x0100007fn;

const TERMS = Object.freeze({
  born: termAtom('born'),
  cancel: termAtom('cancel'),
  cancelRequest: termAtom('cancel-request'),
  continue: termAtom('continue'),
  grow: termAtom('grow'),
  httpServer: termAtom('http-server'),
  ipv4: termAtom('ipv4'),
  live: termAtom('live'),
  request: termAtom('request'),
  requestLocal: termAtom('request-local'),
  response: termAtom('response'),
  sessions: termAtom('sessions'),
  setConfig: termAtom('set-config'),
  start: termAtom('start'),
});

function isCell(noun) {
  return Array.isArray(noun) && noun.length === 2;
}

function decodeEffectsInput(input) {
  if (input instanceof Uint8Array) {
    return cue(atomFromBytesLE(input));
  }
  if (input instanceof ArrayBuffer) {
    return cue(atomFromBytesLE(new Uint8Array(input)));
  }
  if (ArrayBuffer.isView(input)) {
    return cue(atomFromBytesLE(
      new Uint8Array(input.buffer, input.byteOffset, input.byteLength),
    ));
  }
  return input;
}

function listItems(noun, name) {
  const out = [];
  let cur = noun;
  while (cur !== 0n) {
    if (!isCell(cur)) {
      throw new Error(`${name} must be a Hoon list`);
    }
    out.push(cur[0]);
    cur = cur[1];
  }
  return out;
}

function listFrom(items) {
  let out = 0n;
  for (let i = items.length - 1; i >= 0; i--) {
    out = cell(items[i], out);
  }
  return out;
}

function tupleItems(noun, count) {
  const out = [];
  let cur = noun;
  for (let i = 0; i < count - 1; i++) {
    if (!isCell(cur)) {
      return null;
    }
    out.push(cur[0]);
    cur = cur[1];
  }
  out.push(cur);
  return out;
}

function atomText(atom) {
  return textDecoder.decode(bytesFromAtomLE(atom));
}

function textAtom(text) {
  return atomFromBytesLE(textEncoder.encode(String(text)));
}

function safeLength(atom, name) {
  const length = Number(atom);
  if (!Number.isSafeInteger(length) || length < 0) {
    throw new Error(`${name} must be a non-negative safe integer`);
  }
  return length;
}

function bytesFromOcts(noun, name) {
  if (!isCell(noun)) {
    throw new Error(`${name} must be an octs pair`);
  }
  return bytesFromAtomLE(noun[1], safeLength(noun[0], `${name} length`));
}

function bytesToOcts(bytes) {
  const value = bytes == null ? new Uint8Array() : Uint8Array.from(bytes);
  return cell(BigInt(value.length), atomFromBytesLE(value));
}

function unitBytes(bytes) {
  if (bytes == null) {
    return 0n;
  }
  return cell(0n, bytesToOcts(bytes));
}

function unitValue(value) {
  if (value == null) {
    return 0n;
  }
  return cell(0n, BigInt(value));
}

function loobean(value) {
  return value ? 0n : 1n;
}

function boolFromLoobean(noun) {
  return noun === 0n;
}

function decodeUnitBytes(unit, name) {
  if (unit === 0n) {
    return null;
  }
  if (!isCell(unit) || unit[0] !== 0n) {
    throw new Error(`${name} must be a unit octs`);
  }
  return bytesFromOcts(unit[1], name);
}

function decodeHeaderList(noun) {
  return listItems(noun, 'http headers').map(item => {
    if (!isCell(item)) {
      throw new Error('http header must be a pair');
    }
    return [atomText(item[0]), atomText(item[1])];
  });
}

function encodeHeaderList(headers = []) {
  const pairs = [];
  for (const header of headers) {
    const [key, value] = Array.isArray(header)
      ? header
      : [header.key, header.value];
    pairs.push(cell(textAtom(key), textAtom(value)));
  }
  return listFrom(pairs);
}

function addressNoun(address) {
  if (address == null || typeof address === 'bigint' || typeof address === 'number') {
    return tuple(TERMS.ipv4, BigInt(address ?? LOOPBACK_IPV4_ATOM));
  }
  if (address.type === 'ipv4') {
    return tuple(TERMS.ipv4, BigInt(address.value));
  }
  throw new Error(`unsupported http-server address type: ${address.type}`);
}

function requestNoun({
  method = 'GET',
  url = '/',
  headers = [],
  body = null,
} = {}) {
  return tuple(
    textAtom(method),
    textAtom(url),
    encodeHeaderList(headers),
    unitBytes(body),
  );
}

function runtimeOvum({ kind = 'e', wire, card }) {
  if (!wire) {
    throw new Error('wire is required');
  }
  return cell(cell(termAtom(kind), wire), card);
}

function wireTailItems(wire) {
  if (!isCell(wire) || wire[0] !== TERMS.httpServer) {
    return null;
  }
  return listItems(wire[1], 'http-server wire');
}

function requestKey({ service, connectionId, requestId }) {
  return `${service}/${BigInt(connectionId)}/${BigInt(requestId)}`;
}

function decodedWire(wire) {
  const items = wireTailItems(wire);
  if (!items || items.length < 1) {
    return null;
  }
  const out = {
    service: atomText(items[0]),
    connectionId: 0n,
    requestId: 0n,
  };
  if (items.length >= 2) {
    out.connectionId = BigInt(atomText(items[1]));
  }
  if (items.length >= 3) {
    out.requestId = BigInt(atomText(items[2]));
  }
  return out;
}

function decodeResponseEvent(event) {
  if (!isCell(event)) {
    throw new Error('http-server response event must be a cell');
  }

  if (event[0] === TERMS.start) {
    const parts = tupleItems(event[1], 3);
    if (!parts) {
      throw new Error('http-server %start response must have header, body, complete');
    }
    const header = tupleItems(parts[0], 2);
    if (!header) {
      throw new Error('http-server %start header must contain status and headers');
    }
    return {
      type: 'start',
      status: Number(header[0]),
      headers: decodeHeaderList(header[1]),
      body: decodeUnitBytes(parts[1], 'http-server start body'),
      complete: boolFromLoobean(parts[2]),
    };
  }

  if (event[0] === TERMS.continue) {
    const parts = tupleItems(event[1], 2);
    if (!parts) {
      throw new Error('http-server %continue response must have body and complete');
    }
    return {
      type: 'continue',
      body: decodeUnitBytes(parts[0], 'http-server continue body'),
      complete: boolFromLoobean(parts[1]),
    };
  }

  if (event[0] === TERMS.cancel) {
    return {
      type: 'cancel',
    };
  }

  return null;
}

export function httpServerWire({
  service = DEFAULT_HTTP_SERVER_SERVICE,
  connectionId = null,
  requestId = null,
} = {}) {
  const items = [textAtom(service)];
  if (connectionId != null) {
    items.push(textAtom(BigInt(connectionId).toString()));
  }
  if (requestId != null) {
    items.push(textAtom(BigInt(requestId).toString()));
  }
  return cell(TERMS.httpServer, list(...items));
}

export function httpServerBornOvumJam({
  service = DEFAULT_HTTP_SERVER_SERVICE,
  kind = 'e',
} = {}) {
  return jamBytes(runtimeOvum({
    kind,
    wire: httpServerWire({ service }),
    card: cell(TERMS.born, 0n),
  }));
}

export function httpServerLiveOvumJam({
  service = DEFAULT_HTTP_SERVER_SERVICE,
  insecurePort = 8080,
  securePort = null,
  kind = 'e',
} = {}) {
  return jamBytes(runtimeOvum({
    kind,
    wire: httpServerWire({ service }),
    card: tuple(
      TERMS.live,
      BigInt(insecurePort),
      unitValue(securePort),
    ),
  }));
}

export function httpServerRequestOvumJam({
  service = DEFAULT_HTTP_SERVER_SERVICE,
  connectionId,
  requestId,
  secure = false,
  local = true,
  address = LOOPBACK_IPV4_ATOM,
  method = 'GET',
  url = '/',
  headers = [],
  body = null,
  kind = 'e',
} = {}) {
  if (connectionId == null) {
    throw new Error('connectionId is required');
  }
  if (requestId == null) {
    throw new Error('requestId is required');
  }

  return jamBytes(runtimeOvum({
    kind,
    wire: httpServerWire({ service, connectionId, requestId }),
    card: tuple(
      local ? TERMS.requestLocal : TERMS.request,
      loobean(secure),
      addressNoun(address),
      requestNoun({ method, url, headers, body }),
    ),
  }));
}

export function httpServerCancelRequestOvumJam({
  service = DEFAULT_HTTP_SERVER_SERVICE,
  connectionId,
  requestId,
  kind = 'e',
} = {}) {
  if (connectionId == null) {
    throw new Error('connectionId is required');
  }
  if (requestId == null) {
    throw new Error('requestId is required');
  }

  return jamBytes(runtimeOvum({
    kind,
    wire: httpServerWire({ service, connectionId, requestId }),
    card: cell(TERMS.cancelRequest, 0n),
  }));
}

export function extractHttpServerEffects(input) {
  const effects = decodeEffectsInput(input);
  const out = {
    responses: [],
    configs: [],
    sessions: [],
    grows: [],
    unknown: [],
  };

  for (const effect of listItems(effects, 'effects')) {
    if (!isCell(effect)) {
      out.unknown.push({ effect, reason: 'effect is not a cell' });
      continue;
    }

    const wire = decodedWire(effect[0]);
    if (!wire) {
      out.unknown.push({ effect, reason: 'not an http-server wire' });
      continue;
    }

    const card = effect[1];
    if (!isCell(card)) {
      out.unknown.push({ effect, wire, reason: 'http-server card is not a cell' });
      continue;
    }

    if (card[0] === TERMS.response) {
      const response = decodeResponseEvent(card[1]);
      if (response) {
        out.responses.push({ wire, response, raw: effect });
      }
      else {
        out.unknown.push({ effect, wire, reason: 'unknown http-server response event' });
      }
      continue;
    }

    if (card[0] === TERMS.setConfig) {
      out.configs.push({ wire, config: card[1], raw: effect });
      continue;
    }

    if (card[0] === TERMS.sessions) {
      out.sessions.push({ wire, sessions: card[1], raw: effect });
      continue;
    }

    if (card[0] === TERMS.grow) {
      out.grows.push({ wire, path: card[1], raw: effect });
      continue;
    }

    out.unknown.push({ effect, wire, reason: 'not an http-server card' });
  }

  return out;
}

function headersObject(headers) {
  const out = {};
  for (const [key, value] of headers) {
    out[key] = value;
  }
  return out;
}

class PendingHttpResponse {
  constructor({ wire, timeoutMs, onTimeout }) {
    this.wire = wire;
    this.status = null;
    this.headers = [];
    this.chunks = [];
    this.started = false;
    this.finished = false;
    this.promise = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
    this.timer = timeoutMs == null ? null : setTimeout(() => {
      if (!this.finished) {
        onTimeout();
        this.cancel(new Error(`http-server request timed out: ${requestKey(wire)}`));
      }
    }, timeoutMs);
  }

  apply(response) {
    if (this.finished) {
      return;
    }

    if (response.type === 'cancel') {
      this.#finish(() => {
        this.reject(new Error(`http-server request canceled: ${requestKey(this.wire)}`));
      });
      return;
    }

    if (response.type === 'start') {
      this.started = true;
      this.status = response.status;
      this.headers = response.headers;
      if (response.body) {
        this.chunks.push(response.body);
      }
      if (response.complete) {
        this.#resolve();
      }
      return;
    }

    if (response.type === 'continue') {
      if (!this.started) {
        this.started = true;
        this.status = 200;
      }
      if (response.body) {
        this.chunks.push(response.body);
      }
      if (response.complete) {
        this.#resolve();
      }
    }
  }

  cancel(reason) {
    if (this.finished) {
      return;
    }
    this.#finish(() => {
      this.reject(reason instanceof Error ? reason : new Error(String(reason)));
    });
  }

  #resolve() {
    this.#finish(() => {
      const body = new Uint8Array(this.chunks.reduce((sum, chunk) => sum + chunk.length, 0));
      let offset = 0;
      for (const chunk of this.chunks) {
        body.set(chunk, offset);
        offset += chunk.length;
      }
      this.resolve({
        status: this.status ?? 200,
        headers: this.headers,
        body,
      });
    });
  }

  #finish(callback) {
    this.finished = true;
    if (this.timer) {
      clearTimeout(this.timer);
      this.timer = null;
    }
    callback();
  }
}

export class BrowserHttpServerHost {
  constructor({
    service = DEFAULT_HTTP_SERVER_SERVICE,
    insecurePort = 8080,
    securePort = null,
    local = true,
    address = LOOPBACK_IPV4_ATOM,
    requestTimeoutMs = 600_000,
    onLog = () => {},
  } = {}) {
    this.service = service;
    this.insecurePort = insecurePort;
    this.securePort = securePort;
    this.local = local;
    this.address = address;
    this.requestTimeoutMs = requestTimeoutMs;
    this.onLog = onLog;
    this.nextConnectionId = 1n;
    this.nextRequestId = 1n;
    this.pending = new Map();
    this.configured = false;
    this.sessionCount = 0;
    this.cacheGrows = 0;
  }

  snapshot() {
    return {
      service: this.service,
      pending: this.pending.size,
      configured: this.configured,
      sessionCount: this.sessionCount,
      cacheGrows: this.cacheGrows,
    };
  }

  bornOvumJam() {
    return httpServerBornOvumJam({ service: this.service });
  }

  liveOvumJam() {
    return httpServerLiveOvumJam({
      service: this.service,
      insecurePort: this.insecurePort,
      securePort: this.securePort,
    });
  }

  async handleRequest({
    method = 'GET',
    url = '/',
    headers = [],
    body = null,
    secure = false,
    local = this.local,
    address = this.address,
    timeoutMs = this.requestTimeoutMs,
    signal = null,
  } = {}, {
    injectOvum,
  } = {}) {
    if (typeof injectOvum !== 'function') {
      throw new Error('injectOvum callback is required');
    }

    const wire = {
      service: this.service,
      connectionId: this.nextConnectionId++,
      requestId: this.nextRequestId++,
    };
    const key = requestKey(wire);
    const pending = new PendingHttpResponse({
      wire,
      timeoutMs,
      onTimeout: () => this.pending.delete(key),
    });
    this.pending.set(key, pending);

    const abort = () => {
      if (!this.pending.has(key)) {
        return;
      }
      pending.cancel(new Error(`http-server request aborted: ${key}`));
      injectOvum({
        label: `http-server-cancel-${wire.connectionId}-${wire.requestId}`,
        ovumBytes: httpServerCancelRequestOvumJam(wire),
      }).catch(error => {
        this.onLog({
          message: `http-server cancel failed ${key}: ${error}`,
          className: 'err',
        });
      });
      this.pending.delete(key);
    };
    signal?.addEventListener?.('abort', abort, { once: true });

    try {
      this.onLog({
        message: `http-server request ${key} ${method} ${url}`,
        className: 'tx',
      });
      await injectOvum({
        label: `http-server-request-${wire.connectionId}-${wire.requestId}`,
        ovumBytes: httpServerRequestOvumJam({
          service: this.service,
          connectionId: wire.connectionId,
          requestId: wire.requestId,
          secure,
          local,
          address,
          method,
          url,
          headers,
          body,
        }),
      });
      const response = await pending.promise;
      this.onLog({
        message: `http-server response ${key} status=${response.status}`,
        className: 'rx',
      });
      return response;
    }
    finally {
      signal?.removeEventListener?.('abort', abort);
      this.pending.delete(key);
    }
  }

  async handleFetch(request, options = {}) {
    const response = await this.handleRequest({
      method: request.method,
      url: new URL(request.url).pathname + new URL(request.url).search,
      headers: request.headers && typeof request.headers[Symbol.iterator] === 'function'
        ? [...request.headers]
        : [],
      body: request.method === 'GET' || request.method === 'HEAD'
        ? null
        : new Uint8Array(await request.arrayBuffer()),
      signal: request.signal,
      ...options,
    }, options);

    if (typeof Response !== 'function') {
      return response;
    }
    return new Response(
      request.method === 'HEAD' ? null : response.body,
      {
        status: response.status,
        headers: headersObject(response.headers),
      },
    );
  }

  routeEffects(input) {
    const effects = extractHttpServerEffects(input);
    for (const item of effects.configs) {
      this.configured = true;
      this.onLog({
        message: `http-server config from ${item.wire.service}`,
        className: 'rx',
      });
    }
    for (const item of effects.sessions) {
      try {
        this.sessionCount = listItems(item.sessions, 'http-server sessions').length;
      }
      catch (error) {
        this.sessionCount = null;
        this.onLog({
          message: `http-server sessions gift has unknown shape: ${error}`,
          className: 'err',
        });
      }
    }
    for (const _item of effects.grows) {
      this.cacheGrows++;
    }
    for (const item of effects.responses) {
      const key = requestKey(item.wire);
      const pending = this.pending.get(key);
      if (!pending) {
        this.onLog({
          message: `http-server dropped response for unknown request ${key}`,
          className: 'err',
        });
        continue;
      }
      pending.apply(item.response);
    }

    return {
      responses: effects.responses.length,
      configs: effects.configs.length,
      sessions: effects.sessions.length,
      grows: effects.grows.length,
      unknown: effects.unknown.length,
      pending: this.pending.size,
    };
  }

  cancelAll() {
    for (const pending of this.pending.values()) {
      pending.cancel(new Error('http-server host closed'));
    }
    this.pending.clear();
  }
}
