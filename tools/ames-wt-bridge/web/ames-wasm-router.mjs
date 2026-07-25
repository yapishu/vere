import { extractMesaEffects } from './ames-wasm-effects.mjs';
import { normalizeUdpLane } from './ames-udp-frame.mjs';

export function isPactGalaxyLane(lane) {
  return typeof lane === 'bigint' && lane >= 0n && lane < 256n;
}

export function describePactLane(lane) {
  if (isPactGalaxyLane(lane)) {
    return normalizeUdpLane(lane);
  }
  if (lane?.type === 'ames-ship' && isPactGalaxyLane(lane.ship)) {
    return normalizeUdpLane(lane.ship);
  }
  if (lane?.type === 'ames-address') {
    const address = BigInt(lane.address);
    if (address >= 0n && address < (1n << 48n)) {
      return normalizeUdpLane({
        type: 'if',
        ip: Number(address & 0xffff_ffffn),
        port: Number((address >> 32n) & 0xffffn),
      });
    }
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

  const transmissions = [
    ...effects.sends.map(send => ({ effect: send, lanes: [send.lane] })),
    ...effects.pushes.map(push => ({ effect: push, lanes: push.lanes })),
  ];
  for (const { effect, lanes } of transmissions) {
    for (const lane of lanes) {
      const described = describePactLane(lane);
      if (
        (described.type === 'galaxy' || described.type === 'if') &&
        typeof sendLane === 'function'
      ) {
        const mode = await sendLane(described, effect.packet, effect);
        const routed = { lane: described, mode, push: effect };
        sent.push(routed);
        onRoute(routed);
        continue;
      }
      if (described.type === 'galaxy' && typeof sendGalaxy === 'function') {
        const mode = await sendGalaxy(described.ship, effect.packet, effect);
        const routed = { lane: described, mode, push: effect };
        sent.push(routed);
        onRoute(routed);
        continue;
      }

      const drop = { lane: described, push: effect };
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
