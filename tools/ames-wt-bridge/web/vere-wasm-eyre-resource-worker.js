const requestTimeoutMs = 120000;
const hopByHopHeaders = new Set([
  'connection',
  'content-encoding',
  'content-length',
  'keep-alive',
  'proxy-authenticate',
  'proxy-authorization',
  'te',
  'trailer',
  'transfer-encoding',
  'upgrade',
]);

self.addEventListener('install', event => {
  event.waitUntil(self.skipWaiting());
});

self.addEventListener('activate', event => {
  event.waitUntil(self.clients.claim());
});

function responseBody(value) {
  if (value == null) {
    return null;
  }
  if (value instanceof Uint8Array) {
    return value;
  }
  if (value instanceof ArrayBuffer) {
    return new Uint8Array(value);
  }
  if (ArrayBuffer.isView(value)) {
    return new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  }
  if (Array.isArray(value)) {
    return Uint8Array.from(value);
  }
  return null;
}

function responseHeaders(entries) {
  const headers = new Headers();
  for (const entry of entries || []) {
    if (!Array.isArray(entry) || entry.length !== 2) {
      continue;
    }
    const key = String(entry[0]).toLowerCase();
    if (!key || hopByHopHeaders.has(key) || key === 'set-cookie' || key === 'set-cookie2') {
      continue;
    }
    try {
      headers.append(key, String(entry[1]));
    }
    catch (_) {
    }
  }
  return headers;
}

async function requestBody(request) {
  if (request.method === 'GET' || request.method === 'HEAD') {
    return null;
  }
  return new Uint8Array(await request.arrayBuffer());
}

async function fetchThroughClient(event) {
  const client = event.clientId ? await self.clients.get(event.clientId) : null;
  if (!client) {
    return fetch(event.request);
  }

  const body = await requestBody(event.request);
  const channel = new MessageChannel();
  const reply = new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      channel.port1.close();
      reject(new Error('Eyre service worker request timed out'));
    }, requestTimeoutMs);
    channel.port1.onmessage = message => {
      clearTimeout(timer);
      channel.port1.close();
      resolve(message.data || {});
    };
  });
  const transfer = [channel.port2];
  if (body) {
    transfer.push(body.buffer);
  }
  client.postMessage({
    source: 'vere-wasm-eyre-worker',
    type: 'fetch',
    method: event.request.method,
    url: event.request.url,
    headers: Array.from(event.request.headers.entries()),
    body,
  }, transfer);

  const result = await reply;
  if (result.error) {
    return new Response(result.error, {
      status: 502,
      headers: { 'content-type': 'text/plain; charset=utf-8' },
    });
  }

  const status = Number(result.status) || 200;
  return new Response(
    event.request.method === 'HEAD' || [204, 205, 304].includes(status)
      ? null
      : responseBody(result.body),
    {
      status,
      headers: responseHeaders(result.headers),
    },
  );
}

self.addEventListener('fetch', event => {
  const url = new URL(event.request.url);
  if (url.origin !== self.location.origin || !url.pathname.startsWith('/apps/')) {
    return;
  }
  if (url.pathname === '/apps/__vere_wasm_sw.js' || event.request.mode === 'navigate') {
    return;
  }
  event.respondWith(fetchThroughClient(event).catch(error => new Response(String(error), {
    status: 502,
    headers: { 'content-type': 'text/plain; charset=utf-8' },
  })));
});
