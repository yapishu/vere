import {
  atomFromBytesLE,
  bytesFromAtomLE,
  cue,
  termAtom,
} from './urbit-noun.mjs';

import {
  MESA_SESSION_LANE_TAG,
} from './ames-wasm-events.mjs';

const TERMS = Object.freeze({
  ames: termAtom('ames'),
  bind: termAtom('bind'),
  give: termAtom('give'),
  if: termAtom('if'),
  is: termAtom('is'),
  push: termAtom('push'),
  send: termAtom('send'),
});

function isCell(noun) {
  return Array.isArray(noun) && noun.length === 2;
}

function listItems(noun, name) {
  const out = [];
  let cur = noun;
  while (cur !== 0n) {
    if (!isCell(cur)) {
      throw new Error(`${name} must be a Hoon list`);
    }
    out.push(cur[0]);
    cur = cur[1];
  }
  return out;
}

function tupleItems(noun, count) {
  const out = [];
  let cur = noun;
  for (let i = 0; i < count - 1; i++) {
    if (!isCell(cur)) {
      return null;
    }
    out.push(cur[0]);
    cur = cur[1];
  }
  out.push(cur);
  return out;
}

function safeNumber(atom) {
  const value = Number(atom);
  return Number.isSafeInteger(value) ? value : atom;
}

function decodeLane(noun) {
  if (typeof noun === 'bigint') {
    return noun;
  }

  if (isCell(noun) && noun[0] === 0n) {
    return {
      type: 'ames-ship',
      ship: noun[1],
      noun,
    };
  }
  if (isCell(noun) && noun[0] === 1n) {
    return {
      type: 'ames-address',
      address: noun[1],
      noun,
    };
  }

  const tuple = tupleItems(noun, 3);
  if (tuple?.[0] === TERMS.if) {
    return {
      type: 'if',
      ip: safeNumber(tuple[1]),
      port: safeNumber(tuple[2]),
      noun,
    };
  }
  if (tuple?.[0] === TERMS.is) {
    return {
      type: 'is',
      ip: tuple[1],
      port: safeNumber(tuple[2]),
      noun,
    };
  }

  return { type: 'noun', noun };
}

function isAmesWire(noun) {
  const wire = isCell(noun) && noun[0] === 0n ? noun[1] : noun;
  return isCell(wire) && wire[0] === TERMS.ames;
}

function parseMesaCard(card) {
  const send = tupleItems(card, 3);
  if (send?.[0] === TERMS.send) {
    return {
      type: 'send',
      lane: decodeLane(send[1]),
      packet: bytesFromAtomLE(send[2]),
      packetAtom: send[2],
      card,
    };
  }

  const push = tupleItems(card, 3);
  if (push?.[0] === TERMS.push) {
    return {
      type: 'push',
      lanes: listItems(push[1], '%push lanes').map(decodeLane),
      packet: bytesFromAtomLE(push[2]),
      packetAtom: push[2],
      card,
    };
  }

  const bind = tupleItems(card, 6);
  if (bind?.[0] === TERMS.bind) {
    return {
      type: 'bind',
      ship: bind[1],
      rift: bind[2],
      bone: bind[3],
      sequence: bind[4],
      lane: decodeLane(bind[5]),
      card,
    };
  }

  return null;
}

function bucketForType(type) {
  if (type === 'send') return 'sends';
  if (type === 'push') return 'pushes';
  if (type === 'bind') return 'binds';
  throw new Error(`unknown Mesa effect type ${type}`);
}

function giveInner(card) {
  const give = tupleItems(card, 2);
  if (give?.[0] === TERMS.give) {
    return give[1];
  }
  return null;
}

function decodeEffectsInput(input) {
  if (input instanceof Uint8Array) {
    return cue(atomFromBytesLE(input));
  }
  if (input instanceof ArrayBuffer) {
    return cue(atomFromBytesLE(new Uint8Array(input)));
  }
  if (ArrayBuffer.isView(input)) {
    return cue(atomFromBytesLE(
      new Uint8Array(input.buffer, input.byteOffset, input.byteLength),
    ));
  }
  return input;
}

export function decodeEffectList(input) {
  return decodeEffectsInput(input);
}

export function isMesaSessionLane(lane) {
  return typeof lane === 'bigint' && (lane & MESA_SESSION_LANE_TAG) !== 0n;
}

export function mesaSessionIdFromLane(lane) {
  if (!isMesaSessionLane(lane)) {
    throw new Error('lane is not a Mesa session lane');
  }
  return lane & (MESA_SESSION_LANE_TAG - 1n);
}

export function extractMesaEffects(input) {
  const effects = decodeEffectsInput(input);
  const out = {
    sends: [],
    pushes: [],
    binds: [],
    unknown: [],
  };

  for (const effect of listItems(effects, 'effects')) {
    if (!isCell(effect)) {
      out.unknown.push({ effect, reason: 'effect is not a cell' });
      continue;
    }

    const wire = effect[0];
    const card = effect[1];
    const lowered = parseMesaCard(card);
    if (lowered && isAmesWire(wire)) {
      out[bucketForType(lowered.type)].push({
        ...lowered,
        wire,
        raw: effect,
      });
      continue;
    }

    const inner = giveInner(card);
    const raw = inner ? parseMesaCard(inner) : null;
    if (raw) {
      out[bucketForType(raw.type)].push({
        ...raw,
        duct: wire,
        raw: effect,
      });
      continue;
    }

    out.unknown.push({ effect, reason: 'not a Mesa card' });
  }

  return out;
}
