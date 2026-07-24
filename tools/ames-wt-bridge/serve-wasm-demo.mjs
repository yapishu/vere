#!/usr/bin/env node
import { existsSync, createReadStream } from 'node:fs';
import { readFile, stat } from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
import { Readable } from 'node:stream';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));

const HOP_BY_HOP_HEADERS = new Set([
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

const BRIDGE_SERVICE_WORKER_PATH = '/apps/__vere_wasm_sw.js';
const BRIDGE_SERVICE_WORKER_SOURCE = 'vere-wasm-eyre-resource-worker.js';

const NOOP_SERVICE_WORKER = `
self.addEventListener('install', event => {
  self.skipWaiting();
});
self.addEventListener('activate', event => {
  event.waitUntil(self.clients.claim());
});
`;

const VIRTUAL_APP_SHELL = `
<!doctype html>
<meta charset="utf-8">
<title>Vere WASM app frame</title>
`;

function parseArgs(argv) {
  const out = {};
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (!arg.startsWith('--')) {
      continue;
    }
    const eq = arg.indexOf('=');
    if (eq !== -1) {
      out[arg.slice(2, eq)] = arg.slice(eq + 1);
      continue;
    }
    out[arg.slice(2)] = argv[++i];
  }
  return out;
}

function defaultRoot() {
  if (existsSync(path.join(scriptDir, 'index.html'))) {
    return scriptDir;
  }
  return path.resolve(scriptDir, '../../..');
}

function contentType(filePath) {
  switch (path.extname(filePath)) {
    case '.css': return 'text/css; charset=utf-8';
    case '.glob': return 'application/octet-stream';
    case '.html': return 'text/html; charset=utf-8';
    case '.js': return 'text/javascript; charset=utf-8';
    case '.json': return 'application/json; charset=utf-8';
    case '.map': return 'application/json; charset=utf-8';
    case '.mjs': return 'text/javascript; charset=utf-8';
    case '.pill': return 'application/octet-stream';
    case '.wasm': return 'application/wasm';
    default: return 'application/octet-stream';
  }
}

function staticHeaders(filePath, info) {
  const headers = {
    'content-type': contentType(filePath),
    'content-length': info.size,
  };
  switch (path.extname(filePath)) {
    case '.css':
    case '.html':
    case '.js':
    case '.mjs':
    case '.wasm':
      headers['cache-control'] = 'no-store';
      break;
    default:
      headers['cache-control'] = 'public, max-age=3600';
      break;
  }
  return headers;
}

function sendText(res, status, message) {
  res.writeHead(status, {
    'content-type': 'text/plain; charset=utf-8',
    'cache-control': 'no-store',
  });
  res.end(message);
}

function sendHtml(res, status, message) {
  const body = Buffer.from(message.trimStart(), 'utf8');
  res.writeHead(status, {
    'content-type': 'text/html; charset=utf-8',
    'content-length': body.length,
    'cache-control': 'no-store',
  });
  res.end(body);
}

function isVirtualAppServiceWorkerRequest(req, pathname) {
  return (
    String(req.headers['service-worker'] || '').toLowerCase() === 'script' &&
    pathname.startsWith('/apps/')
  );
}

function isVirtualAppDocumentRequest(req, pathname) {
  const fetchDest = String(req.headers['sec-fetch-dest'] || '').toLowerCase();
  const accept = String(req.headers.accept || '').toLowerCase();
  return (
    (pathname === '/apps' || pathname.startsWith('/apps/')) &&
    (fetchDest === 'document' || fetchDest === 'iframe' || accept.includes('text/html'))
  );
}

function serveNoopServiceWorker(req, res) {
  const body = Buffer.from(NOOP_SERVICE_WORKER.trimStart(), 'utf8');
  res.writeHead(200, {
    'content-type': 'text/javascript; charset=utf-8',
    'content-length': body.length,
    'cache-control': 'no-store',
    'service-worker-allowed': '/apps/',
  });
  if (req.method === 'HEAD') {
    res.end();
    return;
  }
  res.end(body);
}

async function readBridgeServiceWorker(root) {
  const candidates = [
    path.join(root, BRIDGE_SERVICE_WORKER_PATH),
    path.join(scriptDir, 'web', BRIDGE_SERVICE_WORKER_SOURCE),
    path.join(scriptDir, BRIDGE_SERVICE_WORKER_PATH),
  ];
  for (const candidate of candidates) {
    try {
      return await readFile(candidate, 'utf8');
    }
    catch (_) {
    }
  }
  throw new Error(`missing ${BRIDGE_SERVICE_WORKER_SOURCE}`);
}

async function serveBridgeServiceWorker(root, req, res) {
  const body = Buffer.from((await readBridgeServiceWorker(root)).trimStart(), 'utf8');
  res.writeHead(200, {
    'content-type': 'text/javascript; charset=utf-8',
    'content-length': body.length,
    'cache-control': 'no-store',
    'service-worker-allowed': '/apps/',
  });
  if (req.method === 'HEAD') {
    res.end();
    return;
  }
  res.end(body);
}

function safeResolve(root, pathname) {
  let decoded;
  try {
    decoded = decodeURIComponent(pathname);
  }
  catch (_) {
    return null;
  }
  if (decoded.includes('\0')) {
    return null;
  }

  let relative = path.posix.normalize(decoded);
  if (relative.startsWith('/')) {
    relative = relative.slice(1);
  }
  const filePath = path.resolve(root, relative || 'index.html');
  const rel = path.relative(root, filePath);
  if (rel.startsWith('..') || path.isAbsolute(rel)) {
    return null;
  }
  return filePath;
}

async function serveStatic(root, req, res, pathname) {
  const filePath = await findStaticFile(root, pathname);
  if (!filePath) {
    return false;
  }

  await serveFile(filePath, req, res);
  return true;
}

async function findStaticFile(root, pathname) {
  let filePath = safeResolve(root, pathname);
  if (!filePath) {
    return null;
  }

  let info = await stat(filePath).catch(() => null);
  if (info?.isDirectory()) {
    filePath = path.join(filePath, 'index.html');
    info = await stat(filePath).catch(() => null);
  }
  if (!info?.isFile()) {
    return null;
  }

  return filePath;
}

async function serveFile(filePath, req, res) {
  const info = await stat(filePath);
  res.writeHead(200, staticHeaders(filePath, info));
  if (req.method === 'HEAD') {
    res.end();
    return;
  }
  createReadStream(filePath).pipe(res);
}

async function serveDebugAsset(root, debugRoot, req, res, pathname) {
  const bundled = await findStaticFile(root, pathname);
  if (bundled) {
    await serveFile(bundled, req, res);
    return;
  }

  const marker = '/assets/debug/';
  const offset = pathname.indexOf(marker);
  const relative = offset === -1 ? '' : pathname.slice(offset + marker.length);
  if (!relative || relative.includes('..') || !/^[a-zA-Z0-9_.-]+$/.test(relative)) {
    sendText(res, 404, 'not found\n');
    return;
  }

  const filePath = await findStaticFile(debugRoot, `/${relative}`);
  if (!filePath) {
    sendText(res, 404, 'not found\n');
    return;
  }

  await serveFile(filePath, req, res);
}

async function readJsonBody(req, limitBytes = 64 * 1024 * 1024) {
  const chunks = [];
  let length = 0;
  for await (const chunk of req) {
    length += chunk.length;
    if (length > limitBytes) {
      throw new Error(`request body exceeds ${limitBytes} bytes`);
    }
    chunks.push(chunk);
  }
  return JSON.parse(Buffer.concat(chunks).toString('utf8'));
}

function proxyHeaders(entries) {
  const headers = new Headers();
  for (const entry of entries ?? []) {
    if (!Array.isArray(entry) || entry.length !== 2) {
      continue;
    }
    const key = String(entry[0]).toLowerCase();
    if (!key || HOP_BY_HOP_HEADERS.has(key) || key === 'host') {
      continue;
    }
    headers.append(key, String(entry[1]));
  }
  return headers;
}

function copyResponseHeaders(upstream, res) {
  for (const [key, value] of upstream.headers) {
    const lower = key.toLowerCase();
    if (HOP_BY_HOP_HEADERS.has(lower)) {
      continue;
    }
    res.setHeader(key, value);
  }
  res.setHeader('access-control-allow-origin', '*');
}

async function serveHttpClientProxy(req, res) {
  if (req.method === 'OPTIONS') {
    res.writeHead(204, {
      'access-control-allow-origin': '*',
      'access-control-allow-methods': 'POST, OPTIONS',
      'access-control-allow-headers': 'content-type',
    });
    res.end();
    return;
  }
  if (req.method !== 'POST') {
    sendText(res, 405, 'method not allowed\n');
    return;
  }

  let payload;
  try {
    payload = await readJsonBody(req);
  }
  catch (error) {
    sendText(res, 400, `bad proxy request: ${error}\n`);
    return;
  }

  let url;
  try {
    url = new URL(String(payload.url));
  }
  catch (_) {
    sendText(res, 400, 'bad proxy url\n');
    return;
  }
  if (url.protocol !== 'http:' && url.protocol !== 'https:') {
    sendText(res, 400, 'proxy url must be http or https\n');
    return;
  }

  const method = String(payload.method || 'GET').toUpperCase();
  const init = {
    method,
    headers: proxyHeaders(payload.headers),
    redirect: 'follow',
  };
  if (payload.bodyBase64 && method !== 'GET' && method !== 'HEAD') {
    init.body = Buffer.from(String(payload.bodyBase64), 'base64');
  }

  const controller = new AbortController();
  init.signal = controller.signal;
  res.on('close', () => {
    if (!res.writableEnded) {
      controller.abort();
    }
  });

  let upstream;
  try {
    upstream = await fetch(url, init);
  }
  catch (error) {
    if (!controller.signal.aborted) {
      sendText(res, 502, `proxy fetch failed: ${error}\n`);
    }
    return;
  }

  res.statusCode = upstream.status;
  res.statusMessage = upstream.statusText;
  copyResponseHeaders(upstream, res);

  if (!upstream.body) {
    res.end();
    return;
  }
  Readable.fromWeb(upstream.body).on('error', error => {
    if (!res.destroyed) {
      res.destroy(error);
    }
  }).pipe(res);
}

const args = parseArgs(process.argv.slice(2));
const root = path.resolve(args.root ?? process.env.ROOT ?? defaultRoot());
const urbitRepo = path.resolve(
  args.urbitRepo ?? process.env.URBIT_REPO ?? path.resolve(scriptDir, '../../../urbit'),
);
const debugRoot = path.join(urbitRepo, 'pkg/arvo/app/debug');
const host = args.host ?? process.env.BIND ?? process.env.HOST ?? '0.0.0.0';
const port = Number(args.port ?? process.env.PORT ?? 8093);

const server = http.createServer(async (req, res) => {
  try {
    const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
    if (url.pathname === '/_vere/http-client') {
      await serveHttpClientProxy(req, res);
      return;
    }
    if (req.method !== 'GET' && req.method !== 'HEAD') {
      sendText(res, 405, 'method not allowed\n');
      return;
    }
    if (url.pathname.includes('/assets/debug/')) {
      await serveDebugAsset(root, debugRoot, req, res, url.pathname);
      return;
    }
    if (url.pathname === BRIDGE_SERVICE_WORKER_PATH) {
      await serveBridgeServiceWorker(root, req, res);
      return;
    }
    if (isVirtualAppServiceWorkerRequest(req, url.pathname)) {
      serveNoopServiceWorker(req, res);
      return;
    }
    if (await serveStatic(root, req, res, url.pathname)) {
      return;
    }
    if (isVirtualAppDocumentRequest(req, url.pathname)) {
      sendHtml(res, 200, VIRTUAL_APP_SHELL);
      return;
    }
    sendText(res, 404, 'not found\n');
  }
  catch (error) {
    if (!res.headersSent) {
      sendText(res, 500, `server error: ${error}\n`);
    }
    else {
      res.destroy(error);
    }
  }
});

server.listen(port, host, () => {
  console.log(`serving ${root}`);
  console.log(`open http://${host === '0.0.0.0' ? 'localhost' : host}:${port}/`);
  console.log('http-client proxy: /_vere/http-client');
});
