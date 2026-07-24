import assert from 'node:assert/strict';
import test from 'node:test';

import {
  BrowserTerminalHost,
  extractTerminalEffects,
  termWire,
  terminalBlewOvumJam,
  terminalBornOvumJam,
  terminalDataOvumJams,
  terminalHailOvumJam,
  terminalRetOvumJam,
  terminalTextOvumJam,
} from './ames-wasm-terminal.mjs';
import {
  atomFromBytesLE,
  cue,
  jamBytes,
  list,
  termAtom,
  tuple,
} from './urbit-noun.mjs';

function chars(text) {
  return list(...[...text].map(char => BigInt(char.codePointAt(0))));
}

function styled(text) {
  return list([tuple(0n, 0n, 0n), chars(text)]);
}

function terminalEffects() {
  return jamBytes(list(
    tuple(
      termWire(),
      tuple(
        termAtom('give'),
        tuple(
          termAtom('blit'),
          list(
            tuple(termAtom('clr'), 0n),
            tuple(termAtom('put'), chars('dojo> ')),
            tuple(termAtom('klr'), styled('ready')),
            tuple(termAtom('nel'), 0n),
          ),
        ),
      ),
    ),
  ));
}

test('terminal ova build cueable native %d /term/1 events', () => {
  for (const ovum of [
    terminalBornOvumJam(),
    terminalBlewOvumJam({ cols: 100, rows: 30 }),
    terminalHailOvumJam(),
    terminalTextOvumJam({ text: '+trouble' }),
    terminalRetOvumJam(),
  ]) {
    assert.ok(cue(atomFromBytesLE(ovum)));
  }
});

test('terminal data maps common tty input to Dill belts', () => {
  const ovums = terminalDataOvumJams({
    data: 'ab\r\x7f\x1b[A\x1b[3~\x03',
  });
  const belts = ovums.map(ovum => cue(atomFromBytesLE(ovum))[1][1]);

  assert.deepEqual(belts.map(belt => belt[0]), [
    termAtom('txt'),
    termAtom('ret'),
    termAtom('bac'),
    termAtom('aro'),
    termAtom('del'),
    termAtom('mod'),
  ]);
  assert.equal(belts[3][1], termAtom('u'));
  assert.deepEqual(belts[5], tuple(termAtom('mod'), termAtom('ctl'), BigInt('c'.codePointAt(0))));
});

test('extractTerminalEffects decodes terminal blits into render events', () => {
  const effects = extractTerminalEffects(terminalEffects());

  assert.equal(effects.blits, 1);
  assert.deepEqual(effects.events, [
    { type: 'clear' },
    { type: 'write', text: 'dojo> ' },
    { type: 'write', text: 'ready' },
    { type: 'write', text: '\r\n' },
  ]);
});

test('BrowserTerminalHost buffers and emits terminal events', () => {
  const emitted = [];
  const host = new BrowserTerminalHost({
    onTerminal: event => emitted.push(event),
  });

  const routed = host.routeEffects(terminalEffects());

  assert.equal(routed.events, 4);
  assert.equal(host.snapshot().bufferChars, 'dojo> ready\r\n'.length);
  assert.equal(host.buffer, 'dojo> ready\r\n');
  assert.deepEqual(emitted.map(event => event.type), ['clear', 'write', 'write', 'write']);
});
