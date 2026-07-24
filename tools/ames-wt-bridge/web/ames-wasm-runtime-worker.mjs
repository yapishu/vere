import {
  AmesWasmRuntimeService,
  publicRuntimeSnapshot,
} from './ames-wasm-runtime-service.mjs';
import {
  BrowserHttpClientHost,
  createHttpClientProxyFetch,
} from './ames-wasm-http-client.mjs';
import {
  IndexedDBVereWasmFileStore,
} from './vere-wasm-host.mjs';
import { patp } from './ames-ship.mjs';

function asBigInt(value, name) {
  if (value == null || value === '') {
    throw new Error(`${name} is required`);
  }
  return BigInt(value);
}

function asBytes(value, name) {
  if (value == null) {
    return null;
  }
  if (value instanceof Uint8Array) {
    return Uint8Array.from(value);
  }
  if (Array.isArray(value)) {
    return Uint8Array.from(value);
  }
  throw new Error(`${name} must be bytes`);
}

function normalizeMemory(input = {}) {
  return {
    loomExponent: input.loomExponent ?? 29,
    overheadBytes: (input.overheadMiB ?? 512) * 1024 * 1024,
    maximumBytes: (input.maximumMiB ?? 1536) * 1024 * 1024,
  };
}

function publicResult(value) {
  if (Array.isArray(value)) {
    return value.map(publicResult);
  }
  if (value instanceof Uint8Array) {
    return Uint8Array.from(value);
  }
  if (typeof value === 'bigint') {
    return value.toString();
  }
  if (value && typeof value === 'object') {
    const out = {};
    for (const [key, inner] of Object.entries(value)) {
      out[key] = publicResult(inner);
    }
    return out;
  }
  return value;
}

function collectTransfers(value, out = [], seen = new Set()) {
  if (value instanceof Uint8Array) {
    if (!seen.has(value.buffer)) {
      seen.add(value.buffer);
      out.push(value.buffer);
    }
    return out;
  }
  if (Array.isArray(value)) {
    for (const item of value) {
      collectTransfers(item, out, seen);
    }
    return out;
  }
  if (value && typeof value === 'object') {
    for (const item of Object.values(value)) {
      collectTransfers(item, out, seen);
    }
  }
  return out;
}

function isRoutineWasmLog(line) {
  return (
    /^disk-wasm: wrote effects \/out\/runtime-\d+-(?:terminal-data-\d+|behn-wake|http-(?:receive|continue|cancel)-\d+)\.effects\.jam /.test(line) ||
    /^disk-wasm: host ovum \/in\/runtime-\d+-(?:terminal-data-\d+|behn-wake|http-(?:receive|continue|cancel)-\d+)\.ovum\.jam committed /.test(line)
  );
}

async function fetchBytes(fetchFn, url) {
  let response;
  try {
    response = await fetchFn(url);
  }
  catch (error) {
    throw new Error(
      `fetch ${url} failed before a response; remote pill URLs must permit ` +
      `browser CORS, otherwise use a same-origin URL such as ` +
      `./assets/brass-4.6.pill: ${error}`,
    );
  }
  if (!response.ok) {
    throw new Error(`fetch ${url} failed: ${response.status}`);
  }
  return new Uint8Array(await response.arrayBuffer());
}

function dawnPointPath(ship) {
  return `/boot/point-${BigInt(ship).toString(16).padStart(32, '0')}.json`;
}

export function snapshotHasPier(snapshot) {
  return Boolean(
    snapshot?.files?.has?.('/pier/.urb/log/meta.bin') ||
    [...(snapshot?.files?.keys?.() ?? [])].some(path => (
      String(path).startsWith('/pier/.urb/log/') &&
      String(path).endsWith('/events.bin')
    )),
  );
}

async function fetchDawnResponse(fetchFn, url, payload) {
  const response = await fetchFn(url, {
    method: 'POST',
    headers: {
      accept: 'application/json',
      'content-type': 'application/json',
    },
    body: JSON.stringify(payload),
  });
  if (!response.ok) {
    throw new Error(`dawn request failed: HTTP ${response.status}`);
  }
  return new Uint8Array(await response.arrayBuffer());
}

export async function prepareOwnedBoot({
  fetchFn,
  proxyUrl,
  ship,
  keyBytes,
  rollerUrl = 'https://roller.urbit.org/v1/azimuth',
  emit = () => {},
}) {
  const ownedShip = BigInt(ship);
  const key = asBytes(keyBytes, 'keyfile');
  if (!key?.length) {
    throw new Error('owned boot requires a non-empty keyfile');
  }
  const fetchPublic = createHttpClientProxyFetch({
    fetchFn,
    proxyUrl,
    onLog: event => emit({ type: 'log', ...event }),
  });
  const files = {
    '/boot/ship.key': key,
  };

  let current = ownedShip;
  const seen = new Set();
  for (;;) {
    const id = current.toString();
    if (seen.has(id)) {
      throw new Error(`dawn sponsor cycle at ${patp(current)}`);
    }
    seen.add(id);
    const bytes = await fetchDawnResponse(fetchPublic, rollerUrl, {
      jsonrpc: '2.0',
      id: `point-${id}`,
      method: 'getPoint',
      params: { ship: patp(current) },
    });
    files[dawnPointPath(current)] = bytes;
    const json = JSON.parse(new TextDecoder().decode(bytes));
    if (current < 256n) {
      break;
    }
    const sponsor = json?.result?.network?.sponsor;
    if (!sponsor?.has) {
      throw new Error(`Roller returned no sponsor for ${patp(current)}`);
    }
    current = BigInt(sponsor.who);
  }

  // Roller expects the same batch shape Vere's +czar:give:dawn emits.
  const galaxyRequests = Array.from({ length: 256 }, (_, galaxy) => ({
    jsonrpc: '2.0',
    id: `gal-${galaxy}`,
    method: 'getPoint',
    params: { ship: patp(BigInt(galaxy)) },
  }));
  files['/boot/galaxies.json'] = await fetchDawnResponse(
    fetchPublic,
    rollerUrl,
    galaxyRequests,
  );
  files['/boot/turf.json'] = await fetchDawnResponse(fetchPublic, rollerUrl, {
    jsonrpc: '2.0',
    id: 'turf',
    method: 'getDns',
    params: {},
  });
  emit({
    type: 'log',
    message: `prepared public dawn state for ${patp(ownedShip)}; keyfile stayed local`,
    className: 'rx',
  });
  return files;
}

class WasmLogBuffer {
  constructor(emit, className) {
    this.emit = emit;
    this.className = className;
    this.decoder = new TextDecoder();
    this.pending = '';
  }

  write(bytes) {
    this.pending += this.decoder.decode(bytes, { stream: true }).replace(/\r/g, '');
    this.#drainCompleteLines();
  }

  flush() {
    this.pending += this.decoder.decode().replace(/\r/g, '');
    this.#emitLine(this.pending);
    this.pending = '';
  }

  #drainCompleteLines() {
    let newline = this.pending.indexOf('\n');
    while (-1 !== newline) {
      this.#emitLine(this.pending.slice(0, newline));
      this.pending = this.pending.slice(newline + 1);
      newline = this.pending.indexOf('\n');
    }
  }

  #emitLine(line) {
    if (line && !isRoutineWasmLog(line)) {
      this.emit({
        type: 'log',
        message: `wasm: ${line}`,
        className: this.className,
      });
    }
  }
}

export function createAmesRuntimeWorkerHandler({
  emit = () => {},
  Service = AmesWasmRuntimeService,
  FileStore = IndexedDBVereWasmFileStore,
  fetchFn = globalThis.fetch,
} = {}) {
  let service = null;
  let loopCompletionPromise = null;
  const wasmStdout = new WasmLogBuffer(emit, '');
  const wasmStderr = new WasmLogBuffer(emit, 'err');

  function flushWasmLogs() {
    wasmStdout.flush();
    wasmStderr.flush();
  }

  const requireService = () => {
    if (!service) {
      throw new Error('runtime service is not started');
    }
    return service;
  };

  async function start(message) {
    if (service) {
      await service.shutdown();
      flushWasmLogs();
      service = null;
    }
    loopCompletionPromise = null;

    if (typeof fetchFn !== 'function') {
      throw new Error('fetch is required');
    }
    const wasmUrl = new URL(message.wasmUrl);
    const pillUrl = new URL(message.pillUrl);
    const fileStore = new FileStore({
      scope: String(message.scope || 'vere-runtime-worker'),
    });
    if (message.clearStore === true) {
      await fileStore.clear();
    }
    const persisted = typeof fileStore.load === 'function'
      ? await fileStore.load()
      : null;
    const resumingPier = snapshotHasPier(persisted);
    const pillBytes = await fetchBytes(fetchFn, pillUrl);
    const bootMode = String(message.bootMode || 'fake');
    const bootFiles = bootMode === 'owned' && !resumingPier
      ? await prepareOwnedBoot({
        fetchFn,
        proxyUrl: message.httpClientProxyUrl,
        ship: asBigInt(message.fakeShip, 'ship'),
        keyBytes: message.keyBytes,
        rollerUrl: message.rollerUrl,
        emit,
      })
      : {};
    if (bootMode === 'owned' && resumingPier) {
      emit({
        type: 'log',
        message: 'resuming owned pier from IndexedDB; keyfile is not required',
        className: 'rx',
      });
    }
    emit({ type: 'log', message: `pill-bytes=${pillBytes.length}` });
    emit({
      type: 'log',
      message: message.httpClientProxyUrl
        ? `http-client proxy enabled ${message.httpClientProxyUrl}`
        : 'http-client proxy disabled',
      className: message.httpClientProxyUrl ? 'rx' : 'err',
    });

    service = new Service({
      wasmUrl,
      pillBytes,
      fileStore,
      bridgeUrl: message.bridgeUrl,
      certificateHash: message.certificateHash || '',
      memoryOptions: normalizeMemory(message),
      fakeShip: asBigInt(message.fakeShip ?? '0x100', 'fakeShip'),
      bootMode,
      bootFiles,
      sessionId: asBigInt(message.sessionId ?? '1', 'sessionId'),
      autoStartHttpClient: message.autoStartHttpClient !== false,
      httpClientHostFactory: input => new BrowserHttpClientHost({
        ...input,
        fetchFn: createHttpClientProxyFetch({
          fetchFn,
          proxyUrl: message.httpClientProxyUrl,
          onLog: event => emit({ type: 'log', ...event }),
        }),
        streamResponses: message.streamHttpClientResponses !== false,
      }),
      onLog: event => emit({ type: 'log', ...event }),
      onStdout: bytes => wasmStdout.write(bytes),
      onStderr: bytes => wasmStderr.write(bytes),
      onPacket: event => emit({
        type: 'packet',
        mode: event.mode,
        bytes: [...event.packet],
      }),
      onHttpServer: event => {
        if (event.response?.type === 'start') {
          return;
        }
        const message = {
          type: 'http-stream',
          streamId: event.streamId,
          key: event.key,
          wire: publicResult(event.wire),
          response: publicResult(event.response),
        };
        emit(message, collectTransfers(message));
      },
      onTerminal: event => emit({
        type: 'terminal',
        event: publicResult(event),
      }),
    });

    return service.start();
  }

  function loopOptions(message = {}) {
    return {
      maxRounds: message.maxRounds ?? Infinity,
      firstIdleTimeoutMs: message.firstIdleTimeoutMs ?? null,
      idleTimeoutMs: message.idleTimeoutMs ?? null,
    };
  }

  function attachLoopCompletion(currentService) {
    const promise = currentService.waitInputLoop()
      .then(result => {
        const message = {
          type: 'loop',
          status: 'complete',
          result: publicResult(result),
        };
        emit(message, collectTransfers(message));
        return result;
      })
      .catch(error => {
        emit({
          type: 'loop',
          status: 'error',
          error: String(error?.stack ?? error),
        });
        return null;
      })
      .finally(() => {
        if (loopCompletionPromise === promise) {
          loopCompletionPromise = null;
        }
      });
    loopCompletionPromise = promise;
  }

  async function handle(message = {}) {
    const id = message.id ?? null;
    try {
      let result;
      switch (message.type) {
        case 'start':
          result = await start(message);
          break;
        case 'connect':
          result = await requireService().connect({
            bridgeUrl: message.bridgeUrl,
            certificateHash: message.certificateHash,
          });
          break;
        case 'mate':
          result = await requireService().mate({
            ship: asBigInt(message.ship, 'ship'),
            dry: Boolean(message.dry),
          });
          break;
        case 'keen':
          result = await requireService().keen({
            ship: asBigInt(message.ship, 'ship'),
            path: message.path,
            security: BigInt(message.security ?? 0),
          });
          break;
        case 'inject-packet':
          result = await requireService().injectPacket({
            packet: asBytes(message.packet, 'packet'),
            lane: message.lane == null ? undefined : BigInt(message.lane),
          });
          break;
        case 'http-request':
          result = await requireService().httpRequest({
            method: message.method ?? 'GET',
            url: message.url ?? '/',
            headers: Array.isArray(message.headers) ? message.headers : [],
            body: asBytes(message.body, 'body'),
            secure: Boolean(message.secure),
            local: message.local === true,
            timeoutMs: message.timeoutMs,
            stream: message.stream === true,
            streamId: message.streamId ?? null,
          });
          break;
        case 'http-request-cancel':
          result = await requireService().httpRequestCancel({
            service: message.service,
            connectionId: message.connectionId,
            requestId: message.requestId,
          });
          break;
        case 'http-client-start':
          result = await requireService().httpClientStart();
          break;
        case 'terminal-start':
          result = await requireService().terminalStart({
            cols: message.cols ?? 80,
            rows: message.rows ?? 24,
          });
          break;
        case 'terminal-resize':
          result = await requireService().terminalResize({
            cols: message.cols ?? 80,
            rows: message.rows ?? 24,
          });
          break;
        case 'terminal-refresh':
          result = await requireService().terminalRefresh();
          break;
        case 'terminal-input':
          result = await requireService().terminalInput({
            text: String(message.text ?? ''),
            enter: message.enter !== false,
          });
          break;
        case 'terminal-data':
          result = await requireService().terminalData({
            data: String(message.data ?? ''),
          });
          break;
        case 'pump':
          result = await requireService().runInputLoop({
            ...loopOptions(message),
          });
          break;
        case 'pump-start': {
          const currentService = requireService();
          result = currentService.startInputLoop(loopOptions(message));
          if (result.started) {
            attachLoopCompletion(currentService);
          }
          break;
        }
        case 'pump-stop':
          result = await requireService().stopInputLoop();
          break;
        case 'save':
          result = await requireService().save();
          break;
        case 'replay':
          result = await requireService().replay({
            args: Array.isArray(message.args) ? message.args.map(String) : null,
            shutdownRuntime: message.shutdownRuntime !== false,
          });
          if (message.shutdownRuntime !== false) {
            flushWasmLogs();
            service = null;
          }
          break;
        case 'snapshot':
          result = {
            snapshot: publicRuntimeSnapshot(requireService().snapshot()),
          };
          break;
        case 'shutdown':
          if (service && loopCompletionPromise) {
            await service.stopInputLoop();
          }
          result = await requireService().shutdown();
          flushWasmLogs();
          service = null;
          loopCompletionPromise = null;
          break;
        default:
          throw new Error(`unknown command: ${message.type}`);
      }

      const response = {
        type: 'result',
        id,
        command: message.type,
        result: publicResult(result),
      };
      emit(response, collectTransfers(response));
    }
    catch (error) {
      emit({
        type: 'error',
        id,
        command: message.type,
        error: String(error?.stack ?? error),
      });
    }
  }

  return {
    handle,
    currentService() {
      return service;
    },
  };
}

if (
  typeof globalThis.document === 'undefined' &&
  typeof globalThis.addEventListener === 'function' &&
  typeof globalThis.postMessage === 'function'
) {
  const handler = createAmesRuntimeWorkerHandler({
    emit: (message, transfer = []) => globalThis.postMessage(message, transfer),
  });
  globalThis.addEventListener('message', event => {
    handler.handle(event.data);
  });
}
