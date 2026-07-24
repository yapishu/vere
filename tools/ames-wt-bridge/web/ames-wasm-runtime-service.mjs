import { AmesWebTransportClient, webTransportOptions } from './ames-client.mjs';
import {
  bornOvumJam,
  keenOvumJam,
  mateOvumJam,
  mesaHeerOvumJam,
} from './ames-wasm-events.mjs';
import {
  decodeEffectList,
  extractMesaEffects,
} from './ames-wasm-effects.mjs';
import { routeMesaEffects } from './ames-wasm-router.mjs';
import { udpLaneNoun } from './ames-udp-frame.mjs';
import {
  BoundedPacketQueue,
  runWasmMesaEventLoop,
} from './ames-wasm-runtime-loop.mjs';
import {
  BrowserHttpClientHost,
  httpClientBornOvumJam,
} from './ames-wasm-http-client.mjs';
import {
  BrowserBehnTimerHost,
  behnBornOvumJam,
} from './ames-wasm-behn.mjs';
import {
  BrowserHttpServerHost,
} from './ames-wasm-http-server.mjs';
import {
  BrowserTerminalHost,
} from './ames-wasm-terminal.mjs';
import {
  instantiateVereDiskWasmRuntime,
  runVereWasmProbeWithFileStore,
} from './vere-wasm-host.mjs';

const DEFAULT_MEMORY_OPTIONS = Object.freeze({
  loomExponent: 29,
  overheadBytes: 512 * 1024 * 1024,
  maximumBytes: 1536 * 1024 * 1024,
});

const DEFAULT_LARGE_HTTP_SERVER_EFFECT_BYTES = 256 * 1024;

function packetCopy(event) {
  return {
    lane: { ...event.lane },
    packet: Uint8Array.from(event.packet),
  };
}

function routeCount(value) {
  return Array.isArray(value) ? value.length : 0;
}

function log(onLog, message, className = '') {
  onLog({ message, className });
}

function isRoutineHostLabel(label) {
  return /^(?:terminal-data-\d+|behn-wake|http-(?:receive|continue|cancel)-\d+)(?:-effects)?$/.test(label);
}

function isLargeHttpServerEffectsLabel(label, effectsBytes, threshold) {
  return (
    effectsBytes?.length >= threshold &&
    /^http-server-request-\d+-\d+-effects$/.test(label)
  );
}

function commitPriority(label) {
  if (/^terminal-(?:data|born|blew|hail|text|ret)(?:-\d+)?$/.test(label)) {
    return 0;
  }
  if (/^(?:http-(?:receive|continue|cancel)-\d+|behn-wake)$/.test(label)) {
    return 2;
  }
  return 1;
}

function effectSummary(effects) {
  if (!effects) {
    return {
      bytes: 0,
      sends: 0,
      pushes: 0,
      binds: 0,
      unknown: 0,
    };
  }

  const mesa = extractMesaEffects(effects.noun ?? effects);
  return {
    bytes: effects.bytes ?? effects.length ?? 0,
    sends: mesa.sends.length,
    pushes: mesa.pushes.length,
    binds: mesa.binds.length,
    unknown: mesa.unknown.length,
  };
}

function logEffectSummary(onLog, label, bytes) {
  const summary = effectSummary(bytes);
  if (!isRoutineHostLabel(label)) {
    log(
      onLog,
      `${label}=bytes=${summary.bytes} sends=${summary.sends} ` +
        `pushes=${summary.pushes} binds=${summary.binds} unknown=${summary.unknown}`,
    );
  }
  return summary;
}

function emptyMesaRoutes() {
  return {
    sent: [],
    dropped: [],
    bound: [],
    droppedBinds: [],
  };
}

function emptyHttpClientRoute() {
  return {
    requests: 0,
    cancels: 0,
    unknown: 0,
    pending: 0,
  };
}

function emptyBehnRoute() {
  return {
    dozes: 0,
    unknown: 0,
    active: false,
  };
}

function emptyHttpServerRoute() {
  return {
    responses: 0,
    configs: 0,
    sessions: 0,
    grows: 0,
    unknown: 0,
    pending: 0,
  };
}

function emptyTerminalRoute() {
  return {
    events: 0,
    blits: 0,
    logos: 0,
    unknown: 0,
    bufferChars: 0,
  };
}

function defaultRuntimeFactory(wasmUrl, options) {
  return instantiateVereDiskWasmRuntime(wasmUrl, options);
}

function defaultReplayProbe(wasmUrl, options) {
  return runVereWasmProbeWithFileStore(wasmUrl, options);
}

function defaultClientFactory(input) {
  return new AmesWebTransportClient(input);
}

function defaultHttpClientHostFactory(input) {
  return new BrowserHttpClientHost(input);
}

function defaultBehnHostFactory(input) {
  return new BrowserBehnTimerHost(input);
}

function defaultHttpServerHostFactory(input) {
  return new BrowserHttpServerHost(input);
}

function defaultTerminalHostFactory(input) {
  return new BrowserTerminalHost(input);
}

function sanitizeLabel(label) {
  return String(label).replace(/[^a-zA-Z0-9_.-]/g, '-');
}

function publicRouteCount(routes) {
  return {
    sent: routeCount(routes?.sent),
    dropped: routeCount(routes?.dropped),
    bound: routeCount(routes?.bound),
    droppedBinds: routeCount(routes?.droppedBinds),
    http: routes?.http ?? {
      requests: 0,
      cancels: 0,
      unknown: 0,
      pending: 0,
    },
    behn: routes?.behn ?? {
      dozes: 0,
      unknown: 0,
      active: false,
    },
    terminal: routes?.terminal ?? {
      events: 0,
      blits: 0,
      logos: 0,
      unknown: 0,
      bufferChars: 0,
    },
  };
}

export function publicRuntimeSnapshot(snapshot) {
  return {
    started: snapshot.started,
    connected: snapshot.connected,
    event: snapshot.event?.toString?.() ?? snapshot.event,
    fakeShip: snapshot.fakeShip?.toString?.() ?? snapshot.fakeShip,
    sessionId: snapshot.sessionId?.toString?.() ?? snapshot.sessionId,
    inputLoopActive: Boolean(snapshot.inputLoopActive),
    queue: snapshot.queue,
    hostedHttp: snapshot.hostedHttp ?? null,
    hostedHttpStarted: Boolean(snapshot.hostedHttpStarted),
    hostedBehn: snapshot.hostedBehn ?? null,
    hostedHttpServer: snapshot.hostedHttpServer ?? null,
    hostedTerminal: snapshot.hostedTerminal ?? null,
    totals: snapshot.totals,
  };
}

export class AmesWasmRuntimeService {
  constructor({
    wasmUrl,
    pillBytes,
    fileStore,
    bridgeUrl = 'https://127.0.0.1:8443/~_~/ames',
    certificateHash = '',
    memoryOptions = DEFAULT_MEMORY_OPTIONS,
    fakeShip = 0x100n,
    bootMode = 'fake',
    bootFiles = {},
    sessionId = 1n,
    maxPackets = 32,
    runtimeFactory = defaultRuntimeFactory,
    replayProbe = defaultReplayProbe,
    clientFactory = defaultClientFactory,
    httpClientHost = null,
    httpClientHostFactory = defaultHttpClientHostFactory,
    autoStartHttpClient = true,
    autoSave = true,
    behnHost = null,
    behnHostFactory = defaultBehnHostFactory,
    httpServerHost = null,
    httpServerHostFactory = defaultHttpServerHostFactory,
    terminalHost = null,
    terminalHostFactory = defaultTerminalHostFactory,
    onLog = () => {},
    onStdout = () => {},
    onStderr = () => {},
    onPacket = () => {},
    onHttpServer = () => {},
    onTerminal = () => {},
    largeHttpServerEffectBytes = DEFAULT_LARGE_HTTP_SERVER_EFFECT_BYTES,
  } = {}) {
    if (!wasmUrl) {
      throw new Error('wasmUrl is required');
    }
    if (!pillBytes) {
      throw new Error('pillBytes is required');
    }
    if (!fileStore) {
      throw new Error('fileStore is required');
    }

    this.wasmUrl = wasmUrl;
    this.pillBytes = pillBytes;
    this.fileStore = fileStore;
    this.bridgeUrl = bridgeUrl;
    this.certificateHash = certificateHash;
    this.memoryOptions = { ...DEFAULT_MEMORY_OPTIONS, ...memoryOptions };
    this.fakeShip = BigInt(fakeShip);
    this.bootMode = String(bootMode);
    this.bootFiles = Object.fromEntries(
      Object.entries(bootFiles).map(([path, bytes]) => [path, Uint8Array.from(bytes)]),
    );
    this.sessionId = BigInt(sessionId);
    this.runtimeFactory = runtimeFactory;
    this.replayProbe = replayProbe;
    this.clientFactory = clientFactory;
    this.autoStartHttpClient = Boolean(autoStartHttpClient);
    this.autoSave = Boolean(autoSave);
    this.onLog = onLog;
    this.onStdout = onStdout;
    this.onStderr = onStderr;
    this.onPacket = onPacket;
    this.onHttpServer = onHttpServer;
    this.onTerminal = onTerminal;
    this.largeHttpServerEffectBytes = largeHttpServerEffectBytes;
    this.httpClientHost = httpClientHost ?? httpClientHostFactory({
      onLog: event => log(this.onLog, event.message, event.className),
    });
    this.behnHost = behnHost ?? behnHostFactory({
      onLog: event => {
        if (/^behn timer (?:scheduled|cleared)/.test(event.message)) {
          return;
        }
        log(this.onLog, event.message, event.className);
      },
    });
    this.httpServerHost = httpServerHost ?? httpServerHostFactory({
      onLog: event => log(this.onLog, event.message, event.className),
    });
    this.terminalHost = terminalHost ?? terminalHostFactory({
      onTerminal: event => this.onTerminal(event),
    });

    this.runtime = null;
    this.client = null;
    this.started = false;
    this.httpClientStarted = false;
    this.closed = false;
    this.inputLoopAbortController = null;
    this.inputLoopPromise = null;
    this.runtimeCommitBusy = false;
    this.runtimeCommitWaiters = [];
    this.runtimeCommitWaiterOrder = 0;
    this.nextFileId = 0;
    this.nextPacketId = 0;
    this.inboundPackets = new BoundedPacketQueue({
      maxPackets,
      copy: packetCopy,
      onDrop: drop => {
        log(this.onLog, `dropped inbound WebTransport packet count=${drop.dropped}`, 'err');
      },
    });
    this.totals = {
      sent: 0,
      droppedRoutes: 0,
      bound: 0,
      droppedBinds: 0,
      inboundPackets: 0,
      httpRequests: 0,
      httpCancels: 0,
      httpServerRequests: 0,
      httpServerResponses: 0,
      terminalEvents: 0,
      terminalBlits: 0,
    };
  }

  snapshot() {
    return {
      started: this.started,
      connected: Boolean(this.client?.sessionOpen),
      event: this.runtime?.event?.() ?? null,
      fakeShip: this.fakeShip,
      sessionId: this.sessionId,
      inputLoopActive: Boolean(this.inputLoopPromise),
      queue: this.inboundPackets.stats(),
      hostedHttp: this.httpClientHost?.snapshot?.() ?? null,
      hostedHttpStarted: this.httpClientStarted,
      hostedBehn: this.behnHost?.snapshot?.() ?? null,
      hostedHttpServer: this.httpServerHost?.snapshot?.() ?? null,
      hostedTerminal: this.terminalHost?.snapshot?.() ?? null,
      totals: { ...this.totals },
    };
  }

  async start() {
    this.#ensureOpen();
    if (this.started) {
      return {
        alreadyStarted: true,
        snapshot: this.snapshot(),
      };
    }

    log(this.onLog, `loading ${this.wasmUrl} ...`);
    this.runtime = await this.runtimeFactory(this.wasmUrl, {
      fileStore: this.fileStore,
      initialFiles: {
        '/brass.pill': this.pillBytes,
        ...this.bootFiles,
      },
      memoryOptions: this.memoryOptions,
      onStdout: this.onStdout,
      onStderr: this.onStderr,
    });

    this.runtime.init({
      loomExponent: this.memoryOptions.loomExponent,
      ship: this.fakeShip,
      fake: this.bootMode !== 'owned',
    });
    // Boot inputs include private key material and are not part of the pier.
    // Drop them before the first IndexedDB checkpoint.
    this.runtime.host?.files?.delete('/brass.pill');
    for (const path of this.runtime.host?.files?.keys?.() ?? []) {
      if (String(path).startsWith('/boot/')) {
        this.runtime.host.files.delete(path);
      }
    }
    this.started = true;
    log(this.onLog, `runtime initialized event=${this.runtime.event()}`, 'rx');

    const loadPath = this.#effectsPath('load-mesa');
    this.runtime.pokeLoadMesa({ effectsPath: loadPath });
    log(this.onLog, `loaded Mesa event=${this.runtime.event()}`, 'rx');
    const load = await this.#routeEffects(
      'load-effects',
      this.runtime.host.files.get(loadPath),
    );
    const behnBorn = this.behnHost
      ? await this.#pokeOvum({
        label: 'behn-born',
        ovumBytes: behnBornOvumJam(),
      })
      : null;
    const httpBorn = this.autoStartHttpClient
      ? await this.httpClientStart()
      : null;
    const httpServerBorn = this.httpServerHost
      ? await this.#pokeOvum({
        label: 'http-server-born',
        ovumBytes: this.httpServerHost.bornOvumJam(),
      })
      : null;
    const httpServerLive = this.httpServerHost
      ? await this.#pokeOvum({
        label: 'http-server-live',
        ovumBytes: this.httpServerHost.liveOvumJam(),
      })
      : null;
    const born = await this.#pokeOvum({
      label: 'born',
      ovumBytes: bornOvumJam(),
    });
    if (this.autoSave) {
      await this.runtime.save?.();
      log(this.onLog, `persisted browser pier event=${this.runtime.event()}`, 'rx');
    }

    return {
      load,
      behnBorn,
      httpBorn,
      httpServerBorn,
      httpServerLive,
      born,
      snapshot: this.snapshot(),
    };
  }

  async connect({
    bridgeUrl = this.bridgeUrl,
    certificateHash = this.certificateHash,
  } = {}) {
    this.#ensureStarted();
    this.bridgeUrl = bridgeUrl;
    this.certificateHash = certificateHash;

    if (!this.client) {
      this.client = this.clientFactory({
        url: this.bridgeUrl,
        options: webTransportOptions({ certificateHash: this.certificateHash }),
        onStatus: status => {
          if (status.type === 'connecting') {
            log(this.onLog, `dialing ${status.url} ...`);
          }
          if (status.type === 'open') {
            log(this.onLog, 'WebTransport session open', 'rx');
          }
          if (status.type === 'closed' && !status.terminal) {
            log(this.onLog, 'WebTransport session closed');
          }
        },
        onPacket: event => {
          this.inboundPackets.push(event);
          this.totals.inboundPackets++;
          this.onPacket(event);
          log(this.onLog, `rx ${event.mode} ${event.packet.length}B`, 'rx');
        },
        onError: ({ type, error }) => {
          log(this.onLog, `${type}: ${error}`, 'err');
        },
      });
    }

    if (typeof this.client.connect === 'function') {
      await this.client.connect();
    }

    return {
      snapshot: this.snapshot(),
    };
  }

  async closeTransport() {
    if (!this.client) {
      return {
        snapshot: this.snapshot(),
      };
    }

    await this.client.close?.({ reason: 'browser runtime service close' });
    this.client = null;
    this.inboundPackets.close();
    return {
      snapshot: this.snapshot(),
    };
  }

  async mate({ ship, dry = false, label = 'mate' } = {}) {
    if (ship == null) {
      throw new Error('ship is required');
    }
    return this.#pokeOvum({
      label,
      ovumBytes: mateOvumJam({ ship: BigInt(ship), dry }),
    });
  }

  async keen({
    ship,
    path,
    security = 0n,
    label = 'keen',
  } = {}) {
    if (ship == null) {
      throw new Error('ship is required');
    }
    return this.#pokeOvum({
      label,
      ovumBytes: keenOvumJam({
        ship: BigInt(ship),
        path,
        security,
      }),
    });
  }

  async injectPacket({
    packet,
    lane,
    label = `reply-${this.nextPacketId++}`,
  } = {}) {
    if (packet == null) {
      throw new Error('packet is required');
    }
    if (lane == null) {
      throw new Error('source lane is required');
    }
    return this.#pokeOvum({
      label,
      ovumBytes: mesaHeerOvumJam({
        lane: udpLaneNoun(lane),
        packet,
      }),
    });
  }

  async httpRequest({
    method = 'GET',
    url = '/',
    headers = [],
    body = null,
    secure = false,
    local = false,
    timeoutMs = undefined,
    stream = false,
    streamId = null,
  } = {}) {
    this.#ensureStarted();
    if (!this.httpServerHost) {
      throw new Error('http-server host is not enabled');
    }

    this.totals.httpServerRequests++;
    return this.httpServerHost.handleRequest({
      method,
      url,
      headers,
      body,
      secure,
      local,
      timeoutMs,
      stream,
      streamId,
      onStream: event => this.onHttpServer(event),
    }, {
      injectOvum: ({ label, ovumBytes }) => this.#pokeOvum({ label, ovumBytes }),
    });
  }

  async httpRequestCancel({
    service,
    connectionId,
    requestId,
  } = {}) {
    this.#ensureStarted();
    if (!this.httpServerHost) {
      throw new Error('http-server host is not enabled');
    }
    if (connectionId == null) {
      throw new Error('connectionId is required');
    }
    if (requestId == null) {
      throw new Error('requestId is required');
    }

    return this.httpServerHost.cancelRequest({
      service: service ?? this.httpServerHost.service,
      connectionId: BigInt(connectionId),
      requestId: BigInt(requestId),
    }, {
      injectOvum: ({ label, ovumBytes }) => this.#pokeOvum({ label, ovumBytes }),
    });
  }

  async terminalStart({
    cols = 80,
    rows = 24,
  } = {}) {
    this.#ensureStarted();
    if (!this.terminalHost) {
      throw new Error('terminal host is not enabled');
    }

    const born = await this.#pokeOvum({
      label: 'terminal-born',
      ovumBytes: this.terminalHost.bornOvumJam(),
    });
    const blew = await this.#pokeOvum({
      label: 'terminal-blew',
      ovumBytes: this.terminalHost.blewOvumJam({ cols, rows }),
    });
    const hail = await this.#pokeOvum({
      label: 'terminal-hail',
      ovumBytes: this.terminalHost.hailOvumJam(),
    });
    this.terminalHost.started = true;

    return {
      born,
      blew,
      hail,
      snapshot: this.snapshot(),
    };
  }

  async httpClientStart() {
    this.#ensureStarted();
    if (!this.httpClientHost) {
      throw new Error('http-client host is not enabled');
    }
    if (this.httpClientStarted) {
      return {
        alreadyStarted: true,
        snapshot: this.snapshot(),
      };
    }

    const httpBorn = await this.#pokeOvum({
      label: 'http-client-born',
      ovumBytes: httpClientBornOvumJam(),
    });
    this.httpClientStarted = true;

    return {
      ...httpBorn,
      snapshot: this.snapshot(),
    };
  }

  async terminalResize({
    cols = 80,
    rows = 24,
  } = {}) {
    this.#ensureStarted();
    if (!this.terminalHost) {
      throw new Error('terminal host is not enabled');
    }
    const resized = await this.#pokeOvum({
      label: 'terminal-blew',
      ovumBytes: this.terminalHost.blewOvumJam({ cols, rows }),
    });
    return {
      resized,
      snapshot: this.snapshot(),
    };
  }

  async terminalRefresh() {
    this.#ensureStarted();
    if (!this.terminalHost) {
      throw new Error('terminal host is not enabled');
    }
    const refreshed = await this.#pokeOvum({
      label: 'terminal-hail',
      ovumBytes: this.terminalHost.hailOvumJam(),
    });
    return {
      refreshed,
      snapshot: this.snapshot(),
    };
  }

  async terminalInput({
    text = '',
    enter = true,
  } = {}) {
    this.#ensureStarted();
    if (!this.terminalHost) {
      throw new Error('terminal host is not enabled');
    }

    const events = [];
    if (String(text).length > 0) {
      events.push(await this.#pokeOvum({
        label: 'terminal-text',
        ovumBytes: this.terminalHost.textOvumJam({ text }),
      }));
    }
    if (enter) {
      events.push(await this.#pokeOvum({
        label: 'terminal-ret',
        ovumBytes: this.terminalHost.retOvumJam(),
      }));
    }

    return {
      events,
      snapshot: this.snapshot(),
    };
  }

  async terminalData({
    data = '',
  } = {}) {
    this.#ensureStarted();
    if (!this.terminalHost) {
      throw new Error('terminal host is not enabled');
    }

    const events = [];
    let index = 0;
    for (const ovumBytes of this.terminalHost.dataOvumJams({ data })) {
      events.push(await this.#pokeOvum({
        label: `terminal-data-${index++}`,
        ovumBytes,
      }));
    }

    return {
      events,
      snapshot: this.snapshot(),
    };
  }

  async runInputLoop({
    maxRounds = Infinity,
    firstIdleTimeoutMs = null,
    idleTimeoutMs = null,
    signal = null,
  } = {}) {
    if (this.inputLoopPromise) {
      throw new Error('runtime input loop is already running');
    }
    return this.#runInputLoop({
      maxRounds,
      firstIdleTimeoutMs,
      idleTimeoutMs,
      signal,
    });
  }

  startInputLoop({
    maxRounds = Infinity,
    firstIdleTimeoutMs = null,
    idleTimeoutMs = null,
  } = {}) {
    this.#ensureStarted();
    if (this.inputLoopPromise) {
      return {
        alreadyStarted: true,
        snapshot: this.snapshot(),
      };
    }

    this.inputLoopAbortController = new AbortController();
    const promise = this.#runInputLoop({
      maxRounds,
      firstIdleTimeoutMs,
      idleTimeoutMs,
      signal: this.inputLoopAbortController.signal,
    }).finally(() => {
      if (this.inputLoopPromise === promise) {
        this.inputLoopPromise = null;
        this.inputLoopAbortController = null;
      }
    });
    this.inputLoopPromise = promise;

    return {
      started: true,
      snapshot: this.snapshot(),
    };
  }

  async waitInputLoop() {
    this.#ensureStarted();
    if (!this.inputLoopPromise) {
      return {
        running: false,
        snapshot: this.snapshot(),
      };
    }
    return this.inputLoopPromise;
  }

  async stopInputLoop() {
    this.#ensureStarted();
    if (!this.inputLoopPromise) {
      return {
        stopped: false,
        snapshot: this.snapshot(),
      };
    }

    const promise = this.inputLoopPromise;
    this.inputLoopAbortController?.abort();
    const result = await promise;
    return {
      stopped: true,
      result,
      snapshot: this.snapshot(),
    };
  }

  async #runInputLoop({
    maxRounds = Infinity,
    firstIdleTimeoutMs = null,
    idleTimeoutMs = null,
    signal = null,
  } = {}) {
    this.#ensureStarted();

    const loop = await runWasmMesaEventLoop({
      queue: this.inboundPackets,
      maxRounds,
      firstIdleTimeoutMs,
      idleTimeoutMs,
      signal,
      onIdle: ({ rounds, reason }) => {
        log(this.onLog, `runtime-loop idle rounds=${rounds} reason=${reason}`);
      },
      onRoundStart: ({ round, packets }) => {
        log(this.onLog, `runtime-loop round=${round} packets=${packets.length}`);
      },
      runBatch: async ({ packets }) => {
        const effects = [];
        for (const event of packets) {
          const result = await this.#commitOvum({
            label: `reply-${this.nextPacketId++}`,
            ovumBytes: mesaHeerOvumJam({
              lane: udpLaneNoun(event.lane),
              packet: event.packet,
            }),
          });
          effects.push({
            label: result.effectsLabel,
            effectsBytes: result.effectsBytes,
          });
        }
        log(this.onLog, `runtime-loop committed event=${this.runtime.event()}`, 'rx');
        return { ok: true, effects };
      },
      routeEffects: async ({ label, effectsBytes }) => {
        return this.#routeEffects(label, effectsBytes);
      },
      onRoundComplete: ({ round, sent, dropped }) => {
        log(this.onLog, `runtime-loop round=${round} sent=${sent} dropped=${dropped}`);
      },
    });

    return {
      loop,
      snapshot: this.snapshot(),
    };
  }

  async save() {
    this.#ensureStarted();
    await this.runtime.save?.();
    return {
      snapshot: this.snapshot(),
    };
  }

  async replay({
    args = null,
    shutdownRuntime = true,
  } = {}) {
    this.#ensureStarted();
    if (this.inputLoopPromise) {
      await this.stopInputLoop();
    }
    await this.runtime.save?.();
    const savedSnapshot = this.snapshot();

    if (shutdownRuntime) {
      await this.shutdown();
    }

    const replay = await this.replayProbe(this.wasmUrl, {
      fileStore: this.fileStore,
      initialFiles: {
        '/brass.pill': this.pillBytes,
      },
      memoryOptions: this.memoryOptions,
      args: args ?? this.#defaultReplayArgs(),
      onStdout: this.onStdout,
      onStderr: this.onStderr,
    });

    log(
      this.onLog,
      `replay exit=${replay.exitCode}`,
      replay.exitCode === 0 ? 'rx' : 'err',
    );

    return {
      replay: {
        exitCode: replay.exitCode,
        initialBytes: replay.plan?.initialBytes,
        maximumBytes: replay.plan?.maximumBytes,
      },
      snapshot: savedSnapshot,
    };
  }

  async shutdown() {
    if (this.closed) {
      return {
        snapshot: this.snapshot(),
      };
    }

    try {
      if (this.inputLoopPromise) {
        this.inputLoopAbortController?.abort();
        try {
          await this.inputLoopPromise;
        }
        catch (_) {}
      }
      this.httpClientHost?.abortAll?.();
      this.behnHost?.abortAll?.();
      this.httpServerHost?.cancelAll?.();
      await this.client?.close?.({ reason: 'browser runtime service shutdown' });
    }
    finally {
      this.client = null;
      this.inboundPackets.close();
      if (this.runtime) {
        this.runtime.shutdown();
      }
      this.closed = true;
    }

    return {
      snapshot: this.snapshot(),
    };
  }

  async #pokeOvum({ label, ovumBytes }) {
    const result = await this.#commitOvum({ label, ovumBytes });
    const routes = await this.#routeEffects(result.effectsLabel, result.effectsBytes);
    return {
      label,
      event: this.runtime.event(),
      effects: routes.summary,
      routes: publicRouteCount(routes),
      snapshot: this.snapshot(),
    };
  }

  async #commitOvum({ label, ovumBytes }) {
    return this.#withRuntimeCommitLock(
      () => this.#commitOvumUnlocked({ label, ovumBytes }),
      commitPriority(label),
    );
  }

  async #withRuntimeCommitLock(fn, priority = 1) {
    if (this.runtimeCommitBusy) {
      await new Promise(resolve => {
        this.runtimeCommitWaiters.push({
          resolve,
          priority,
          order: this.runtimeCommitWaiterOrder++,
        });
      });
    }
    else {
      this.runtimeCommitBusy = true;
    }

    try {
      return await fn();
    }
    finally {
      this.runtimeCommitWaiters.sort((a, b) => (
        (a.priority - b.priority) || (a.order - b.order)
      ));
      const next = this.runtimeCommitWaiters.shift();
      if (next) {
        next.resolve();
      }
      else {
        this.runtimeCommitBusy = false;
      }
    }
  }

  async #commitOvumUnlocked({ label, ovumBytes }) {
    this.#ensureStarted();

    const safe = sanitizeLabel(label);
    const ovumPath = this.#inputPath(safe);
    const effectsPath = this.#effectsPath(safe);

    this.runtime.host.files.set(ovumPath, ovumBytes);
    //  witness large commits even for routine labels: a multi-megabyte
    //  ovum (e.g. a buffered glob body) runs as one synchronous wasm
    //  event, and if it wedges, this is the last line you will see
    if (ovumBytes.length >= 256 * 1024) {
      log(
        this.onLog,
        `committing %${label} ${ovumBytes.length}B (large single event)`,
        'tx',
      );
    }
    this.runtime.pokeOvum({ ovumPath, effectsPath });
    if (this.autoSave) {
      await this.runtime.save?.();
    }
    if (!isRoutineHostLabel(label)) {
      log(this.onLog, `injected %${label} event=${this.runtime.event()}`, 'rx');
    }

    const effectsBytes = this.runtime.host.files.get(effectsPath);
    return {
      effectsLabel: `${label}-effects`,
      effectsBytes,
    };
  }

  async #routeEffects(label, effectsBytes) {
    if (isLargeHttpServerEffectsLabel(label, effectsBytes, this.largeHttpServerEffectBytes)) {
      return this.#routeLargeHttpServerEffects(label, effectsBytes);
    }

    const effectsNoun = effectsBytes ? decodeEffectList(effectsBytes) : null;
    const effects = effectsNoun
      ? {
        noun: effectsNoun,
        bytes: effectsBytes.length,
      }
      : null;
    const summary = logEffectSummary(this.onLog, label, effects);
    if (!effectsBytes) {
      return { sent: [], dropped: [], bound: [], droppedBinds: [] };
    }
    const routine = isRoutineHostLabel(label);

    const routes = await routeMesaEffects(effectsNoun, {
      sendLane: async (lane, packet) => {
        if (!this.client) {
          throw new Error('WebTransport client is not connected');
        }
        if (!routine) {
          log(
            this.onLog,
            `${label}: route ${lane.type} ${JSON.stringify(lane)} ${packet.length}B`,
          );
        }
        return this.client.sendTo(lane, packet);
      },
      onRoute: routed => {
        if (!routine) {
          log(this.onLog, `${label}: tx ${routed.mode} ${routed.push.packet.length}B`, 'tx');
        }
      },
      onDrop: ({ lane, push }) => {
        log(
          this.onLog,
          `${label}: drop lane=${JSON.stringify(lane, (_, value) => (
            typeof value === 'bigint' ? value.toString() : value
          ))} packet=${push.packet.length}B`,
          'err',
        );
      },
    });

    this.totals.sent += routeCount(routes.sent);
    this.totals.droppedRoutes += routeCount(routes.dropped);
    this.totals.bound += routeCount(routes.bound);
    this.totals.droppedBinds += routeCount(routes.droppedBinds);

    const http = this.httpClientHost
      ? await this.httpClientHost.routeEffects(effectsNoun, {
        injectOvum: ({ label: ovumLabel, ovumBytes }) => this.#pokeOvum({
          label: ovumLabel,
          ovumBytes,
        }),
      })
      : emptyHttpClientRoute();
    this.totals.httpRequests += http.requests;
    this.totals.httpCancels += http.cancels;

    const behn = this.behnHost
      ? this.behnHost.routeEffects(effectsNoun, {
        injectOvum: ({ label: ovumLabel, ovumBytes }) => this.#pokeOvum({
          label: ovumLabel,
          ovumBytes,
        }),
      })
      : emptyBehnRoute();

    const httpServer = this.httpServerHost
      ? this.httpServerHost.routeEffects(effectsNoun)
      : emptyHttpServerRoute();
    this.totals.httpServerResponses += httpServer.responses;

    const terminal = this.terminalHost
      ? this.terminalHost.routeEffects(effectsNoun)
      : emptyTerminalRoute();
    this.totals.terminalEvents += terminal.events;
    this.totals.terminalBlits += terminal.blits;

    return {
      ...routes,
      summary,
      http,
      behn,
      httpServer,
      terminal,
    };
  }

  #routeLargeHttpServerEffects(label, effectsBytes) {
    log(
      this.onLog,
      `${label}=bytes=${effectsBytes.length} routing=http-server-only`,
      'tx',
    );

    const httpServer = this.httpServerHost
      ? this.httpServerHost.routeEffects(effectsBytes)
      : emptyHttpServerRoute();
    this.totals.httpServerResponses += httpServer.responses;

    log(
      this.onLog,
      `${label}=bytes=${effectsBytes.length} http-server ` +
        `responses=${httpServer.responses} unknown=${httpServer.unknown}`,
      'rx',
    );

    return {
      ...emptyMesaRoutes(),
      summary: {
        bytes: effectsBytes.length,
        sends: 0,
        pushes: 0,
        binds: 0,
        unknown: 0,
      },
      http: emptyHttpClientRoute(),
      behn: emptyBehnRoute(),
      httpServer,
      terminal: emptyTerminalRoute(),
    };
  }

  #inputPath(label) {
    return `/in/runtime-${this.nextFileId++}-${label}.ovum.jam`;
  }

  #effectsPath(label) {
    return `/out/runtime-${this.nextFileId++}-${label}.effects.jam`;
  }

  #defaultReplayArgs() {
    return [
      '--loom',
      String(this.memoryOptions.loomExponent),
      ...(this.bootMode === 'owned'
        ? ['--owned-ship', this.fakeShip.toString()]
        : (this.fakeShip === 0n ? [] : ['--fake-ship', this.fakeShip.toString()])),
      '--load-only',
      '--run-boot',
    ];
  }

  #ensureOpen() {
    if (this.closed) {
      throw new Error('runtime service is closed');
    }
  }

  #ensureStarted() {
    this.#ensureOpen();
    if (!this.started || !this.runtime) {
      throw new Error('runtime service is not started');
    }
  }
}
