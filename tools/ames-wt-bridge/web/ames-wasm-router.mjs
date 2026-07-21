import {
  extractMesaEffects,
  isMesaSessionLane,
  mesaSessionIdFromLane,
} from './ames-wasm-effects.mjs';

export function isPactGalaxyLane(lane) {
  return typeof lane === 'bigint' && lane >= 0n && lane < 256n;
}

export function describePactLane(lane) {
  if (isPactGalaxyLane(lane)) {
    return { type: 'galaxy', ship: lane };
  }
  if (isMesaSessionLane(lane)) {
    return { type: 'session', sessionId: mesaSessionIdFromLane(lane) };
  }
  if (typeof lane === 'bigint') {
    return { type: 'opaque', lane };
  }
  return lane;
}

function keyOf(value) {
  return BigInt(value).toString();
}

function freshnessOf(bind) {
  return {
    rift: BigInt(bind.rift),
    bone: BigInt(bind.bone),
    sequence: BigInt(bind.sequence),
  };
}

export function compareMesaBindFreshness(a, b) {
  const left = freshnessOf(a);
  const right = freshnessOf(b);

  for (const field of ['rift', 'bone', 'sequence']) {
    if (left[field] > right[field]) return 1;
    if (left[field] < right[field]) return -1;
  }
  return 0;
}

export class MesaSessionRouteTable {
  constructor() {
    this.sessions = new Map();
    this.bindings = new Map();
  }

  registerSession(sessionId, route) {
    const sid = BigInt(sessionId);
    if (!route || typeof route.send !== 'function') {
      throw new Error('session route requires a send(packet, push) function');
    }
    this.sessions.set(keyOf(sid), { sessionId: sid, route });
    return sid;
  }

  unregisterSession(sessionId) {
    const sid = BigInt(sessionId);
    const key = keyOf(sid);
    const removed = [];
    this.sessions.delete(key);

    for (const [shipKey, binding] of this.bindings) {
      if (binding.sessionId === sid) {
        this.bindings.delete(shipKey);
        removed.push(binding);
      }
    }

    return removed;
  }

  resolveSession(sessionId) {
    return this.sessions.get(keyOf(sessionId))?.route;
  }

  resolveShip(ship) {
    return this.bindings.get(keyOf(ship));
  }

  bind(bind) {
    const lane = describePactLane(bind.lane);
    if (lane.type !== 'session') {
      return { ok: false, reason: 'non-session-lane', bind, lane };
    }

    const session = this.sessions.get(keyOf(lane.sessionId));
    if (!session) {
      return { ok: false, reason: 'unknown-session', bind, lane };
    }

    const ship = BigInt(bind.ship);
    const existing = this.bindings.get(keyOf(ship));
    if (existing && compareMesaBindFreshness(bind, existing) <= 0) {
      return {
        ok: false,
        reason: 'stale',
        bind,
        lane,
        existing,
      };
    }

    const binding = {
      ship,
      rift: BigInt(bind.rift),
      bone: BigInt(bind.bone),
      sequence: BigInt(bind.sequence),
      lane,
      sessionId: lane.sessionId,
      route: session.route,
      bind,
      previous: existing ?? null,
    };
    this.bindings.set(keyOf(ship), binding);
    return { ok: true, binding, bind, lane };
  }

  applyEffects(input) {
    const effects = input?.binds ? input : extractMesaEffects(input);
    const bound = [];
    const dropped = [];

    for (const bind of effects.binds) {
      const result = this.bind(bind);
      if (result.ok) {
        bound.push(result.binding);
      } else {
        dropped.push(result);
      }
    }

    return { bound, dropped };
  }

  async sendSession(sessionId, packet, push) {
    const route = this.resolveSession(sessionId);
    if (!route) {
      throw new Error(`session ${sessionId.toString()} is not live`);
    }
    return route.send(packet, push);
  }
}

export async function routeMesaEffects(input, {
  sessionRoutes,
  sendGalaxy,
  sendSession,
  onRoute = () => {},
  onDrop = () => {},
  onBind = () => {},
  onDropBind = () => {},
} = {}) {
  const effects = input?.pushes ? input : extractMesaEffects(input);
  const bindResults = sessionRoutes
    ? sessionRoutes.applyEffects(effects)
    : { bound: [], dropped: [] };
  for (const binding of bindResults.bound) {
    onBind(binding);
  }
  for (const droppedBind of bindResults.dropped) {
    onDropBind(droppedBind);
  }

  const sent = [];
  const dropped = [];

  for (const push of effects.pushes) {
    for (const lane of push.lanes) {
      const described = describePactLane(lane);
      if (described.type === 'galaxy' && typeof sendGalaxy === 'function') {
        const mode = await sendGalaxy(described.ship, push.packet, push);
        const routed = { lane: described, mode, push };
        sent.push(routed);
        onRoute(routed);
        continue;
      }

      if (described.type === 'session') {
        const route = sessionRoutes?.resolveSession(described.sessionId);
        if (!route && (sessionRoutes || typeof sendSession !== 'function')) {
          const drop = { lane: described, push };
          dropped.push(drop);
          onDrop(drop);
          continue;
        }

        const mode = typeof sendSession === 'function'
          ? await sendSession(described.sessionId, push.packet, push)
          : await route.send(push.packet, push);
        const routed = { lane: described, mode, push };
        sent.push(routed);
        onRoute(routed);
        continue;
      }

      const drop = { lane: described, push };
      dropped.push(drop);
      onDrop(drop);
    }
  }

  return {
    sent,
    dropped,
    bound: bindResults.bound,
    droppedBinds: bindResults.dropped,
  };
}
