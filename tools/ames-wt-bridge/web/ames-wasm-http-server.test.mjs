import assert from 'node:assert/strict';
import test from 'node:test';

import {
  BrowserHttpServerHost,
  extractHttpServerEffects,
  httpServerBornOvumJam,
  httpServerLiveOvumJam,
  httpServerRequestOvumJam,
  httpServerWire,
} from './ames-wasm-http-server.mjs';
import {
  atomFromBytesLE,
  jamBytes,
  list,
  termAtom,
  tuple,
} from './urbit-noun.mjs';

const encoder = new TextEncoder();

function textAtom(text) {
  return atomFromBytesLE(encoder.encode(String(text)));
}

function unitBytes(text) {
  const bytes = encoder.encode(text);
  return [0n, [BigInt(bytes.length), atomFromBytesLE(bytes)]];
}

function headersList(headers = []) {
  return list(...headers.map(([key, value]) => [textAtom(key), textAtom(value)]));
}

function responseEffects({
  service = '0v1n.2m9vh',
  connectionId = 1n,
  requestId = 1n,
  status = 200n,
  headers = [['content-type', 'text/plain']],
  body = 'ok',
  complete = true,
} = {}) {
  return jamBytes(list(
    tuple(
      httpServerWire({ service, connectionId, requestId }),
      tuple(
        termAtom('response'),
        tuple(
          termAtom('start'),
          tuple(status, headersList(headers)),
          unitBytes(body),
          complete ? 0n : 1n,
        ),
      ),
    ),
  ));
}

test('http-server born/live/request ova build cueable runtime events', () => {
  assert.ok(httpServerBornOvumJam().length > 0);
  assert.ok(httpServerLiveOvumJam({ insecurePort: 8080 }).length > 0);
  assert.ok(httpServerRequestOvumJam({
    connectionId: 1n,
    requestId: 1n,
    method: 'GET',
    url: '/~/name',
  }).length > 0);
});

test('extractHttpServerEffects decodes %response start cards', () => {
  const effects = extractHttpServerEffects(responseEffects({
    connectionId: 7n,
    requestId: 8n,
    status: 204n,
    headers: [['x-test', 'yes']],
    body: '',
  }));

  assert.equal(effects.responses.length, 1);
  assert.equal(effects.responses[0].wire.service, '0v1n.2m9vh');
  assert.equal(effects.responses[0].wire.connectionId, 7n);
  assert.equal(effects.responses[0].wire.requestId, 8n);
  assert.equal(effects.responses[0].response.status, 204);
  assert.deepEqual(effects.responses[0].response.headers, [['x-test', 'yes']]);
});

test('BrowserHttpServerHost resolves pending requests from response effects', async () => {
  const host = new BrowserHttpServerHost({ requestTimeoutMs: null });
  const injected = [];
  const promise = host.handleRequest({
    method: 'GET',
    url: '/~/name',
  }, {
    injectOvum: async item => {
      injected.push(item);
    },
  });

  assert.equal(injected.length, 1);
  assert.equal(host.snapshot().pending, 1);

  const routed = host.routeEffects(responseEffects());
  const response = await promise;

  assert.equal(routed.responses, 1);
  assert.equal(response.status, 200);
  assert.deepEqual([...response.body], [...encoder.encode('ok')]);
  assert.equal(host.snapshot().pending, 0);
});

test('BrowserHttpServerHost aborts pending requests and injects cancel ovum', async () => {
  const host = new BrowserHttpServerHost({ requestTimeoutMs: null });
  const controller = new AbortController();
  const injected = [];
  const promise = host.handleRequest({
    method: 'GET',
    url: '/slow',
    signal: controller.signal,
  }, {
    injectOvum: async item => {
      injected.push(item);
    },
  });

  assert.equal(host.snapshot().pending, 1);
  controller.abort();

  await assert.rejects(promise, /http-server request aborted/);
  assert.equal(host.snapshot().pending, 0);
  assert.equal(injected.length, 2);
  assert.match(injected[1].label, /^http-server-cancel-/);
});

test('BrowserHttpServerHost records config/session/cache effects', () => {
  const host = new BrowserHttpServerHost();
  const effects = jamBytes(list(
    tuple(httpServerWire(), tuple(termAtom('set-config'), 0n)),
    tuple(httpServerWire(), tuple(termAtom('sessions'), list(textAtom('sid')))),
    tuple(httpServerWire(), tuple(termAtom('grow'), list(textAtom('cache')))),
  ));

  const routed = host.routeEffects(effects);

  assert.equal(routed.configs, 1);
  assert.equal(routed.sessions, 1);
  assert.equal(routed.grows, 1);
  assert.equal(host.snapshot().configured, true);
  assert.equal(host.snapshot().sessionCount, 1);
  assert.equal(host.snapshot().cacheGrows, 1);
});
