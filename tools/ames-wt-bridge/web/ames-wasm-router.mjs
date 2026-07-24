import { extractMesaEffects } from './ames-wasm-effects.mjs';
import { normalizeUdpLane } from './ames-udp-frame.mjs';

export function isPactGalaxyLane(lane) {
  return typeof lane === 'bigint' && lane >= 0n && lane < 256n;
}

export function describePactLane(lane) {
  if (isPactGalaxyLane(lane)) {
    return normalizeUdpLane(lane);
  }
  if (lane?.type === 'if') {
    return normalizeUdpLane(lane);
  }
  if (typeof lane === 'bigint') {
    return { type: 'unsupported', lane };
  }
  return { type: 'unsupported', lane };
}

export async function routeMesaEffects(input, {
  sendLane,
  sendGalaxy,
  onRoute = () => {},
  onDrop = () => {},
} = {}) {
  const effects = input?.pushes ? input : extractMesaEffects(input);
  const sent = [];
  const dropped = [];

  for (const push of effects.pushes) {
    for (const lane of push.lanes) {
      const described = describePactLane(lane);
      if (
        (described.type === 'galaxy' || described.type === 'if') &&
        typeof sendLane === 'function'
      ) {
        const mode = await sendLane(described, push.packet, push);
        const routed = { lane: described, mode, push };
        sent.push(routed);
        onRoute(routed);
        continue;
      }
      if (described.type === 'galaxy' && typeof sendGalaxy === 'function') {
        const mode = await sendGalaxy(described.ship, push.packet, push);
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
    bound: [],
    droppedBinds: effects.binds.map(bind => ({
      reason: 'session-bind-unsupported',
      bind,
    })),
  };
}
