import assert from 'node:assert/strict';
import test from 'node:test';

import {
  BrowserHttpClientHost,
  createHttpClientProxyFetch,
  extractHttpClientEffects,
  httpClientBornOvumJam,
  httpClientReceiveOvumJam,
  httpClientWire,
  httpRequestNoun,
} from './ames-wasm-http-client.mjs';
import {
  atomFromBytesLE,
  cue,
  jamBytes,
  list,
  termAtom,
  tuple,
  cell,
} from './urbit-noun.mjs';

const decoder = new TextDecoder();

function textFromAtom(atom) {
  return decoder.decode(bytesFromAtom(atom));
}

function bytesFromAtom(atom) {
  let value = BigInt(atom);
  const out = [];
  while (value > 0n) {
    out.push(Number(value & 0xffn));
    value >>= 8n;
  }
  return Uint8Array.from(out);
}

function tupleItems(noun, count) {
  const out = [];
  let cur = noun;
  for (let i = 0; i < count - 1; i++) {
    assert.ok(Array.isArray(cur));
    out.push(cur[0]);
    cur = cur[1];
  }
  out.push(cur);
  return out;
}

function requestEffect({
  id = 42n,
  method = 'GET',
  url = 'https://example.invalid/data',
  headers = [['accept', 'text/plain']],
  body = null,
} = {}) {
  return cell(
    httpClientWire('0v1n.2m9vh'),
    tuple(
      termAtom('request'),
      id,
      httpRequestNoun({ method, url, headers, body }),
    ),
  );
}

test('extractHttpClientEffects decodes request and cancel cards', () => {
  const body = Uint8Array.from([1, 2, 3]);
  const request = requestEffect({
    method: 'POST',
    headers: [['content-type', 'application/octet-stream']],
    body,
  });
  const cancel = cell(
    httpClientWire('0v1n.2m9vh'),
    tuple(termAtom('cancel-request'), 42n),
  );

  const out = extractHttpClientEffects(jamBytes(list(request, cancel)));

  assert.equal(out.requests.length, 1);
  assert.equal(out.cancels.length, 1);
  assert.equal(out.unknown.length, 0);
  assert.equal(out.requests[0].id, 42n);
  assert.equal(out.requests[0].method, 'POST');
  assert.equal(out.requests[0].url, 'https://example.invalid/data');
  assert.deepEqual(out.requests[0].headers, [
    ['content-type', 'application/octet-stream'],
  ]);
  assert.deepEqual([...out.requests[0].body], [...body]);
  assert.equal(out.cancels[0].id, 42n);
});

test('httpClientReceiveOvumJam builds the %iris receive ovum shape', () => {
  const bytes = httpClientReceiveOvumJam({
    wire: httpClientWire('0v1n.2m9vh'),
    id: 7n,
    status: 201,
    headers: [['content-type', 'text/plain']],
    body: new TextEncoder().encode('ok'),
  });
  const ovum = cue(atomFromBytesLE(bytes));
  const [wire, card] = ovum;
  const [kind, innerWire] = wire;
  const [tag, id, event] = tupleItems(card, 3);
  const [eventTag, responseHeader, bodyUnit, complete] = tupleItems(event, 4);
  const [status, headers] = tupleItems(responseHeader, 2);

  assert.equal(kind, termAtom('i'));
  assert.deepEqual(innerWire, httpClientWire('0v1n.2m9vh'));
  assert.equal(tag, termAtom('receive'));
  assert.equal(id, 7n);
  assert.equal(eventTag, termAtom('start'));
  assert.equal(status, 201n);
  assert.equal(complete, 0n);
  assert.equal(headers[0][0], termAtom('content-type'));
  assert.equal(headers[0][1], termAtom('text/plain'));
  assert.equal(bodyUnit[0], 0n);
  assert.equal(textFromAtom(bodyUnit[1][1]), 'ok');
});

test('httpClientBornOvumJam builds the hosted %http-client born ovum', () => {
  const ovum = cue(atomFromBytesLE(httpClientBornOvumJam()));
  const [wire, card] = ovum;
  const [kind, innerWire] = wire;
  const [driver, service, tail] = tupleItems(innerWire, 3);
  const [tag, data] = tupleItems(card, 2);

  assert.equal(kind, termAtom('i'));
  assert.equal(driver, termAtom('http-client'));
  assert.equal(textFromAtom(service), '0v1n.2m9vh');
  assert.equal(tail, 0n);
  assert.equal(tag, termAtom('born'));
  assert.equal(data, 0n);
});

test('createHttpClientProxyFetch posts original requests to the host proxy', async () => {
  const calls = [];
  const proxied = createHttpClientProxyFetch({
    proxyUrl: 'https://demo.invalid/_vere/http-client',
    fetchFn: async (url, init) => {
      calls.push({ url, init });
      return {
        status: 204,
        headers: new Map(),
        async arrayBuffer() {
          return new ArrayBuffer(0);
        },
      };
    },
  });

  const response = await proxied('https://example.invalid/upload', {
    method: 'POST',
    headers: {
      accept: 'application/octet-stream',
      'content-type': 'application/octet-stream',
    },
    body: Uint8Array.from([1, 2, 3]),
  });

  assert.equal(response.status, 204);
  assert.equal(calls.length, 1);
  assert.equal(calls[0].url, 'https://demo.invalid/_vere/http-client');
  assert.equal(calls[0].init.method, 'POST');
  assert.deepEqual(JSON.parse(calls[0].init.body), {
    url: 'https://example.invalid/upload',
    method: 'POST',
    headers: [
      ['accept', 'application/octet-stream'],
      ['content-type', 'application/octet-stream'],
    ],
    bodyBase64: 'AQID',
  });
});

test('BrowserHttpClientHost fetches requests and injects receive ova', async () => {
  const injected = [];
  const logs = [];
  const host = new BrowserHttpClientHost({
    onLog: event => logs.push(event.message),
    fetchFn: async (url, init) => {
      assert.equal(url, 'https://example.invalid/data');
      assert.equal(init.method, 'GET');
      assert.equal(init.headers.accept, 'text/plain');
      return {
        status: 202,
        headers: new Map([['content-type', 'text/plain']]),
        async arrayBuffer() {
          return new TextEncoder().encode('done').buffer;
        },
      };
    },
  });

  const routed = await host.routeEffects(jamBytes(list(requestEffect())), {
    injectOvum: async ovum => injected.push(ovum),
  });
  await host.waitAll();

  assert.deepEqual(routed, {
    requests: 1,
    cancels: 0,
    unknown: 0,
    pending: 1,
  });
  assert.equal(host.snapshot().pending, 0);
  assert.equal(injected.length, 1);
  assert.equal(injected[0].label, 'http-receive-42');
  assert.ok(logs.some(line => line.includes('http-client request id=42')));
  assert.ok(logs.some(line => line.includes('http-client receive id=42 status=202')));
});

test('BrowserHttpClientHost injects a 504 response when fetch fails', async () => {
  const injected = [];
  const host = new BrowserHttpClientHost({
    fetchFn: async () => {
      throw new Error('network down');
    },
  });

  await host.routeEffects(jamBytes(list(requestEffect())), {
    injectOvum: async ovum => injected.push(ovum),
  });
  await host.waitAll();

  const ovum = cue(atomFromBytesLE(injected[0].ovumBytes));
  const [, card] = ovum;
  const [, , event] = tupleItems(card, 3);
  const [, responseHeader] = tupleItems(event, 4);
  const [status] = tupleItems(responseHeader, 2);

  assert.equal(status, 504n);
});

test('BrowserHttpClientHost streams responses by default like native cttp', async () => {
  //  native cttp forwards %continue chunks as they arrive from the wire;
  //  buffered whole-body delivery makes one giant synchronous wasm event
  //  (a multi-megabyte glob wedges the runtime loop), so streaming is the
  //  default and buffering is opt-in via streamResponses: false.
  const injected = [];
  const host = new BrowserHttpClientHost({
    fetchFn: async () => ({
      status: 200,
      headers: new Map([['content-type', 'text/plain']]),
      body: new ReadableStream({
        start(controller) {
          controller.enqueue(new TextEncoder().encode('hel'));
          controller.enqueue(new TextEncoder().encode('lo'));
          controller.close();
        },
      }),
      async arrayBuffer() {
        return new TextEncoder().encode('hello').buffer;
      },
    }),
  });

  await host.routeEffects(jamBytes(list(requestEffect())), {
    injectOvum: async ovum => injected.push(ovum),
  });
  await host.waitAll();

  assert.equal(injected.length, 3);
  const start = cue(atomFromBytesLE(injected[0].ovumBytes));
  const [, startCard] = start;
  const [, , startEvent] = tupleItems(startCard, 3);
  const [startTag, responseHeader, startBody, startComplete] = tupleItems(startEvent, 4);
  const [status] = tupleItems(responseHeader, 2);
  assert.equal(startTag, termAtom('start'));
  assert.equal(status, 200n);
  assert.equal(textFromAtom(startBody[1][1]), 'hel');
  assert.equal(startComplete, 1n);
});

test('BrowserHttpClientHost streams response chunks when enabled', async () => {
  const injected = [];
  const host = new BrowserHttpClientHost({
    streamResponses: true,
    fetchFn: async () => ({
      status: 200,
      headers: new Map([['content-type', 'text/plain']]),
      body: new ReadableStream({
        start(controller) {
          controller.enqueue(new TextEncoder().encode('hel'));
          controller.enqueue(new TextEncoder().encode('lo'));
          controller.close();
        },
      }),
    }),
  });

  await host.routeEffects(jamBytes(list(requestEffect())), {
    injectOvum: async ovum => injected.push(ovum),
  });
  await host.waitAll();

  assert.equal(injected.length, 3);

  const start = cue(atomFromBytesLE(injected[0].ovumBytes));
  const [, startCard] = start;
  const [, , startEvent] = tupleItems(startCard, 3);
  const [startTag, responseHeader, startBody, startComplete] = tupleItems(startEvent, 4);
  const [status] = tupleItems(responseHeader, 2);
  assert.equal(startTag, termAtom('start'));
  assert.equal(status, 200n);
  assert.equal(textFromAtom(startBody[1][1]), 'hel');
  assert.equal(startComplete, 1n);

  const cont = cue(atomFromBytesLE(injected[1].ovumBytes));
  const [, contCard] = cont;
  const [, , contEvent] = tupleItems(contCard, 3);
  const [contTag, contBody, contComplete] = tupleItems(contEvent, 3);
  assert.equal(contTag, termAtom('continue'));
  assert.equal(textFromAtom(contBody[1][1]), 'lo');
  assert.equal(contComplete, 1n);

  const done = cue(atomFromBytesLE(injected[2].ovumBytes));
  const [, doneCard] = done;
  const [, , doneEvent] = tupleItems(doneCard, 3);
  const [doneTag, doneBody, doneComplete] = tupleItems(doneEvent, 3);
  assert.equal(doneTag, termAtom('continue'));
  assert.equal(doneBody, 0n);
  assert.equal(doneComplete, 0n);
});

test('BrowserHttpClientHost aborts pending fetches on cancel-request', async () => {
  const injected = [];
  let abortSignal = null;
  const host = new BrowserHttpClientHost({
    fetchFn: async (_url, init) => {
      abortSignal = init.signal;
      return new Promise((resolve, reject) => {
        init.signal.addEventListener('abort', () => {
          reject(new DOMException('aborted', 'AbortError'));
        });
      });
    },
  });
  const cancel = cell(
    httpClientWire('0v1n.2m9vh'),
    tuple(termAtom('cancel-request'), 42n),
  );

  await host.routeEffects(jamBytes(list(requestEffect())), {
    injectOvum: async ovum => injected.push(ovum),
  });
  assert.equal(host.snapshot().pending, 1);
  await host.routeEffects(jamBytes(list(cancel)), {
    injectOvum: async ovum => injected.push(ovum),
  });
  await host.waitAll();

  assert.equal(abortSignal.aborted, true);
  assert.equal(host.snapshot().pending, 0);
  assert.equal(injected.length, 0);
});

test('BrowserHttpClientHost emits %cancel if a stream fails after %start', async () => {
  const injected = [];
  const host = new BrowserHttpClientHost({
    streamResponses: true,
    fetchFn: async () => ({
      status: 200,
      headers: new Map(),
      body: new ReadableStream({
        start(controller) {
          controller.enqueue(new TextEncoder().encode('ok'));
          controller.enqueue(new TextEncoder().encode('too-long'));
          controller.close();
        },
      }),
    }),
    maxResponseBytes: 3,
  });

  await host.routeEffects(jamBytes(list(requestEffect())), {
    injectOvum: async ovum => injected.push(ovum),
  });
  await host.waitAll();

  assert.equal(injected.length, 2);
  const startOvum = cue(atomFromBytesLE(injected[0].ovumBytes));
  const [, startCard] = startOvum;
  const [, , startEvent] = tupleItems(startCard, 3);
  const [startTag, responseHeader] = tupleItems(startEvent, 4);
  const [status] = tupleItems(responseHeader, 2);
  assert.equal(startTag, termAtom('start'));
  assert.equal(status, 200n);

  const ovum = cue(atomFromBytesLE(injected[1].ovumBytes));
  const [, card] = ovum;
  const [, , event] = tupleItems(card, 3);
  const [tag, tail] = tupleItems(event, 2);
  assert.equal(tag, termAtom('cancel'));
  assert.equal(tail, 0n);
});
