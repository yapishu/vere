import { AmesWebTransportClient, webTransportOptions } from './ames-client.mjs';
import {
  bornOvumJam,
  keenOvumJam,
  mateOvumJam,
  mesaHeerOvumJam,
  mesaSessionLane,
} from './ames-wasm-events.mjs';
import { extractMesaEffects } from './ames-wasm-effects.mjs';
import {
  MesaSessionRouteTable,
  routeMesaEffects,
} from './ames-wasm-router.mjs';
import {
  BoundedPacketQueue,
  runWasmMesaEventLoop,
} from './ames-wasm-runtime-loop.mjs';
import {
  BrowserHttpClientHost,
  httpClientBornOvumJam,
} from './ames-wasm-http-client.mjs';
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

function packetCopy(packet) {
  return packet instanceof Uint8Array ? Uint8Array.from(packet) : Uint8Array.from(packet);
}

function routeCount(value) {
  return Array.isArray(value) ? value.length : 0;
}

function log(onLog, message, className = '') {
  onLog({ message, className });
}

function effectSummary(bytes) {
  if (!bytes) {
    return {
      bytes: 0,
      sends: 0,
      pushes: 0,
      binds: 0,
      unknown: 0,
    };
  }

  const effects = extractMesaEffects(bytes);
  return {
    bytes: bytes.length,
    sends: effects.sends.length,
    pushes: effects.pushes.length,
    binds: effects.binds.length,
    unknown: effects.unknown.length,
  };
}

function logEffectSummary(onLog, label, bytes) {
  const summary = effectSummary(bytes);
  log(
    onLog,
    `${label}=bytes=${summary.bytes} sends=${summary.sends} ` +
      `pushes=${summary.pushes} binds=${summary.binds} unknown=${summary.unknown}`,
  );
  return summary;
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
    sessionId = 1n,
    maxPackets = 32,
    runtimeFactory = defaultRuntimeFactory,
    replayProbe = defaultReplayProbe,
    clientFactory = defaultClientFactory,
    httpClientHost = null,
    httpClientHostFactory = defaultHttpClientHostFactory,
    httpServerHost = null,
    httpServerHostFactory = defaultHttpServerHostFactory,
    terminalHost = null,
    terminalHostFactory = defaultTerminalHostFactory,
    onLog = () => {},
    onStdout = () => {},
    onStderr = () => {},
    onPacket = () => {},
    onTerminal = () => {},
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
    this.sessionId = BigInt(sessionId);
    this.runtimeFactory = runtimeFactory;
    this.replayProbe = replayProbe;
    this.clientFactory = clientFactory;
    this.onLog = onLog;
    this.onStdout = onStdout;
    this.onStderr = onStderr;
    this.onPacket = onPacket;
    this.onTerminal = onTerminal;
    this.httpClientHost = httpClientHost ?? httpClientHostFactory({
      onLog: event => log(this.onLog, event.message, event.className),
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
    this.closed = false;
    this.inputLoopAbortController = null;
    this.inputLoopPromise = null;
    this.nextFileId = 0;
    this.nextPacketId = 0;
    this.sessionRoutes = new MesaSessionRouteTable();
    this.inboundPackets = new BoundedPacketQueue({
      maxPackets,
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
      },
      memoryOptions: this.memoryOptions,
      onStdout: this.onStdout,
      onStderr: this.onStderr,
    });

    this.runtime.init({
      loomExponent: this.memoryOptions.loomExponent,
      ship: this.fakeShip,
    });
    this.started = true;
    log(this.onLog, `runtime initialized event=${this.runtime.event()}`, 'rx');

    const loadPath = this.#effectsPath('load-mesa');
    this.runtime.pokeLoadMesa({ effectsPath: loadPath });
    log(this.onLog, `loaded Mesa event=${this.runtime.event()}`, 'rx');
    const load = await this.#routeEffects(
      'load-effects',
      this.runtime.host.files.get(loadPath),
    );
    const httpBorn = this.httpClientHost
      ? await this.#pokeOvum({
        label: 'http-client-born',
        ovumBytes: httpClientBornOvumJam(),
      })
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

    return {
      load,
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
          this.inboundPackets.push(event.packet);
          this.totals.inboundPackets++;
          this.onPacket(event);
          log(this.onLog, `rx ${event.mode} ${event.packet.length}B`, 'rx');
        },
        onError: ({ type, error }) => {
          log(this.onLog, `${type}: ${error}`, 'err');
        },
      });
      this.sessionRoutes.registerSession(this.sessionId, {
        send: packet => this.client.send(packet),
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
    this.sessionRoutes.unregisterSession(this.sessionId);
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
    lane = mesaSessionLane(this.sessionId),
    label = `reply-${this.nextPacketId++}`,
  } = {}) {
    if (packet == null) {
      throw new Error('packet is required');
    }
    return this.#pokeOvum({
      label,
      ovumBytes: mesaHeerOvumJam({
        lane: BigInt(lane),
        packet: packetCopy(packet),
      }),
    });
  }

  async httpRequest({
    method = 'GET',
    url = '/',
    headers = [],
    body = null,
    secure = false,
    local = true,
    timeoutMs = undefined,
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
        for (const packet of packets) {
          const result = await this.#commitOvum({
            label: `reply-${this.nextPacketId++}`,
            ovumBytes: mesaHeerOvumJam({
              lane: mesaSessionLane(this.sessionId),
              packet: packetCopy(packet),
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
      effects: result.summary,
      routes: publicRouteCount(routes),
      snapshot: this.snapshot(),
    };
  }

  async #commitOvum({ label, ovumBytes }) {
    this.#ensureStarted();

    const safe = sanitizeLabel(label);
    const ovumPath = this.#inputPath(safe);
    const effectsPath = this.#effectsPath(safe);

    this.runtime.host.files.set(ovumPath, ovumBytes);
    this.runtime.pokeOvum({ ovumPath, effectsPath });
    log(this.onLog, `injected %${label} event=${this.runtime.event()}`, 'rx');

    const effectsBytes = this.runtime.host.files.get(effectsPath);
    return {
      effectsLabel: `${label}-effects`,
      effectsBytes,
      summary: effectSummary(effectsBytes),
    };
  }

  async #routeEffects(label, effectsBytes) {
    const summary = logEffectSummary(this.onLog, label, effectsBytes);
    if (!effectsBytes) {
      return { sent: [], dropped: [], bound: [], droppedBinds: [] };
    }

    const routes = await routeMesaEffects(effectsBytes, {
      sessionRoutes: this.sessionRoutes,
      sendGalaxy: async (ship, packet) => {
        if (!this.client) {
          throw new Error('WebTransport client is not connected');
        }
        log(this.onLog, `${label}: route galaxy ${ship.toString()} ${packet.length}B`);
        return this.client.send(packet);
      },
      onBind: binding => {
        log(
          this.onLog,
          `${label}: bind ${binding.ship.toString()} -> session ` +
            `${binding.sessionId.toString()}`,
          'rx',
        );
      },
      onDropBind: ({ reason, bind }) => {
        log(this.onLog, `${label}: drop bind ${bind.ship.toString()} reason=${reason}`, 'err');
      },
      onRoute: routed => {
        log(this.onLog, `${label}: tx ${routed.mode} ${routed.push.packet.length}B`, 'tx');
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
      ? await this.httpClientHost.routeEffects(effectsBytes, {
        injectOvum: ({ label: ovumLabel, ovumBytes }) => this.#pokeOvum({
          label: ovumLabel,
          ovumBytes,
        }),
      })
      : {
        requests: 0,
        cancels: 0,
        unknown: 0,
        pending: 0,
      };
    this.totals.httpRequests += http.requests;
    this.totals.httpCancels += http.cancels;

    const httpServer = this.httpServerHost
      ? this.httpServerHost.routeEffects(effectsBytes)
      : {
        responses: 0,
        configs: 0,
        sessions: 0,
        grows: 0,
        unknown: 0,
        pending: 0,
      };
    this.totals.httpServerResponses += httpServer.responses;

    const terminal = this.terminalHost
      ? this.terminalHost.routeEffects(effectsBytes)
      : {
        events: 0,
        blits: 0,
        logos: 0,
        unknown: 0,
        bufferChars: 0,
      };
    this.totals.terminalEvents += terminal.events;
    this.totals.terminalBlits += terminal.blits;

    return {
      ...routes,
      summary,
      http,
      httpServer,
      terminal,
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
      ...(this.fakeShip === 0n ? [] : ['--fake-ship', this.fakeShip.toString()]),
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
