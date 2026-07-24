import {
  atomFromBytesLE,
  atomBytesLE,
  bytesFromAtomLE,
  cell,
  cue,
  jamBytes,
  termAtom,
  tuple,
} from './urbit-noun.mjs';

const textDecoder = new TextDecoder();
const textEncoder = new TextEncoder();

export const DEFAULT_HTTP_CLIENT_SERVICE = '0v1n.2m9vh';

const TERMS = Object.freeze({
  cancelRequest: termAtom('cancel-request'),
  born: termAtom('born'),
  continue: termAtom('continue'),
  httpClient: termAtom('http-client'),
  receive: termAtom('receive'),
  request: termAtom('request'),
  start: termAtom('start'),
  cancel: termAtom('cancel'),
});

function isCell(noun) {
  return Array.isArray(noun) && noun.length === 2;
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

function wirePayload(wire) {
  return isCell(wire) && wire[0] === 0n ? wire[1] : wire;
}

function isHttpClientWire(wire) {
  const parts = tupleItems(wirePayload(wire), 3);
  return parts?.[0] === TERMS.httpClient;
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
  return cell(BigInt(value.length), atomBytesLE(value));
}

function unitBytes(bytes) {
  if (bytes == null) {
    return 0n;
  }
  return cell(0n, bytesToOcts(bytes));
}

function loobean(value) {
  return value ? 0n : 1n;
}

function decodeBody(unit) {
  if (unit === 0n) {
    return null;
  }
  if (!isCell(unit)) {
    throw new Error('http request body must be a unit');
  }
  return bytesFromOcts(unit[1], 'http request body');
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

function decodeRequest(card, wire, raw) {
  const parts = tupleItems(card, 3);
  if (parts?.[0] !== TERMS.request) {
    return null;
  }

  const request = tupleItems(parts[2], 4);
  if (!request) {
    throw new Error('%http-client request must contain a +request:http');
  }

  return {
    type: 'request',
    id: parts[1],
    method: atomText(request[0]),
    url: atomText(request[1]),
    headers: decodeHeaderList(request[2]),
    body: decodeBody(request[3]),
    wire,
    card,
    raw,
  };
}

function decodeCancel(card, wire, raw) {
  const parts = tupleItems(card, 2);
  if (parts?.[0] !== TERMS.cancelRequest) {
    return null;
  }

  return {
    type: 'cancel-request',
    id: parts[1],
    wire,
    card,
    raw,
  };
}

export function httpClientWire(service = DEFAULT_HTTP_CLIENT_SERVICE) {
  return tuple(TERMS.httpClient, textAtom(service), 0n);
}

export function httpClientBornTask() {
  return cell(TERMS.born, 0n);
}

export function httpClientBornOvumJam({
  wire = httpClientWire(),
  kind = 'i',
} = {}) {
  return jamBytes(runtimeOvum({
    kind,
    wire,
    card: httpClientBornTask(),
  }));
}

export function httpRequestNoun({
  method = 'GET',
  url,
  headers = [],
  body = null,
} = {}) {
  if (!url) {
    throw new Error('url is required');
  }
  return tuple(
    textAtom(method),
    textAtom(url),
    encodeHeaderList(headers),
    unitBytes(body),
  );
}

export function httpClientReceiveTask({
  id,
  status,
  headers = [],
  body = null,
  complete = true,
} = {}) {
  if (id == null) {
    throw new Error('id is required');
  }
  if (status == null) {
    throw new Error('status is required');
  }

  return tuple(
    TERMS.receive,
    BigInt(id),
    tuple(
      TERMS.start,
      tuple(BigInt(status), encodeHeaderList(headers)),
      unitBytes(body),
      loobean(complete),
    ),
  );
}

export function httpClientContinueTask({
  id,
  body = null,
  complete = true,
} = {}) {
  if (id == null) {
    throw new Error('id is required');
  }

  return tuple(
    TERMS.receive,
    BigInt(id),
    tuple(
      TERMS.continue,
      unitBytes(body),
      loobean(complete),
    ),
  );
}

export function httpClientCancelTask({
  id,
} = {}) {
  if (id == null) {
    throw new Error('id is required');
  }

  return tuple(
    TERMS.receive,
    BigInt(id),
    tuple(TERMS.cancel, 0n),
  );
}

export function runtimeOvum({ kind = 'i', wire, card }) {
  if (!wire) {
    throw new Error('wire is required');
  }
  return cell(cell(termAtom(kind), wire), card);
}

export function httpClientReceiveOvumJam({
  wire = httpClientWire(),
  id,
  status,
  headers = [],
  body = null,
  complete = true,
  kind = 'i',
} = {}) {
  return jamBytes(runtimeOvum({
    kind,
    wire,
    card: httpClientReceiveTask({ id, status, headers, body, complete }),
  }));
}

export function httpClientContinueOvumJam({
  wire = httpClientWire(),
  id,
  body = null,
  complete = true,
  kind = 'i',
} = {}) {
  return jamBytes(runtimeOvum({
    kind,
    wire,
    card: httpClientContinueTask({ id, body, complete }),
  }));
}

export function httpClientCancelOvumJam({
  wire = httpClientWire(),
  id,
  kind = 'i',
} = {}) {
  return jamBytes(runtimeOvum({
    kind,
    wire,
    card: httpClientCancelTask({ id }),
  }));
}

export function extractHttpClientEffects(input) {
  const effects = decodeEffectsInput(input);
  const out = {
    requests: [],
    cancels: [],
    unknown: [],
  };

  for (const effect of listItems(effects, 'effects')) {
    if (!isCell(effect)) {
      out.unknown.push({ effect, reason: 'effect is not a cell' });
      continue;
    }

    const wire = effect[0];
    const card = effect[1];
    if (!isHttpClientWire(wire)) {
      out.unknown.push({ effect, reason: 'not an http-client wire' });
      continue;
    }

    const request = decodeRequest(card, wire, effect);
    if (request) {
      out.requests.push(request);
      continue;
    }

    const cancel = decodeCancel(card, wire, effect);
    if (cancel) {
      out.cancels.push(cancel);
      continue;
    }

    out.unknown.push({ effect, reason: 'not an http-client card' });
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

function headerEntries(headers) {
  if (!headers) {
    return [];
  }
  if (typeof Headers === 'function' && headers instanceof Headers) {
    return [...headers].map(([key, value]) => [String(key), String(value)]);
  }
  if (Array.isArray(headers)) {
    return headers.map(([key, value]) => [String(key), String(value)]);
  }
  if (typeof headers[Symbol.iterator] === 'function') {
    return [...headers].map(([key, value]) => [String(key), String(value)]);
  }
  return Object.entries(headers).map(([key, value]) => [String(key), String(value)]);
}

function responseHeaders(response) {
  const headers = [];
  if (response.headers && typeof response.headers[Symbol.iterator] === 'function') {
    for (const [key, value] of response.headers) {
      headers.push([key, value]);
    }
  }
  return headers;
}

function requestKey(id) {
  return BigInt(id).toString();
}

async function bytesFromFetchBody(body) {
  if (body == null) {
    return null;
  }
  if (body instanceof Uint8Array) {
    return body;
  }
  if (ArrayBuffer.isView(body)) {
    return new Uint8Array(body.buffer, body.byteOffset, body.byteLength);
  }
  if (body instanceof ArrayBuffer) {
    return new Uint8Array(body);
  }
  if (typeof body === 'string') {
    return textEncoder.encode(body);
  }
  if (typeof Blob === 'function' && body instanceof Blob) {
    return new Uint8Array(await body.arrayBuffer());
  }
  throw new Error('http-client proxy cannot encode this request body type');
}

function base64FromBytes(bytes) {
  let binary = '';
  for (let i = 0; i < bytes.length; i += 0x8000) {
    binary += String.fromCharCode(...bytes.subarray(i, i + 0x8000));
  }
  if (typeof btoa === 'function') {
    return btoa(binary);
  }
  return Buffer.from(bytes).toString('base64');
}

function yieldToEventLoop() {
  return new Promise(resolve => setTimeout(resolve, 0));
}

export function createHttpClientProxyFetch({
  fetchFn = globalThis.fetch,
  proxyUrl = null,
  onLog = () => {},
} = {}) {
  if (!proxyUrl) {
    return fetchFn;
  }

  return async (resource, init = {}) => {
    const url = String(resource?.url ?? resource);
    const method = String(init.method ?? resource?.method ?? 'GET');
    const bodyBytes = await bytesFromFetchBody(init.body);
    const payload = {
      url,
      method,
      headers: headerEntries(init.headers ?? resource?.headers),
    };
    if (bodyBytes?.length) {
      payload.bodyBase64 = base64FromBytes(bodyBytes);
    }

    onLog({
      message: `http-client proxy ${method} ${url}`,
      className: 'tx',
    });

    return fetchFn(proxyUrl, {
      method: 'POST',
      headers: {
        'content-type': 'application/json',
      },
      body: JSON.stringify(payload),
      signal: init.signal,
    });
  };
}

export class BrowserHttpClientHost {
  constructor({
    fetchFn = globalThis.fetch,
    onLog = () => {},
    maxResponseBytes = 256 * 1024 * 1024,
    //  stream by default: buffered mode delivers the whole body as one
    //  %receive ovum, i.e. one synchronous wasm event — a multi-megabyte
    //  glob body wedges the runtime loop for its whole (possibly
    //  unbounded) processing time. chunked %continue events with yields
    //  between them keep the dojo alive.
    streamResponses = true,
  } = {}) {
    this.fetchFn = fetchFn;
    this.onLog = onLog;
    this.maxResponseBytes = maxResponseBytes;
    this.streamResponses = streamResponses;
    this.pending = new Map();
  }

  snapshot() {
    return {
      pending: this.pending.size,
    };
  }

  async routeEffects(input, {
    injectOvum,
  } = {}) {
    if (typeof injectOvum !== 'function') {
      throw new Error('injectOvum callback is required');
    }

    const effects = extractHttpClientEffects(input);
    for (const cancel of effects.cancels) {
      this.cancel(cancel.id);
    }
    for (const request of effects.requests) {
      this.#startRequest(request, injectOvum);
    }

    return {
      requests: effects.requests.length,
      cancels: effects.cancels.length,
      unknown: effects.unknown.length,
      pending: this.pending.size,
    };
  }

  cancel(id) {
    const pending = this.pending.get(requestKey(id));
    if (!pending) {
      return false;
    }
    pending.controller.abort();
    this.pending.delete(requestKey(id));
    return true;
  }

  abortAll() {
    for (const pending of this.pending.values()) {
      pending.controller.abort();
    }
    this.pending.clear();
  }

  async waitAll() {
    await Promise.allSettled([...this.pending.values()].map(pending => pending.done));
  }

  #startRequest(request, injectOvum) {
    if (typeof this.fetchFn !== 'function') {
      throw new Error('fetch is required for hosted http-client requests');
    }

    const key = requestKey(request.id);
    this.cancel(request.id);

    const controller = new AbortController();
    const done = this.#fetchAndInject(request, injectOvum, controller)
      .finally(() => {
        if (this.pending.get(key)?.controller === controller) {
          this.pending.delete(key);
        }
      });
    this.pending.set(key, { controller, done });
    this.onLog({
      message: `http-client request id=${key} ${request.method} ${request.url}`,
      className: 'tx',
    });
  }

  async #fetchAndInject(request, injectOvum, controller) {
    try {
      const response = await this.fetchFn(request.url, {
        method: request.method,
        headers: headersObject(request.headers),
        body: request.body ?? undefined,
        signal: controller.signal,
      });
      const result = this.streamResponses && response.body?.getReader
        ? await this.#streamAndInject(request, response, injectOvum)
        : await this.#bufferAndInject(request, response, injectOvum);

      if (!result?.canceled) {
        this.onLog({
          message: `http-client receive id=${requestKey(request.id)} status=${response.status}`,
          className: 'rx',
        });
      }
    }
    catch (error) {
      if (controller.signal.aborted) {
        this.onLog({
          message: `http-client canceled id=${requestKey(request.id)}`,
          className: 'err',
        });
        return;
      }
      await injectOvum({
        label: `http-receive-${requestKey(request.id)}`,
        ovumBytes: httpClientReceiveOvumJam({
          wire: request.wire,
          id: request.id,
          status: 504,
          headers: [],
          body: null,
        }),
      });
      this.onLog({
        message: `http-client failed id=${requestKey(request.id)}: ${error}`,
        className: 'err',
      });
    }
  }

  async #bufferAndInject(request, response, injectOvum) {
    const body = new Uint8Array(await response.arrayBuffer());
    if (body.length > this.maxResponseBytes) {
      throw new Error(`http-client response exceeds ${this.maxResponseBytes} bytes`);
    }
    await injectOvum({
      label: `http-receive-${requestKey(request.id)}`,
      ovumBytes: httpClientReceiveOvumJam({
        wire: request.wire,
        id: request.id,
        status: response.status,
        headers: responseHeaders(response),
        body: body.length ? body : null,
        complete: true,
      }),
    });
    return { canceled: false };
  }

  async #streamAndInject(request, response, injectOvum) {
    const key = requestKey(request.id);
    const reader = response.body.getReader();
    let total = 0;
    let started = false;

    try {
      while (true) {
        const { done, value } = await reader.read();
        if (done) {
          if (!started) {
            await injectOvum({
              label: `http-receive-${key}`,
              ovumBytes: httpClientReceiveOvumJam({
                wire: request.wire,
                id: request.id,
                status: response.status,
                headers: responseHeaders(response),
                body: null,
                complete: true,
              }),
            });
            await yieldToEventLoop();
          }
          else {
            await injectOvum({
              label: `http-continue-${key}`,
              ovumBytes: httpClientContinueOvumJam({
                wire: request.wire,
                id: request.id,
                body: null,
                complete: true,
              }),
            });
            await yieldToEventLoop();
          }
          return { canceled: false };
        }

        const chunk = value instanceof Uint8Array ? value : Uint8Array.from(value);
        total += chunk.length;
        if (total > this.maxResponseBytes) {
          throw new Error(`http-client response exceeds ${this.maxResponseBytes} bytes`);
        }

        if (!started) {
          started = true;
          await injectOvum({
            label: `http-receive-${key}`,
            ovumBytes: httpClientReceiveOvumJam({
              wire: request.wire,
              id: request.id,
              status: response.status,
              headers: responseHeaders(response),
              body: chunk.length ? chunk : null,
              complete: false,
            }),
          });
          await yieldToEventLoop();
        }
        else {
          await injectOvum({
            label: `http-continue-${key}`,
            ovumBytes: httpClientContinueOvumJam({
              wire: request.wire,
              id: request.id,
              body: chunk.length ? chunk : null,
              complete: false,
            }),
          });
          await yieldToEventLoop();
        }
      }
    }
    catch (error) {
      if (!started) {
        throw error;
      }
      await injectOvum({
        label: `http-cancel-${key}`,
        ovumBytes: httpClientCancelOvumJam({
          wire: request.wire,
          id: request.id,
        }),
      });
      this.onLog({
        message: `http-client stream failed id=${key}: ${error}`,
        className: 'err',
      });
      return { canceled: true };
    }
    finally {
      reader.releaseLock?.();
    }
  }
}
