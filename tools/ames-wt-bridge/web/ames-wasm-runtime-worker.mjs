import {
  AmesWasmRuntimeService,
  publicRuntimeSnapshot,
} from './ames-wasm-runtime-service.mjs';
import {
  IndexedDBVereWasmFileStore,
} from './vere-wasm-host.mjs';

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
    return [...value];
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

async function fetchBytes(fetchFn, url) {
  const response = await fetchFn(url);
  if (!response.ok) {
    throw new Error(`fetch ${url} failed: ${response.status}`);
  }
  return new Uint8Array(await response.arrayBuffer());
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
    if (line) {
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
    if (message.clearStore !== false) {
      await fileStore.clear();
    }
    const pillBytes = await fetchBytes(fetchFn, pillUrl);
    emit({ type: 'log', message: `pill-bytes=${pillBytes.length}` });

    service = new Service({
      wasmUrl,
      pillBytes,
      fileStore,
      bridgeUrl: message.bridgeUrl,
      certificateHash: message.certificateHash || '',
      memoryOptions: normalizeMemory(message),
      fakeShip: asBigInt(message.fakeShip ?? '0x100', 'fakeShip'),
      sessionId: asBigInt(message.sessionId ?? '1', 'sessionId'),
      onLog: event => emit({ type: 'log', ...event }),
      onStdout: bytes => wasmStdout.write(bytes),
      onStderr: bytes => wasmStderr.write(bytes),
      onPacket: event => emit({
        type: 'packet',
        mode: event.mode,
        bytes: [...event.packet],
      }),
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
        emit({
          type: 'loop',
          status: 'complete',
          result: publicResult(result),
        });
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
            local: message.local !== false,
            timeoutMs: message.timeoutMs,
          });
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

      emit({
        type: 'result',
        id,
        command: message.type,
        result: publicResult(result),
      });
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
    emit: message => globalThis.postMessage(message),
  });
  globalThis.addEventListener('message', event => {
    handler.handle(event.data);
  });
}
