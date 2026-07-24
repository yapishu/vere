import {
  atomFromBytesLE,
  cell,
  cue,
  jamBytes,
  termAtom,
  tuple,
} from './urbit-noun.mjs';

const URBIT_UNIX_EPOCH_SECONDS = 0x8000000cce9e0d80n;
const URBIT_FRACTION_SHIFT = 48n;
const URBIT_FRACTION_SCALE = 65536n;
const URBIT_LOW_MASK = (1n << 64n) - 1n;
const MAX_TIMEOUT_MS = 0x7fffffff;

const TERMS = Object.freeze({
  behn: termAtom('behn'),
  born: termAtom('born'),
  doze: termAtom('doze'),
  give: termAtom('give'),
  wake: termAtom('wake'),
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

function runtimeOvum({ kind = 'e', wire, card }) {
  if (!wire) {
    throw new Error('wire is required');
  }
  return cell(cell(termAtom(kind), wire), card);
}

function giveInner(card) {
  const give = tupleItems(card, 2);
  if (give?.[0] === TERMS.give) {
    return give[1];
  }
  return null;
}

function decodeUnitDate(unit) {
  if (unit === 0n) {
    return null;
  }
  if (!isCell(unit) || unit[0] !== 0n || typeof unit[1] !== 'bigint') {
    throw new Error('%behn %doze date must be a unit @da');
  }
  return unit[1];
}

function decodeDoze(card) {
  const direct = tupleItems(card, 2);
  if (direct?.[0] === TERMS.doze) {
    return {
      type: 'doze',
      date: decodeUnitDate(direct[1]),
      card,
    };
  }

  const inner = giveInner(card);
  if (!inner) {
    return null;
  }
  const given = tupleItems(inner, 2);
  if (given?.[0] === TERMS.doze) {
    return {
      type: 'doze',
      date: decodeUnitDate(given[1]),
      card: inner,
    };
  }
  return null;
}

export function behnWire() {
  return cell(TERMS.behn, 0n);
}

export function behnBornOvumJam({
  kind = 'b',
} = {}) {
  return jamBytes(runtimeOvum({
    kind,
    wire: behnWire(),
    card: cell(TERMS.born, 0n),
  }));
}

export function behnWakeOvumJam({
  kind = 'b',
} = {}) {
  return jamBytes(runtimeOvum({
    kind,
    wire: behnWire(),
    card: cell(TERMS.wake, 0n),
  }));
}

export function urbitDateFromUnixMs(ms) {
  if (!Number.isSafeInteger(ms) || ms < 0) {
    throw new Error('unix ms must be a non-negative safe integer');
  }
  const value = BigInt(ms);
  const seconds = value / 1000n;
  const millis = value % 1000n;
  const high = URBIT_UNIX_EPOCH_SECONDS + seconds;
  const low = ((millis * URBIT_FRACTION_SCALE) / 1000n) << URBIT_FRACTION_SHIFT;
  return (high << 64n) | low;
}

export function urbitDateToUnixMs(date) {
  const value = BigInt(date);
  const seconds = (value >> 64n) - URBIT_UNIX_EPOCH_SECONDS;
  const low = value & URBIT_LOW_MASK;
  const millis = ((low >> URBIT_FRACTION_SHIFT) * 1000n) / URBIT_FRACTION_SCALE;
  return Number((seconds * 1000n) + millis);
}

export function extractBehnEffects(input) {
  const effects = decodeEffectsInput(input);
  const out = {
    dozes: [],
    unknown: [],
  };

  for (const effect of listItems(effects, 'effects')) {
    if (!isCell(effect)) {
      out.unknown.push({ effect, reason: 'effect is not a cell' });
      continue;
    }

    const wire = effect[0];
    const card = effect[1];
    const doze = decodeDoze(card);
    if (doze) {
      out.dozes.push({
        ...doze,
        wire,
        raw: effect,
      });
      continue;
    }

    out.unknown.push({ effect, reason: 'not a Behn card' });
  }

  return out;
}

export class BrowserBehnTimerHost {
  constructor({
    onLog = () => {},
    clock = () => Date.now(),
    setTimer = (fn, delay) => setTimeout(fn, delay),
    clearTimer = timer => clearTimeout(timer),
  } = {}) {
    this.onLog = onLog;
    this.clock = clock;
    this.setTimer = setTimer;
    this.clearTimer = clearTimer;
    this.timer = null;
    this.nextWake = null;
  }

  snapshot() {
    return {
      nextWake: this.nextWake?.toString?.() ?? null,
      active: this.timer !== null,
    };
  }

  routeEffects(input, {
    injectOvum,
  } = {}) {
    if (typeof injectOvum !== 'function') {
      throw new Error('injectOvum callback is required');
    }

    const effects = extractBehnEffects(input);
    for (const doze of effects.dozes) {
      this.#schedule(doze.date, injectOvum);
    }

    return {
      dozes: effects.dozes.length,
      unknown: effects.unknown.length,
      active: this.timer !== null,
    };
  }

  abortAll() {
    if (this.timer !== null) {
      this.clearTimer(this.timer);
      this.timer = null;
    }
    this.nextWake = null;
  }

  #schedule(date, injectOvum) {
    if (this.timer !== null) {
      this.clearTimer(this.timer);
      this.timer = null;
    }
    this.nextWake = date;

    if (date === null) {
      this.onLog({
        message: 'behn timer cleared',
        className: '',
      });
      return;
    }

    const wakeMs = urbitDateToUnixMs(date);
    const delay = Math.max(0, Math.min(MAX_TIMEOUT_MS, wakeMs - this.clock()));
    this.onLog({
      message: `behn timer scheduled delay=${delay}ms`,
      className: 'rx',
    });
    this.timer = this.setTimer(() => {
      this.timer = null;
      this.nextWake = null;
      injectOvum({
        label: 'behn-wake',
        ovumBytes: behnWakeOvumJam(),
      }).catch(error => {
        this.onLog({
          message: `behn wake failed: ${error}`,
          className: 'err',
        });
      });
    }, delay);
  }
}
