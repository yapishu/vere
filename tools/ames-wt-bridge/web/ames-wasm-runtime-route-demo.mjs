import {
  AmesWasmRuntimeService,
} from './ames-wasm-runtime-service.mjs';
import {
  IndexedDBVereWasmFileStore,
} from './vere-wasm-host.mjs';

const decoder = new TextDecoder();

function requireString(value, name) {
  if (typeof value !== 'string' || value === '') {
    throw new Error(`${name} is required`);
  }
  return value;
}

function requireNumber(value, name) {
  if (!Number.isSafeInteger(value) || value < 0) {
    throw new Error(`${name} must be a non-negative safe integer`);
  }
  return value;
}

function parseShip(value, name) {
  return BigInt(requireString(value, name));
}

export function normalizeRuntimeRouteDemoConfig(input = {}) {
  const wasmUrl = new URL(requireString(input.wasmUrl, 'wasmUrl'));
  const pillUrl = new URL(requireString(input.pillUrl, 'pillUrl'));
  const peer = parseShip(input.peer, 'peer');
  const overheadMiB = requireNumber(input.overheadMiB, 'overheadMiB');
  const maximumMiB = requireNumber(input.maximumMiB, 'maximumMiB');

  return {
    wasmUrl,
    pillUrl,
    bridgeUrl: requireString(input.bridgeUrl, 'bridgeUrl'),
    certificateHash: String(input.certificateHash ?? ''),
    scope: String(input.scope || 'vere-disk-route-smoke'),
    overheadMiB,
    maximumMiB,
    memoryOptions: {
      loomExponent: requireNumber(input.loomExponent, 'loomExponent'),
      overheadBytes: overheadMiB * 1024 * 1024,
      maximumBytes: maximumMiB * 1024 * 1024,
    },
    fakeShip: parseShip(input.fakeShip, 'fakeShip'),
    peer,
    matePeer: parseShip(input.matePeer ?? input.peer, 'matePeer'),
    sessionId: parseShip(input.sessionId, 'sessionId'),
    keenPath: String(input.keenPath || '/c/x/1/kids/sys/kelvin'),
    maxRounds: requireNumber(input.maxRounds, 'maxRounds'),
    idleRoundsToStop: requireNumber(input.idleRoundsToStop, 'idleRoundsToStop'),
    firstDelayMs: requireNumber(input.firstDelayMs, 'firstDelayMs'),
    delayMs: requireNumber(input.delayMs, 'delayMs'),
    minReplies: requireNumber(input.minReplies, 'minReplies'),
  };
}

export function routeSummaryLines(summary) {
  return [
    `initial-sent=${summary.initial.sent}`,
    `reply-packets=${summary.loop.packets}`,
    `reply-routed=${summary.loop.routedPushes}`,
    `replay-exit=${summary.replay?.exitCode ?? 'skipped'}`,
  ];
}

function routeCount(value) {
  return Number.isSafeInteger(value) && value >= 0 ? value : 0;
}

function commandError(message) {
  const error = new Error(message.error || `runtime worker ${message.command} failed`);
  error.workerMessage = message;
  return error;
}

function routeFailures(summary, {
  minInitialSends = 1,
  minReplies = 1,
  maxInitialDrops = 0,
  maxDroppedRoutes = 0,
  maxDroppedInbound = 0,
  requireReplay = true,
} = {}) {
  const failures = [];

  if (summary.initial.sent < minInitialSends) {
    failures.push(`sent ${summary.initial.sent} initial packets, want >= ${minInitialSends}`);
  }
  if (summary.initial.dropped > maxInitialDrops) {
    failures.push(`dropped ${summary.initial.dropped} initial routes, want <= ${maxInitialDrops}`);
  }
  if (summary.loop.packets < minReplies) {
    failures.push(`received ${summary.loop.packets} WebTransport replies, want >= ${minReplies}`);
  }
  if (summary.loop.droppedRoutes > maxDroppedRoutes) {
    failures.push(`dropped ${summary.loop.droppedRoutes} reply routes, want <= ${maxDroppedRoutes}`);
  }
  if (summary.loop.droppedInbound > maxDroppedInbound) {
    failures.push(`dropped ${summary.loop.droppedInbound} inbound packets, want <= ${maxDroppedInbound}`);
  }
  if (requireReplay && summary.replay?.exitCode !== 0) {
    failures.push(`replay exit ${summary.replay?.exitCode ?? 'missing'}, want 0`);
  }

  return failures;
}

export function assertRuntimeRouteSummary(summary, options = {}) {
  const failures = routeFailures(summary, options);
  summary.ok = failures.length === 0;
  summary.failures = failures;

  if (failures.length) {
    const error = new Error(`WASM runtime route demo failed: ${failures.join('; ')}`);
    error.summary = summary;
    throw error;
  }

  return summary;
}

function emitConfig(emit, config, input) {
  for (const message of [
    `wasm=${config.wasmUrl.href}`,
    `pill=${config.pillUrl.href}`,
    `bridge=${config.bridgeUrl}`,
    `fake-ship=${config.fakeShip.toString()}`,
    `peer=${config.peer.toString()}`,
    `keen-path=${config.keenPath}`,
    `memory=loom-${config.memoryOptions.loomExponent} ` +
      `overhead-${config.overheadMiB}MiB maximum-${config.maximumMiB}MiB`,
  ]) {
    emit({ type: 'log', message });
  }
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

export function createRuntimeWorkerCommandClient(worker, {
  emit = () => {},
} = {}) {
  let nextId = 0;
  const pending = new Map();

  const rejectPending = error => {
    for (const { reject } of pending.values()) {
      reject(error);
    }
    pending.clear();
  };

  worker.addEventListener('message', event => {
    const message = event.data ?? {};
    if (message.type === 'log') {
      emit(message);
      return;
    }
    if (message.type === 'packet') {
      emit({
        type: 'log',
        message: `rx ${message.mode} ${message.bytes?.length ?? 0}B`,
        className: 'rx',
      });
      return;
    }

    const id = message.id;
    const entry = pending.get(id);
    if (!entry) {
      return;
    }
    pending.delete(id);

    if (message.type === 'result') {
      entry.resolve(message.result);
    }
    else if (message.type === 'error') {
      entry.reject(commandError(message));
    }
  });

  worker.addEventListener('error', event => {
    rejectPending(event.error ?? new Error(event.message));
  });

  return {
    command(type, body = {}) {
      const id = ++nextId;
      return new Promise((resolve, reject) => {
        pending.set(id, { resolve, reject });
        try {
          worker.postMessage({ ...body, id, type });
        }
        catch (error) {
          pending.delete(id);
          reject(error);
        }
      });
    },
    terminate() {
      rejectPending(new Error('runtime worker terminated'));
      worker.terminate?.();
    },
  };
}

function browserRuntimeWorker() {
  return new Worker(
    new URL('./ames-wasm-runtime-worker.mjs', import.meta.url),
    { type: 'module' },
  );
}

export async function runRuntimeWorkerRouteDemo(input, {
  createWorker = browserRuntimeWorker,
  emit = () => {},
} = {}) {
  const config = normalizeRuntimeRouteDemoConfig(input);
  const worker = createWorker();
  const client = createRuntimeWorkerCommandClient(worker, { emit });
  let serviceClosed = false;

  try {
    emitConfig(emit, config, input);

    await client.command('start', {
      wasmUrl: config.wasmUrl.href,
      pillUrl: config.pillUrl.href,
      bridgeUrl: config.bridgeUrl,
      certificateHash: config.certificateHash,
      scope: config.scope,
      loomExponent: config.memoryOptions.loomExponent,
      overheadMiB: config.overheadMiB,
      maximumMiB: config.maximumMiB,
      fakeShip: config.fakeShip.toString(),
      sessionId: config.sessionId.toString(),
      clearStore: true,
    });
    await client.command('connect', {
      bridgeUrl: config.bridgeUrl,
      certificateHash: config.certificateHash,
    });

    const mate = await client.command('mate', {
      ship: config.matePeer.toString(),
    });
    const keen = await client.command('keen', {
      ship: config.peer.toString(),
      path: config.keenPath,
    });
    const initial = {
      sent: routeCount(mate.routes?.sent) + routeCount(keen.routes?.sent),
      dropped: routeCount(mate.routes?.dropped) + routeCount(keen.routes?.dropped),
    };
    emit({
      type: 'log',
      message: `initial-routes sent=${initial.sent} dropped=${initial.dropped}`,
    });

    const pumped = await client.command('pump', {
      maxRounds: config.maxRounds,
      firstIdleTimeoutMs: config.firstDelayMs,
      idleTimeoutMs: config.idleRoundsToStop * config.delayMs,
    });
    const loop = pumped.loop;
    emit({
      type: 'log',
      message:
        `runtime-loop complete packets=${loop.packets} sent=${loop.routedPushes} ` +
        `dropped-routes=${loop.droppedRoutes} stopped=${loop.stoppedBy}`,
    });

    const saved = await client.command('save');
    const replayed = await client.command('replay', {
      shutdownRuntime: true,
    });
    serviceClosed = true;

    const summary = {
      ok: false,
      bridgeUrl: config.bridgeUrl,
      fakeShip: config.fakeShip,
      peer: config.peer,
      sessionId: config.sessionId,
      memoryOptions: config.memoryOptions,
      initial,
      loop,
      replay: replayed.replay,
      finalEvent: replayed.snapshot?.event ?? saved.snapshot?.event ?? pumped.snapshot?.event,
    };

    const checked = assertRuntimeRouteSummary(summary, {
      minReplies: config.minReplies,
    });
    for (const line of routeSummaryLines(checked)) {
      emit({ type: 'log', message: line });
    }
    return checked;
  }
  finally {
    if (!serviceClosed) {
      try {
        await client.command('shutdown');
      }
      catch (_) {}
    }
    client.terminate();
  }
}

export async function runRuntimeServiceRouteDemo(input, {
  Service = AmesWasmRuntimeService,
  FileStore = IndexedDBVereWasmFileStore,
  fetchFn = globalThis.fetch,
  emit = () => {},
} = {}) {
  if (typeof fetchFn !== 'function') {
    throw new Error('fetch is required');
  }

  const config = normalizeRuntimeRouteDemoConfig(input);
  emitConfig(emit, config, input);

  const pillBytes = await fetchBytes(fetchFn, config.pillUrl);
  emit({ type: 'log', message: `pill-bytes=${pillBytes.length}` });

  const fileStore = new FileStore({ scope: config.scope });
  await fileStore.clear();

  let service = new Service({
    wasmUrl: config.wasmUrl,
    pillBytes,
    fileStore,
    bridgeUrl: config.bridgeUrl,
    certificateHash: config.certificateHash,
    memoryOptions: config.memoryOptions,
    fakeShip: config.fakeShip,
    sessionId: config.sessionId,
    onLog: event => emit({ type: 'log', ...event }),
    onStdout: bytes => writeBytes(emit, '', bytes),
    onStderr: bytes => writeBytes(emit, 'err', bytes),
  });
  let serviceClosed = false;

  try {
    await service.start();
    await service.connect();
    const mate = await service.mate({
      ship: config.matePeer,
    });
    const keen = await service.keen({
      ship: config.peer,
      path: config.keenPath,
    });
    const initial = {
      sent: routeCount(mate.routes?.sent) + routeCount(keen.routes?.sent),
      dropped: routeCount(mate.routes?.dropped) + routeCount(keen.routes?.dropped),
    };
    emit({
      type: 'log',
      message: `initial-routes sent=${initial.sent} dropped=${initial.dropped}`,
    });

    const pumped = await service.runInputLoop({
      maxRounds: config.maxRounds,
      firstIdleTimeoutMs: config.firstDelayMs,
      idleTimeoutMs: config.idleRoundsToStop * config.delayMs,
    });
    const loop = pumped.loop;
    emit({
      type: 'log',
      message:
        `runtime-loop complete packets=${loop.packets} sent=${loop.routedPushes} ` +
        `dropped-routes=${loop.droppedRoutes} stopped=${loop.stoppedBy}`,
    });

    const saved = await service.save();
    const replayed = await service.replay({
      shutdownRuntime: true,
    });
    serviceClosed = true;

    const summary = {
      ok: false,
      bridgeUrl: config.bridgeUrl,
      fakeShip: config.fakeShip,
      peer: config.peer,
      sessionId: config.sessionId,
      memoryOptions: config.memoryOptions,
      initial,
      loop,
      replay: replayed.replay,
      finalEvent: replayed.snapshot?.event ?? saved.snapshot?.event ?? pumped.snapshot?.event,
    };

    const checked = assertRuntimeRouteSummary(summary, {
      minReplies: config.minReplies,
    });
    for (const line of routeSummaryLines(checked)) {
      emit({ type: 'log', message: line });
    }
    return checked;
  }
  finally {
    if (!serviceClosed) {
      try {
        await service.shutdown();
      }
      catch (_) {}
    }
    service = null;
  }
}
