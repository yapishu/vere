import {
  AmesWasmRuntimeService,
  publicRuntimeSnapshot,
} from './ames-wasm-runtime-service.mjs';
import {
  IndexedDBVereWasmFileStore,
} from './vere-wasm-host.mjs';

const decoder = new TextDecoder();

function asBigInt(value, name) {
  if (value == null || value === '') {
    throw new Error(`${name} is required`);
  }
  return BigInt(value);
}

function asBytes(value, name) {
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

function writeBytes(emit, className, bytes) {
  const text = decoder.decode(bytes).replace(/\r/g, '');
  for (const line of text.split('\n')) {
    if (line) {
      emit({ type: 'log', message: `wasm: ${line}`, className });
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

  const requireService = () => {
    if (!service) {
      throw new Error('runtime service is not started');
    }
    return service;
  };

  async function start(message) {
    if (service) {
      await service.shutdown();
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
      onStdout: bytes => writeBytes(emit, '', bytes),
      onStderr: bytes => writeBytes(emit, 'err', bytes),
      onPacket: event => emit({
        type: 'packet',
        mode: event.mode,
        bytes: [...event.packet],
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
