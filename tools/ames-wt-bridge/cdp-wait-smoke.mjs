#!/usr/bin/env node

const DEFAULT_TIMEOUT_MS = 600_000;
const DEFAULT_POLL_MS = 2_000;

function usage() {
  console.error(
    'usage: node --experimental-websocket cdp-wait-smoke.mjs ' +
      '[--port PORT] [--match TEXT] [--timeout-ms MS] [--poll-ms MS]',
  );
}

function parseArgs(argv) {
  const options = {
    port: 19222,
    match: 'wasm-vere-disk-route-smoke.html',
    timeoutMs: DEFAULT_TIMEOUT_MS,
    pollMs: DEFAULT_POLL_MS,
  };

  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    const next = () => {
      if (i + 1 >= argv.length) {
        throw new Error(`${arg} requires a value`);
      }
      return argv[++i];
    };

    if (arg === '--port') {
      options.port = Number(next());
    }
    else if (arg === '--match') {
      options.match = next();
    }
    else if (arg === '--timeout-ms') {
      options.timeoutMs = Number(next());
    }
    else if (arg === '--poll-ms') {
      options.pollMs = Number(next());
    }
    else if (arg === '-h' || arg === '--help') {
      usage();
      process.exit(0);
    }
    else {
      throw new Error(`unknown argument: ${arg}`);
    }
  }

  for (const [name, value] of [
    ['port', options.port],
    ['timeout-ms', options.timeoutMs],
    ['poll-ms', options.pollMs],
  ]) {
    if (!Number.isSafeInteger(value) || value <= 0) {
      throw new Error(`${name} must be a positive safe integer`);
    }
  }

  return options;
}

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function withTimeout(promise, ms, label) {
  let timer = null;
  try {
    return await Promise.race([
      promise,
      new Promise((_, reject) => {
        timer = setTimeout(() => reject(new Error(`${label} timed out`)), ms);
      }),
    ]);
  }
  finally {
    clearTimeout(timer);
  }
}

async function fetchJson(url) {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`fetch ${url} failed: ${response.status}`);
  }
  return response.json();
}

async function findPage({ port, match, deadline }) {
  const url = `http://127.0.0.1:${port}/json/list`;

  while (Date.now() < deadline) {
    try {
      const targets = await fetchJson(url);
      const page = targets.find(target => (
        target.type === 'page' && String(target.url).includes(match)
      ));
      if (page) {
        return page;
      }
    }
    catch (_) {}
    await sleep(500);
  }

  throw new Error(`timed out waiting for CDP page matching ${JSON.stringify(match)}`);
}

function connectDebugger(wsUrl) {
  if (typeof WebSocket !== 'function') {
    throw new Error('global WebSocket is unavailable; rerun node with --experimental-websocket');
  }

  return new Promise((resolve, reject) => {
    const ws = new WebSocket(wsUrl);
    const pending = new Map();
    let nextId = 1;

    function call(method, params = {}) {
      const id = nextId++;
      return new Promise((res, rej) => {
        pending.set(id, { res, rej });
        ws.send(JSON.stringify({ id, method, params }));
      });
    }

    ws.addEventListener('open', () => resolve({ ws, call }));
    ws.addEventListener('message', event => {
      const message = JSON.parse(event.data);
      if (!message.id) {
        return;
      }

      const callbacks = pending.get(message.id);
      if (!callbacks) {
        return;
      }
      pending.delete(message.id);

      if (message.error) {
        callbacks.rej(new Error(`${message.error.message}: ${message.error.data ?? ''}`));
      }
      else {
        callbacks.res(message.result ?? {});
      }
    });
    ws.addEventListener('error', reject);
    ws.addEventListener('close', () => {
      for (const callbacks of pending.values()) {
        callbacks.rej(new Error('CDP websocket closed'));
      }
      pending.clear();
    });
  });
}

async function evalValue(call, expression) {
  const result = await call('Runtime.evaluate', {
    expression,
    returnByValue: true,
    awaitPromise: true,
  });
  if (result.exceptionDetails) {
    throw new Error(result.exceptionDetails.text || 'Runtime.evaluate failed');
  }
  return result.result?.value;
}

function tailLines(text, count) {
  return String(text ?? '').trimEnd().split('\n').slice(-count).join('\n');
}

async function main() {
  const options = parseArgs(process.argv.slice(2));
  const deadline = Date.now() + options.timeoutMs;
  const page = await findPage({ ...options, deadline });
  const { ws, call } = await withTimeout(
    connectDebugger(page.webSocketDebuggerUrl),
    10_000,
    'CDP connect',
  );

  try {
    await call('Runtime.enable');

    let lastMarker = '';
    while (Date.now() < deadline) {
      const status = await withTimeout(
        evalValue(call, 'document.getElementById("out")?.dataset.status || ""'),
        240_000,
        'status evaluation',
      );
      const text = await withTimeout(
        evalValue(call, 'document.getElementById("out")?.textContent || ""'),
        240_000,
        'text evaluation',
      );
      const lines = String(text ?? '').trimEnd().split('\n');
      const marker = `${status}:${lines.length}:${lines[lines.length - 1] ?? ''}`;

      if (marker !== lastMarker) {
        console.log(`status=${status || '(none)'} lines=${lines.length}`);
        console.log(tailLines(text, 22));
        lastMarker = marker;
      }

      if (status && status !== 'running') {
        console.log(`FINAL_STATUS=${status}`);
        process.exitCode = status === 'ok' ? 0 : 1;
        return;
      }

      await sleep(options.pollMs);
    }

    throw new Error('timed out waiting for smoke page to finish');
  }
  finally {
    ws.close();
  }
}

main().catch(error => {
  console.error(error?.stack ?? String(error));
  process.exitCode = 1;
});
