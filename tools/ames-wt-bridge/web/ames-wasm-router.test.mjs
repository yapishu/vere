import assert from 'node:assert/strict';
import test from 'node:test';

import { mesaSessionLane } from './ames-wasm-events.mjs';
import {
  compareMesaBindFreshness,
  describePactLane,
  isPactGalaxyLane,
  MesaSessionRouteTable,
  routeMesaEffects,
} from './ames-wasm-router.mjs';
import { atomFromBytesLE, jamBytes, list, termAtom, tuple } from './urbit-noun.mjs';

function packetAtom(bytes = [1, 2, 3]) {
  return atomFromBytesLE(Uint8Array.from(bytes));
}

function pushEffect(...lanes) {
  return list([
    [termAtom('ames'), 0n],
    tuple(termAtom('give'), tuple(termAtom('push'), list(...lanes), packetAtom())),
  ]);
}

function bindEffect({
  ship = 0x100n,
  rift = 0n,
  bone = 1n,
  sequence = 1n,
  lane = mesaSessionLane(5n),
} = {}) {
  return list([
    [termAtom('ames'), 0n],
    tuple(termAtom('bind'), ship, rift, bone, sequence, lane),
  ]);
}

function routeableEffects(...cards) {
  return {
    sends: [],
    pushes: [],
    binds: [],
    unknown: [],
    ...cards.reduce((out, card) => {
      if (card.type === 'push') out.pushes.push(card);
      if (card.type === 'bind') out.binds.push(card);
      return out;
    }, { pushes: [], binds: [] }),
  };
}

test('describePactLane classifies galaxy, session, and opaque atom lanes', () => {
  assert.equal(isPactGalaxyLane(0n), true);
  assert.equal(isPactGalaxyLane(255n), true);
  assert.equal(isPactGalaxyLane(256n), false);
  assert.deepEqual(describePactLane(0n), { type: 'galaxy', ship: 0n });
  assert.deepEqual(describePactLane(mesaSessionLane(7n)), {
    type: 'session',
    sessionId: 7n,
  });
  assert.deepEqual(describePactLane(0x100n), {
    type: 'opaque',
    lane: 0x100n,
  });
});

test('routeMesaEffects sends galaxy and session lanes through their callbacks', async () => {
  const routed = [];
  const drops = [];
  const out = await routeMesaEffects(jamBytes(pushEffect(0n, mesaSessionLane(5n))), {
    sendGalaxy: async (ship, packet) => {
      routed.push(['galaxy', ship, Array.from(packet)]);
      return 'datagram';
    },
    sendSession: async (sessionId, packet) => {
      routed.push(['session', sessionId, Array.from(packet)]);
      return 'stream';
    },
    onDrop: drop => drops.push(drop),
  });

  assert.deepEqual(routed, [
    ['galaxy', 0n, [1, 2, 3]],
    ['session', 5n, [1, 2, 3]],
  ]);
  assert.equal(out.sent.length, 2);
  assert.equal(out.dropped.length, 0);
  assert.deepEqual(drops, []);
});

test('compareMesaBindFreshness orders rift, bone, then sequence', () => {
  assert.equal(
    compareMesaBindFreshness(
      { rift: 1n, bone: 0n, sequence: 0n },
      { rift: 0n, bone: 99n, sequence: 99n },
    ),
    1,
  );
  assert.equal(
    compareMesaBindFreshness(
      { rift: 1n, bone: 8n, sequence: 1n },
      { rift: 1n, bone: 9n, sequence: 0n },
    ),
    -1,
  );
  assert.equal(
    compareMesaBindFreshness(
      { rift: 1n, bone: 8n, sequence: 1n },
      { rift: 1n, bone: 8n, sequence: 1n },
    ),
    0,
  );
});

test('MesaSessionRouteTable binds only live session lanes with fresh metadata', () => {
  const routes = new MesaSessionRouteTable();
  routes.registerSession(5n, { send: async () => 'datagram' });

  const first = routes.bind({
    type: 'bind',
    ship: 0x100n,
    rift: 0n,
    bone: 7n,
    sequence: 5n,
    lane: mesaSessionLane(5n),
  });
  assert.equal(first.ok, true);
  assert.equal(routes.resolveShip(0x100n).sessionId, 5n);

  const stale = routes.bind({
    type: 'bind',
    ship: 0x100n,
    rift: 0n,
    bone: 7n,
    sequence: 5n,
    lane: mesaSessionLane(5n),
  });
  assert.equal(stale.ok, false);
  assert.equal(stale.reason, 'stale');

  const unknown = routes.bind({
    type: 'bind',
    ship: 0x200n,
    rift: 0n,
    bone: 1n,
    sequence: 1n,
    lane: mesaSessionLane(99n),
  });
  assert.equal(unknown.ok, false);
  assert.equal(unknown.reason, 'unknown-session');

  const nonSession = routes.bind({
    type: 'bind',
    ship: 0x200n,
    rift: 0n,
    bone: 1n,
    sequence: 1n,
    lane: 0n,
  });
  assert.equal(nonSession.ok, false);
  assert.equal(nonSession.reason, 'non-session-lane');

  const fresh = routes.bind({
    type: 'bind',
    ship: 0x100n,
    rift: 0n,
    bone: 7n,
    sequence: 6n,
    lane: mesaSessionLane(5n),
  });
  assert.equal(fresh.ok, true);
  assert.equal(fresh.binding.previous.sessionId, 5n);
});

test('MesaSessionRouteTable unregisters sessions and their ship bindings', () => {
  const routes = new MesaSessionRouteTable();
  routes.registerSession(5n, { send: async () => 'datagram' });
  routes.registerSession(6n, { send: async () => 'datagram' });
  routes.bind({
    type: 'bind',
    ship: 0x100n,
    rift: 0n,
    bone: 1n,
    sequence: 1n,
    lane: mesaSessionLane(5n),
  });
  routes.bind({
    type: 'bind',
    ship: 0x200n,
    rift: 0n,
    bone: 1n,
    sequence: 1n,
    lane: mesaSessionLane(6n),
  });

  const removed = routes.unregisterSession(5n);
  assert.equal(removed.length, 1);
  assert.equal(removed[0].ship, 0x100n);
  assert.equal(routes.resolveShip(0x100n), undefined);
  assert.equal(routes.resolveShip(0x200n).sessionId, 6n);
});

test('routeMesaEffects applies %bind effects and sends session lanes through the table', async () => {
  const routes = new MesaSessionRouteTable();
  const sent = [];
  const bound = [];
  const droppedBinds = [];

  routes.registerSession(5n, {
    send: async (packet, push) => {
      sent.push([Array.from(packet), push.packetAtom]);
      return 'datagram';
    },
  });

  const packet = Uint8Array.from([8, 9]);
  const out = await routeMesaEffects(routeableEffects(
    {
      type: 'bind',
      ship: 0x100n,
      rift: 0n,
      bone: 7n,
      sequence: 5n,
      lane: mesaSessionLane(5n),
    },
    {
      type: 'push',
      lanes: [mesaSessionLane(5n)],
      packet,
      packetAtom: packetAtom([8, 9]),
    },
  ), {
    sessionRoutes: routes,
    onBind: binding => bound.push(binding),
    onDropBind: drop => droppedBinds.push(drop),
  });

  assert.equal(out.sent.length, 1);
  assert.equal(out.dropped.length, 0);
  assert.equal(out.bound.length, 1);
  assert.deepEqual(sent, [[[8, 9], packetAtom([8, 9])]]);
  assert.equal(bound[0].ship, 0x100n);
  assert.deepEqual(droppedBinds, []);
  assert.equal(routes.resolveShip(0x100n).sessionId, 5n);
});

test('routeMesaEffects reports rejected %bind effects', async () => {
  const routes = new MesaSessionRouteTable();
  const droppedBinds = [];

  const out = await routeMesaEffects(jamBytes(bindEffect({
    lane: mesaSessionLane(9n),
  })), {
    sessionRoutes: routes,
    onDropBind: drop => droppedBinds.push(drop),
  });

  assert.equal(out.bound.length, 0);
  assert.equal(out.droppedBinds.length, 1);
  assert.equal(out.droppedBinds[0].reason, 'unknown-session');
  assert.equal(droppedBinds[0].reason, 'unknown-session');
});

test('routeMesaEffects drops session lanes that no longer have a live route', async () => {
  const routes = new MesaSessionRouteTable();
  const drops = [];

  const out = await routeMesaEffects(routeableEffects({
    type: 'push',
    lanes: [mesaSessionLane(5n)],
    packet: Uint8Array.from([1]),
    packetAtom: packetAtom([1]),
  }), {
    sessionRoutes: routes,
    onDrop: drop => drops.push(drop),
  });

  assert.equal(out.sent.length, 0);
  assert.equal(out.dropped.length, 1);
  assert.deepEqual(drops[0].lane, { type: 'session', sessionId: 5n });
});

test('routeMesaEffects drops lanes without a matching route callback', async () => {
  const drops = [];
  const out = await routeMesaEffects(jamBytes(pushEffect(0x100n)), {
    sendGalaxy: async () => 'datagram',
    onDrop: drop => drops.push(drop),
  });

  assert.equal(out.sent.length, 0);
  assert.equal(out.dropped.length, 1);
  assert.deepEqual(drops[0].lane, { type: 'opaque', lane: 0x100n });
});
