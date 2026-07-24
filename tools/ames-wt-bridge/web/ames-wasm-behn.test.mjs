import assert from 'node:assert/strict';
import test from 'node:test';

import {
  BrowserBehnTimerHost,
  behnBornOvumJam,
  behnWakeOvumJam,
  behnWire,
  extractBehnEffects,
  urbitDateFromUnixMs,
  urbitDateToUnixMs,
} from './ames-wasm-behn.mjs';
import {
  atomFromBytesLE,
  cue,
  jamBytes,
  list,
  termAtom,
  tuple,
} from './urbit-noun.mjs';

function tupleItems(noun, count) {
  const out = [];
  let cur = noun;
  for (let i = 0; i < count - 1; i++) {
    if (!Array.isArray(cur)) {
      return null;
    }
    out.push(cur[0]);
    cur = cur[1];
  }
  out.push(cur);
  return out;
}

function dozeEffects(date, wire = behnWire()) {
  return jamBytes(list(
    tuple(
      wire,
      tuple(
        termAtom('give'),
        tuple(termAtom('doze'), date == null ? 0n : [0n, date]),
      ),
    ),
  ));
}

test('behn ova build the runtime timer wire', () => {
  assert.ok(behnBornOvumJam().length > 0);
  assert.ok(behnWakeOvumJam().length > 0);

  const born = cue(atomFromBytesLE(behnBornOvumJam()));
  const [head, card] = tupleItems(born, 2);
  const [kind, wire] = tupleItems(head, 2);
  assert.equal(kind, termAtom('b'));
  assert.equal(wire[0], termAtom('behn'));
  assert.equal(card[0], termAtom('born'));
});

test('urbit date conversion round-trips browser millisecond time', () => {
  const ms = 1_784_683_200_123;
  assert.ok(Math.abs(urbitDateToUnixMs(urbitDateFromUnixMs(ms)) - ms) <= 1);
});

test('extractBehnEffects decodes %doze gifts', () => {
  const date = urbitDateFromUnixMs(1_784_683_200_000);
  const effects = extractBehnEffects(dozeEffects(date));
  assert.equal(effects.dozes.length, 1);
  assert.equal(effects.dozes[0].date, date);
});

test('extractBehnEffects treats %doze as Behn even on opaque ducts', () => {
  const date = urbitDateFromUnixMs(1_784_683_200_000);
  const runtimeDuct = list(termAtom('runtime'), termAtom('behn'));
  const effects = extractBehnEffects(dozeEffects(date, runtimeDuct));
  assert.equal(effects.dozes.length, 1);
  assert.equal(effects.dozes[0].date, date);
});

test('BrowserBehnTimerHost schedules and injects wake ova', async () => {
  const timers = [];
  const injected = [];
  const host = new BrowserBehnTimerHost({
    clock: () => 1_000,
    setTimer: (fn, delay) => {
      timers.push({ fn, delay });
      return timers.length;
    },
    clearTimer: () => {},
  });

  const routed = host.routeEffects(dozeEffects(urbitDateFromUnixMs(1_250)), {
    injectOvum: async value => {
      injected.push(value);
    },
  });

  assert.equal(routed.dozes, 1);
  assert.equal(routed.active, true);
  assert.equal(timers[0].delay, 250);
  await timers[0].fn();
  assert.equal(injected.length, 1);
  assert.equal(injected[0].label, 'behn-wake');
  assert.ok(injected[0].ovumBytes.length > 0);
});
