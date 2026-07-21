const defaultDelay = ms => new Promise(resolve => setTimeout(resolve, ms));

function packetCopy(packet) {
  return Uint8Array.from(packet);
}

function routeCount(value) {
  if (Array.isArray(value)) {
    return value.length;
  }
  if (Number.isSafeInteger(value) && value >= 0) {
    return value;
  }
  return 0;
}

export class BoundedPacketQueue {
  constructor({
    maxPackets = 32,
    copy = packetCopy,
    onDrop = () => {},
  } = {}) {
    if (!Number.isSafeInteger(maxPackets) || maxPackets <= 0) {
      throw new Error('maxPackets must be a positive safe integer');
    }
    this.maxPackets = maxPackets;
    this.copy = copy;
    this.onDrop = onDrop;
    this.packets = [];
    this.accepted = 0;
    this.dropped = 0;
    this.closed = false;
    this.waiters = new Set();
  }

  push(packet) {
    if (this.closed) {
      this.dropped++;
      this.onDrop({ packet, dropped: this.dropped, reason: 'closed' });
      return false;
    }
    if (this.packets.length >= this.maxPackets) {
      this.dropped++;
      this.onDrop({ packet, dropped: this.dropped, reason: 'overflow' });
      return false;
    }

    this.packets.push(this.copy(packet));
    this.accepted++;
    this.#resolveWaiters('packet');
    return true;
  }

  drainAll() {
    return this.packets.splice(0, this.packets.length);
  }

  get length() {
    return this.packets.length;
  }

  stats() {
    return {
      accepted: this.accepted,
      dropped: this.dropped,
      queued: this.length,
      maxPackets: this.maxPackets,
    };
  }

  close() {
    if (this.closed) {
      return;
    }
    this.closed = true;
    this.#resolveWaiters('closed');
  }

  waitForPacket({
    timeoutMs = null,
    signal = null,
  } = {}) {
    if (this.length > 0) {
      return Promise.resolve('packet');
    }
    if (this.closed) {
      return Promise.resolve('closed');
    }
    if (signal?.aborted) {
      return Promise.resolve('aborted');
    }
    if (
      timeoutMs != null &&
      (!Number.isSafeInteger(timeoutMs) || timeoutMs < 0)
    ) {
      return Promise.reject(new Error('timeoutMs must be a non-negative safe integer'));
    }

    return new Promise(resolve => {
      let timer = null;

      const waiter = reason => {
        if (timer != null) {
          clearTimeout(timer);
        }
        signal?.removeEventListener?.('abort', onAbort);
        this.waiters.delete(waiter);
        resolve(reason);
      };
      const onAbort = () => waiter('aborted');

      if (timeoutMs != null) {
        timer = setTimeout(() => waiter('timeout'), timeoutMs);
      }
      signal?.addEventListener?.('abort', onAbort, { once: true });
      this.waiters.add(waiter);
    });
  }

  #resolveWaiters(reason) {
    const waiters = [...this.waiters];
    this.waiters.clear();
    for (const waiter of waiters) {
      waiter(reason);
    }
  }
}

function validateMaxRounds(maxRounds) {
  if (
    maxRounds !== Infinity &&
    (!Number.isSafeInteger(maxRounds) || maxRounds <= 0)
  ) {
    throw new Error('maxRounds must be a positive safe integer or Infinity');
  }
}

async function processWasmMesaBatch({
  round,
  startIndex,
  batch,
  runBatch,
  routeEffects,
}) {
  const result = await runBatch({ round, startIndex, packets: batch });
  const effects = result?.effects ?? [];
  let roundSent = 0;
  let roundDropped = 0;

  for (const effect of effects) {
    const routes = await routeEffects(effect);
    roundSent += routeCount(routes?.sent);
    roundDropped += routeCount(routes?.dropped);
  }

  return {
    ok: result?.ok !== false,
    sent: roundSent,
    dropped: roundDropped,
  };
}

export async function runQueuedWasmMesaLoop({
  queue,
  maxRounds = 4,
  idleRoundsToStop = 1,
  firstDelayMs = 3000,
  delayMs = 1000,
  delay = defaultDelay,
  runBatch,
  routeEffects = async () => ({ sent: 0, dropped: 0 }),
  onIdle = () => {},
  onRoundStart = () => {},
  onRoundComplete = () => {},
  onComplete = () => {},
} = {}) {
  if (!queue || typeof queue.drainAll !== 'function') {
    throw new Error('queue with drainAll() is required');
  }
  validateMaxRounds(maxRounds);
  if (!Number.isSafeInteger(idleRoundsToStop) || idleRoundsToStop <= 0) {
    throw new Error('idleRoundsToStop must be a positive safe integer');
  }
  if (typeof runBatch !== 'function') {
    throw new Error('runBatch callback is required');
  }

  let packets = 0;
  let rounds = 0;
  let idleRounds = 0;
  let routedPushes = 0;
  let droppedRoutes = 0;
  let stoppedBy = 'round-cap';

  while (rounds < maxRounds) {
    await delay((rounds === 0 && idleRounds === 0) ? firstDelayMs : delayMs);

    const batch = queue.drainAll();
    if (!batch.length) {
      idleRounds++;
      await onIdle({ rounds, packets, idleRounds });
      if (idleRounds >= idleRoundsToStop) {
        stoppedBy = 'idle';
        break;
      }
      continue;
    }

    idleRounds = 0;
    rounds++;
    const round = rounds;
    const startIndex = packets;
    await onRoundStart({ round, startIndex, packets: batch });

    const result = await processWasmMesaBatch({
      round,
      startIndex,
      batch,
      runBatch,
      routeEffects,
    });

    routedPushes += result.sent;
    droppedRoutes += result.dropped;
    packets += batch.length;
    await onRoundComplete({
      round,
      startIndex,
      packets: batch,
      sent: result.sent,
      dropped: result.dropped,
      ok: result.ok,
    });

    if (!result.ok) {
      stoppedBy = 'batch-failed';
      break;
    }
  }

  const stats = queue.stats?.() ?? {
    accepted: undefined,
    dropped: 0,
    queued: queue.length ?? 0,
  };
  const summary = {
    rounds,
    packets,
    routedPushes,
    droppedRoutes,
    droppedInbound: stats.dropped ?? 0,
    queued: stats.queued ?? queue.length ?? 0,
    stoppedBy,
  };
  await onComplete(summary);
  return summary;
}

export async function runWasmMesaEventLoop({
  queue,
  maxRounds = Infinity,
  firstIdleTimeoutMs = null,
  idleTimeoutMs = null,
  signal = null,
  runBatch,
  routeEffects = async () => ({ sent: 0, dropped: 0 }),
  onIdle = () => {},
  onWait = () => {},
  onRoundStart = () => {},
  onRoundComplete = () => {},
  onComplete = () => {},
} = {}) {
  if (
    !queue ||
    typeof queue.drainAll !== 'function' ||
    typeof queue.waitForPacket !== 'function'
  ) {
    throw new Error('queue with drainAll() and waitForPacket() is required');
  }
  validateMaxRounds(maxRounds);
  for (const [name, value] of [
    ['firstIdleTimeoutMs', firstIdleTimeoutMs],
    ['idleTimeoutMs', idleTimeoutMs],
  ]) {
    if (
      value != null &&
      (!Number.isSafeInteger(value) || value < 0)
    ) {
      throw new Error(`${name} must be a non-negative safe integer`);
    }
  }
  if (typeof runBatch !== 'function') {
    throw new Error('runBatch callback is required');
  }

  let packets = 0;
  let rounds = 0;
  let routedPushes = 0;
  let droppedRoutes = 0;
  let stoppedBy = 'round-cap';

  while (rounds < maxRounds) {
    const timeoutMs = rounds === 0 ? firstIdleTimeoutMs : idleTimeoutMs;
    await onWait({ rounds, packets, timeoutMs });
    const waitReason = await queue.waitForPacket({ timeoutMs, signal });

    if (waitReason !== 'packet') {
      stoppedBy = waitReason === 'timeout' ? 'idle' : waitReason;
      await onIdle({ rounds, packets, reason: stoppedBy });
      break;
    }

    if (signal?.aborted) {
      stoppedBy = 'aborted';
      break;
    }

    const batch = queue.drainAll();
    if (!batch.length) {
      continue;
    }

    rounds++;
    const round = rounds;
    const startIndex = packets;
    await onRoundStart({ round, startIndex, packets: batch });

    const result = await processWasmMesaBatch({
      round,
      startIndex,
      batch,
      runBatch,
      routeEffects,
    });

    routedPushes += result.sent;
    droppedRoutes += result.dropped;
    packets += batch.length;
    await onRoundComplete({
      round,
      startIndex,
      packets: batch,
      sent: result.sent,
      dropped: result.dropped,
      ok: result.ok,
    });

    if (!result.ok) {
      stoppedBy = 'batch-failed';
      break;
    }
  }

  const stats = queue.stats?.() ?? {
    accepted: undefined,
    dropped: 0,
    queued: queue.length ?? 0,
  };
  const summary = {
    rounds,
    packets,
    routedPushes,
    droppedRoutes,
    droppedInbound: stats.dropped ?? 0,
    queued: stats.queued ?? queue.length ?? 0,
    stoppedBy,
  };
  await onComplete(summary);
  return summary;
}
